#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_MAX1704X.h"

#include "sharedData.h"
#include "taskVoltage.h"

void taskVoltage(void *) {
  Adafruit_MAX17048 fuelGauge;
  bool initialized = false;
  TickType_t nextInitializationAttempt = 0;

  for (;;) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST) {
      xEventGroupSetBits(lifecycleEvents, EVENT_VOLTAGE_STOPPED);
      reportHeartbeat(TaskId::Voltage);
      vTaskSuspend(nullptr);
    }

    if (!initialized && xTaskGetTickCount() >= nextInitializationAttempt) {
      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
          pdTRUE) {
        initialized = fuelGauge.begin(&Wire);
        xSemaphoreGive(i2cMutex);
      }
      if (!initialized) {
        Serial.println("[Voltage] MAX17048 unavailable; retrying later");
        nextInitializationAttempt =
            xTaskGetTickCount() + pdMS_TO_TICKS(30000);
      }
    }

    if (initialized) {
      float voltage = NAN;
      float percent = NAN;
      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
          pdTRUE) {
        voltage = fuelGauge.cellVoltage();
        percent = fuelGauge.cellPercent();
        xSemaphoreGive(i2cMutex);
      }
      const bool valid = !isnan(voltage) && !isnan(percent);
      setBatterySnapshot({voltage, percent, valid});
      if (!valid) {
        initialized = false;
      }
    }

    reportHeartbeat(TaskId::Voltage);
    vTaskDelay(pdMS_TO_TICKS(VOLTAGE_PERIOD));
  }
}
