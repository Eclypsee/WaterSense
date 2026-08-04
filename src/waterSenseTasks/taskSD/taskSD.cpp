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
    const ClockSnapshot clock = getClockSnapshot();
    filesReady = storage.writeHeader() &&
                 storage.createDataFile(measurementFile, clock.unixTime);
    if (filesReady && getSurveyMode() == SurveyMode::GnssRaw) {
      filesReady = storage.createGnssFile(gnssFile, clock.unixTime);
    }
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
    if (xQueueReceive(measurementQueue, &record, pdMS_TO_TICKS(SD_PERIOD)) ==
        pdTRUE) {
      if (lockSd()) {
        if (measurementFile.fileSize() >= MAX_FILESIZE) {
          storage.createDataFile(measurementFile, record.unixTime);
        }
        if (!storage.writeMeasurement(measurementFile, record)) {
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
      if (!lockSd()) {
        xQueueSendToFront(gnssReadyQueue, &buffer, 0);
        break;
      }
      if (!gnssFile) {
        storage.createGnssFile(gnssFile, getClockSnapshot().unixTime);
      } else if (gnssFile.fileSize() + buffer->length >= MAX_FILESIZE) {
        storage.createGnssFile(gnssFile, getClockSnapshot().unixTime);
      }
      if (!storage.writeGnssData(gnssFile, buffer->data, buffer->length)) {
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
        if (clock.positionValid) {
          storage.writeLog(clock);
        }
        storage.close(measurementFile);
        storage.close(gnssFile);
        xSemaphoreGive(sdMutex);
      }
      xEventGroupSetBits(lifecycleEvents, EVENT_STORAGE_STOPPED);
      reportHeartbeat(TaskId::Storage);
      vTaskSuspend(nullptr);
    }

    reportHeartbeat(TaskId::Storage);
  }
}
