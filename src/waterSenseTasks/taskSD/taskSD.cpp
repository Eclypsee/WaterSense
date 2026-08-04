#include <Arduino.h>

#include "sharedData.h"
#include "taskSD.h"
#include "waterSenseLibs/sdData/sdData.h"

namespace {
bool lockSd() {
  return xSemaphoreTake(sdMutex, pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS)) ==
         pdTRUE;
}
}  // namespace

void taskSD(void *) {
  while (!(xEventGroupGetBits(lifecycleEvents) & EVENT_CLOCK_READY)) {
    reportHeartbeat(TaskId::Storage);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  SD_Data storage(SD_CS, SD_SCK, SD_MISO, SD_MOSI);
  ExFile measurementFile;
  ExFile gnssFile;

  if (!lockSd()) {
    signalFatalError("storage", "SD mutex unavailable during initialization");
    xEventGroupSetBits(lifecycleEvents, EVENT_STORAGE_STOPPED);
    vTaskSuspend(nullptr);
  }

  const bool initialized = storage.begin();
  bool filesReady = false;
  if (initialized) {
    filesReady = storage.writeHeader();
  }
  xSemaphoreGive(sdMutex);

  #ifndef DEBUG_DISABLE_SD
    if (!initialized || !filesReady) {
      signalFatalError("storage", "SD initialization or file creation failed");
      xEventGroupSetBits(lifecycleEvents, EVENT_STORAGE_STOPPED);
      vTaskSuspend(nullptr);
    }
  #endif
  #ifdef DEBUG_DISABLE_SD
    if (!initialized || !filesReady) {
      Serial.println("[SD] SD unavailable; continuing without storage");
      xEventGroupSetBits(lifecycleEvents,EVENT_STORAGE_READY | EVENT_STORAGE_STOPPED);
      reportHeartbeat(TaskId::Storage);
      vTaskSuspend(nullptr);
    }
  #endif

  xEventGroupSetBits(lifecycleEvents, EVENT_STORAGE_READY);

  for (;;) {
    MeasurementRecord record{};
    if (xQueueReceive(measurementQueue, &record, pdMS_TO_TICKS(SD_PERIOD)) == pdTRUE) {
      if (record.unixTime < MIN_VALID_UNIX_TIME) {
        Serial.println("[SD] Dropping measurement with invalid timestamp");
      }else if (lockSd()) {
        bool fileReady = static_cast<bool>(measurementFile);
        if (!fileReady || measurementFile.fileSize() >= MAX_FILESIZE) {
          fileReady = storage.createDataFile(measurementFile, record.unixTime); }
        if (!fileReady) {
          signalFatalError("storage","measurement file creation failed");
        } else if (!storage.writeMeasurement(measurementFile, record)) {
          signalFatalError("storage", "measurement write failed");
        }
        xSemaphoreGive(sdMutex);
      } else {
        xQueueSendToFront(measurementQueue, &record, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

    GnssBuffer *buffer = nullptr;
    while (xQueueReceive(gnssReadyQueue, &buffer, 0) == pdTRUE) {
    const ClockSnapshot clock = getClockSnapshot();
    if (clock.unixTime < MIN_VALID_UNIX_TIME) {
      Serial.println("[SD] Dropping GNSS data with invalid timestamp");
      buffer->length = 0;
      xQueueSend(gnssFreeQueue, &buffer, portMAX_DELAY);
      continue;
    }
    if (!lockSd()) {
      xQueueSendToFront(gnssReadyQueue, &buffer, 0);
      break;
    }
    bool fileReady = static_cast<bool>(gnssFile);
    if (!fileReady || gnssFile.fileSize() + static_cast<uint64_t>(buffer->length) >= MAX_FILESIZE) {
      fileReady = storage.createGnssFile(gnssFile, clock.unixTime);
    }
    if (!fileReady) {
      signalFatalError("storage", "GNSS file creation failed");
    } else if (!storage.writeGnssData(gnssFile, buffer->data, buffer->length)) {
      signalFatalError("storage", "GNSS write failed");
    }
    xSemaphoreGive(sdMutex);
    buffer->length = 0;
    xQueueSend(gnssFreeQueue, &buffer, portMAX_DELAY);
  }

    const EventBits_t bits = xEventGroupGetBits(lifecycleEvents);
    const bool producersDone =
        (bits & (EVENT_GNSS_DONE | EVENT_RADAR_STOPPED)) ==
        (EVENT_GNSS_DONE | EVENT_RADAR_STOPPED);
    const bool shutdownRequested = bits & EVENT_SHUTDOWN_REQUEST;
    if (shutdownRequested && producersDone &&
        uxQueueMessagesWaiting(gnssReadyQueue) == 0 &&
        uxQueueMessagesWaiting(measurementQueue) == 0) {
      if (lockSd()) {
        const ClockSnapshot clock = getClockSnapshot();
        if (clock.positionValid && clock.unixTime > MIN_VALID_UNIX_TIME) {
          storage.writeLog(clock);
        }
        if(measurementFile)storage.close(measurementFile);
        if(gnssFile)storage.close(gnssFile);
        xSemaphoreGive(sdMutex);
      }
      xEventGroupSetBits(lifecycleEvents, EVENT_STORAGE_STOPPED);
      reportHeartbeat(TaskId::Storage);
      vTaskSuspend(nullptr);
    }

    reportHeartbeat(TaskId::Storage);
  }
}
