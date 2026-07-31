#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_Qwiic_XM125_Arduino_Library.h"

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
  constexpr uint32_t minimumRangeMm = 1000;
  constexpr uint32_t maximumRangeMm = 13000;
  bool initialized = false;

  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      initialized =
          radar.begin(address, Wire) == 1 &&
          radar.distanceSetup(minimumRangeMm, maximumRangeMm) == 0;
      if (initialized) {
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
    signalFatalError("radar", "initialization failed");
    xEventGroupSetBits(lifecycleEvents, EVENT_RADAR_STOPPED);
    vTaskSuspend(nullptr);
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
            if (radar.getPeakDistance(index, distanceMm) == ksfTkErrOk &&
                distanceMm > furthestMm) {
              furthestMm = distanceMm;
            }
          }
        }
      }
      xSemaphoreGive(i2cMutex);
    }

    if (setupResult == 0 && furthestMm > 0) {
      const ClockSnapshot clock = getClockSnapshot();
      const BatterySnapshot battery = getBatterySnapshot();
      MeasurementRecord record{
          clock.unixTime,
          static_cast<int32_t>(furthestMm),
          battery.voltage,
          battery.percent
      };
      if (xQueueSend(measurementQueue, &record, pdMS_TO_TICKS(100)) !=
          pdTRUE) {
        Serial.println("[Radar] Measurement queue full; sample dropped");
      }
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
