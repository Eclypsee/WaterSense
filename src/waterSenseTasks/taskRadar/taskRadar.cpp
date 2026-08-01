#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_Qwiic_XM125_Arduino_Library.h"
#include <algorithm>

#include "sharedData.h"
#include "taskRadar.h"

void taskRadar(void *) {
  const EventBits_t startupBits = EVENT_CLOCK_READY | EVENT_STORAGE_READY;
  while ((xEventGroupGetBits(lifecycleEvents) & startupBits) != startupBits) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST) {
      xEventGroupSetBits(lifecycleEvents, EVENT_RADAR_STOPPED);
      vTaskSuspend(nullptr);
    }
    reportHeartbeat(TaskId::Radar);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  SparkFunXM125Distance radar;
  constexpr uint8_t address = SFE_XM125_I2C_ADDRESS;
  bool initialized = false;

  //getting the median of MAX_SAMPLES values
  uint32_t samples[MAX_SAMPLES];
  uint8_t sampleCount = 0;
  TickType_t lastPublish = xTaskGetTickCount();

  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE) {
      bool radarFound = radar.begin(address, Wire);
      if(radarFound) Serial.printf("[RADAR] Found on attempt %u\n", attempt + 1);
      initialized =
          radarFound &&
          radar.distanceSetup(minimumRangeMm, maximumRangeMm) == 0;
      if (initialized) {
        Serial.printf("[RADAR] Initialized on attempt %u\n", attempt + 1);
        radar.setCloseRangeLeakageCancellation(true);
      }
      xSemaphoreGive(i2cMutex);
    }
    if (initialized) {
      break;
    }
    reportHeartbeat(TaskId::Radar);
    vTaskDelay(pdMS_TO_TICKS(HARDWARE_RETRY_DELAY_MS));
  }

  if (!initialized) {
    #ifndef DEBUG_DISABLE_RADAR
      signalFatalError("radar", "initialization failed");
      xEventGroupSetBits(lifecycleEvents, EVENT_RADAR_STOPPED);
      vTaskSuspend(nullptr);
    #endif
    #ifdef DEBUG_DISABLE_RADAR
      Serial.println("[Radar] unavailable; continuing without radar");
      xEventGroupSetBits(lifecycleEvents, EVENT_RADAR_STOPPED);
      reportHeartbeat(TaskId::Radar);
      vTaskSuspend(nullptr);
    #endif
  }

  while (!(xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST)) {
    int32_t setupResult = -1;
    uint32_t furthestMm = 0;

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      setupResult = radar.detectorReadingSetup();
      if (setupResult == 0) {
        radar.busyWait();
        uint32_t count = 0;
        if (radar.getNumberDistances(count) == ksfTkErrOk) {
          for (uint32_t index = 0; index < count; ++index) {
            uint32_t distanceMm = 0;
            if (radar.getPeakDistance(index, distanceMm) == ksfTkErrOk && distanceMm >= minimumRangeMm && distanceMm <= maximumRangeMm && distanceMm > furthestMm) {
              furthestMm = distanceMm;
            }
          }
        }
      }
      xSemaphoreGive(i2cMutex);
    }

    if (setupResult == 0 && furthestMm > 0 && sampleCount < MAX_SAMPLES) {
      samples[sampleCount++] = furthestMm;
    }
    if (xTaskGetTickCount() - lastPublish >= pdMS_TO_TICKS(1000)) {
      if (sampleCount > 0) {
        std::sort(samples, samples + sampleCount);
        uint32_t median;
        if (sampleCount & 1) {
          median = samples[sampleCount / 2];
        } else {
          median = (samples[sampleCount / 2 - 1] + samples[sampleCount / 2]) / 2;
        }
        const ClockSnapshot clock = getClockSnapshot();
        const BatterySnapshot battery = getBatterySnapshot();
        MeasurementRecord record{
            clock.unixTime,
            static_cast<int32_t>(median),
            battery.voltage,
            battery.percent
        };
        if (xQueueSend(measurementQueue, &record, pdMS_TO_TICKS(100)) != pdTRUE) {
          Serial.println("[Radar] Measurement queue full; sample dropped");
        }
        Serial.printf("[Radar] Publishing median value: %u mm\n", median);
        sampleCount = 0;
      }
      lastPublish += pdMS_TO_TICKS(1000);
    }

    reportHeartbeat(TaskId::Radar);
    vTaskDelay(pdMS_TO_TICKS(RADAR_TASK_PERIOD));
  }

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
      pdTRUE) {
    radar.stop();
    xSemaphoreGive(i2cMutex);
  }
  xEventGroupSetBits(lifecycleEvents, EVENT_RADAR_STOPPED);
  reportHeartbeat(TaskId::Radar);
  vTaskSuspend(nullptr);
}
