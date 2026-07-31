#include "bluetooth.h"

#include <cstring>

#include "sharedData.h"
#include "waterSenseLibs/sdData/sdData.h"

BluetoothFileManager bluetoothFileManager;

namespace {
bool takeSdMutex() {
  return xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) ==
         pdTRUE;
}
}  // namespace

bool BluetoothFileManager::begin() {
  return (xEventGroupGetBits(lifecycleEvents) & EVENT_STORAGE_READY) != 0;
}

uint32_t BluetoothFileManager::updateChecksum(uint32_t checksum,
                                              uint8_t value) const {
  return ((checksum << 1) + value) ^ (checksum >> 31);
}

bool BluetoothFileManager::loadFile(const String &fileName) {
  clearFile();
  if (!takeSdMutex()) {
    return false;
  }

  currentFile_ = SD.open(fileName.c_str(), O_RDONLY);
  if (!currentFile_ || currentFile_.isDirectory()) {
    if (currentFile_) {
      currentFile_.close();
    }
    xSemaphoreGive(sdMutex);
    return false;
  }

  currentFileSize_ = currentFile_.fileSize();
  if (currentFileSize_ > BT_TRANSF_SIZE) {
    currentFile_.close();
    currentFileSize_ = 0;
    xSemaphoreGive(sdMutex);
    return false;
  }

  uint8_t checksumBuffer[128];
  currentChecksum_ = 0;
  size_t remaining = currentFileSize_;
  while (remaining > 0) {
    const size_t requested =
        remaining > sizeof(checksumBuffer) ? sizeof(checksumBuffer) : remaining;
    const int read = currentFile_.read(checksumBuffer, requested);
    if (read <= 0) {
      currentFile_.close();
      currentFileSize_ = 0;
      xSemaphoreGive(sdMutex);
      return false;
    }
    for (int index = 0; index < read; ++index) {
      currentChecksum_ = updateChecksum(currentChecksum_,
                                        checksumBuffer[index]);
    }
    remaining -= static_cast<size_t>(read);
  }
  currentFile_.seekSet(0);
  currentFileName_ = fileName;
  currentOffset_ = 0;
  xSemaphoreGive(sdMutex);
  return true;
}

size_t BluetoothFileManager::readChunk(uint8_t *destination,
                                       size_t capacity) {
  if (!currentFile_ || !destination || capacity == 0 ||
      currentOffset_ >= currentFileSize_ || !takeSdMutex()) {
    return 0;
  }

  const size_t remaining = currentFileSize_ - currentOffset_;
  const size_t requested = remaining < capacity ? remaining : capacity;
  const int bytesRead = currentFile_.read(destination, requested);
  xSemaphoreGive(sdMutex);
  if (bytesRead <= 0) {
    return 0;
  }
  currentOffset_ += static_cast<size_t>(bytesRead);
  return static_cast<size_t>(bytesRead);
}

bool BluetoothFileManager::transferFinished() const {
  return currentFile_.isOpen() && currentOffset_ >= currentFileSize_;
}

uint32_t BluetoothFileManager::getCurrentChecksum() const {
  return currentChecksum_;
}

size_t BluetoothFileManager::getCurrentFileSize() const {
  return currentFileSize_;
}

void BluetoothFileManager::clearFile() {
  if (currentFile_ && takeSdMutex()) {
    currentFile_.close();
    xSemaphoreGive(sdMutex);
  }
  currentFileName_ = "";
  currentFileSize_ = 0;
  currentOffset_ = 0;
  currentChecksum_ = 0;
}

bool BluetoothFileManager::generateFileList() {
  if (!takeSdMutex()) {
    return false;
  }

  // Remove only files owned by this feature.
  for (uint16_t index = 1; index <= 999; ++index) {
    const String path = "/filelist" + String(index) + ".txt";
    if (!SD.exists(path.c_str())) {
      if (index > fileListCount_ + 1) {
        break;
      }
      continue;
    }
    SD.remove(path.c_str());
  }

  fileListCount_ = 1;
  ExFile list = SD.open("/filelist1.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (!list) {
    xSemaphoreGive(sdMutex);
    return false;
  }

  auto rotateIfNeeded = [&]() -> bool {
    if (list.fileSize() < MAX_FILESIZE) {
      return true;
    }
    list.close();
    ++fileListCount_;
    const String path = "/filelist" + String(fileListCount_) + ".txt";
    list = SD.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    return static_cast<bool>(list);
  };

  const char *directories[] = {"/Data", "/GNSS_Data", "/"};
  char name[96];
  bool ok = true;
  for (const char *directoryName : directories) {
    ExFile directory = SD.open(directoryName, O_RDONLY);
    if (!directory || !directory.isDirectory()) {
      continue;
    }
    ExFile entry;
    while (entry.openNext(&directory, O_RDONLY)) {
      entry.getName(name, sizeof(name));
      const bool isRoot = strcmp(directoryName, "/") == 0;
      const bool ownedFileList = strncmp(name, "filelist", 8) == 0;
      if (!entry.isDirectory() && !ownedFileList) {
        if (isRoot) {
          list.printf("/%s\n", name);
        } else {
          list.printf("%s/%s\n", directoryName, name);
        }
        if (!rotateIfNeeded()) {
          ok = false;
          entry.close();
          break;
        }
      }
      entry.close();
      reportHeartbeat(TaskId::Bluetooth);
    }
    directory.close();
    if (!ok) {
      break;
    }
  }

  list.sync();
  list.close();
  xSemaphoreGive(sdMutex);
  return ok;
}

uint16_t BluetoothFileManager::getFileListCount() const {
  return fileListCount_;
}
