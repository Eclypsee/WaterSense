#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_MAX1704X.h"

#include "sharedData.h"
#include "taskVoltage.h"

void taskVoltage(void *) {
  Adafruit_MAX17048 fuelGauge;
  bool initialized = false;
  uint8_t invalidReadCount = 0;
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
        vTaskDelay(pdMS_TO_TICKS(500));
      }
      if(initialized){
        invalidReadCount = 0;
      }else {
        Serial.printf("[Voltage] MAX17048 unavailable; retrying later\n");
        nextInitializationAttempt = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
      }
    }

    if (initialized) {
      float voltage = NAN;
      float percent = NAN;
      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        voltage = fuelGauge.cellVoltage();
        percent = fuelGauge.cellPercent();
        xSemaphoreGive(i2cMutex);
      }
      const bool valid = !isnan(voltage) && !isnan(percent) && voltage >= 2.0f && voltage <= 5.0f && percent >= 1.0f && percent <= 100.0f;
      if(valid){
        setBatterySnapshot({voltage, percent, valid});
        xEventGroupSetBits(lifecycleEvents, EVENT_VOLTAGE_READY);
        invalidReadCount=0;
      }else{
        ++invalidReadCount;
        Serial.printf("[Voltage] Invalid reading %u/3: %.3f V, %.1f%%\n", invalidReadCount, voltage, percent);
        if(invalidReadCount>=HARDWARE_RETRY_COUNT){
          initialized=false;
          invalidReadCount=0;
          setBatterySnapshot({voltage, percent, valid});//set it anyways. an invalid reading
          nextInitializationAttempt = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
        }
      }
    }

    reportHeartbeat(TaskId::Voltage);
    vTaskDelay(pdMS_TO_TICKS(VOLTAGE_PERIOD));
  }
}
