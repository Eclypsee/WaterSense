#include <Arduino.h>
#include <Wire.h>
#include "driver/gpio.h"

#include "setup.h"
#include "sharedData.h"
#include "waterSenseTasks/taskBluetooth/taskBluetooth.h"
#include "waterSenseTasks/taskClockGNSS2/taskClockGNSS2.h"
#include "waterSenseTasks/taskRadar/taskRadar.h"
#include "waterSenseTasks/taskSD/taskSD.h"
#include "waterSenseTasks/taskSleep/taskSleep.h"
#include "waterSenseTasks/taskVoltage/taskVoltage.h"
#include "waterSenseTasks/taskWatch/taskWatch.h"

namespace {
bool createTask(TaskFunction_t function, const char *name,
                uint32_t stackDepth, UBaseType_t priority) {
  if (xTaskCreate(function, name, stackDepth, nullptr, priority, nullptr) ==
      pdPASS) {
    return true;
  }
  Serial.printf("[FATAL] Unable to create task: %s\n", name);
  return false;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialDeadline = millis() + 2000;
  while (!Serial && static_cast<int32_t>(serialDeadline - millis()) > 0) {
    delay(10);
  }

  const gpio_num_t gnssPin = static_cast<gpio_num_t>(GNSS_EN_PIN);
  const gpio_num_t radarPin = static_cast<gpio_num_t>(RADAR_WAKE_PIN);
  gpio_set_direction(gnssPin, GPIO_MODE_OUTPUT);
  gpio_set_level(gnssPin, 1);
  gpio_set_direction(radarPin, GPIO_MODE_OUTPUT);
  gpio_set_level(radarPin, 1);
  ESP_ERROR_CHECK(gpio_hold_dis(gnssPin));
  ESP_ERROR_CHECK(gpio_hold_dis(radarPin));
  gpio_deep_sleep_hold_dis();

  
  if (!sharedDataBegin()) {
    Serial.println("[FATAL] Unable to allocate FreeRTOS synchronization objects");
    delay(1000);
    esp_restart();
  }

  setReadTimeSeconds(HI_READ);
  setAlignmentMinutes(HI_ALLIGN);
  Wire.begin(SDA, SCL, CLK);

  #ifdef DEBUG_I2C_SCAN
  for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
          Serial.printf("Found device at 0x%02X\n", addr);
      }
  }
  #endif
#ifndef BLE_on
  xEventGroupSetBits(lifecycleEvents, EVENT_BLUETOOTH_STOPPED);
#endif

  bool tasksCreated = true;
  tasksCreated &= createTask(taskWatch, "watchdog", 4096, 5);
  tasksCreated &= createTask(taskClockGNSS2, "clock_gnss", 8192, 4);
  tasksCreated &= createTask(taskSD, "storage", 8192, 3);
  tasksCreated &= createTask(taskRadar, "radar", 6144, 2);
  tasksCreated &= createTask(taskVoltage, "voltage", 4096, 1);
  tasksCreated &= createTask(taskSleep, "power", 4096, 1);

#ifdef BLE_on
  tasksCreated &= createTask(taskBluetooth, "bluetooth", 8192, 2);
#endif

  if (!tasksCreated) {
    signalFatalError("startup", "one or more tasks could not be created");
  }
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
