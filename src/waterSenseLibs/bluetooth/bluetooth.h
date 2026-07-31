#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>
#include <SdFat.h>

class BluetoothFileManager {
 public:
  bool begin();
  bool loadFile(const String &fileName);
  size_t readChunk(uint8_t *destination, size_t capacity);
  bool transferFinished() const;
  uint32_t getCurrentChecksum() const;
  size_t getCurrentFileSize() const;
  void clearFile();
  bool generateFileList();
  uint16_t getFileListCount() const;

 private:
  uint32_t updateChecksum(uint32_t checksum, uint8_t value) const;

  ExFile currentFile_;
  String currentFileName_;
  size_t currentFileSize_ = 0;
  size_t currentOffset_ = 0;
  uint32_t currentChecksum_ = 0;
  uint16_t fileListCount_ = 0;
};

extern BluetoothFileManager bluetoothFileManager;

#endif
