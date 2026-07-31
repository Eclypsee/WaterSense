#include <Arduino.h>

#include "sharedData.h"
#include "taskWatch.h"

namespace {
bool taskHasStopped(TaskId task, EventBits_t bits) {
  switch (task) {
    case TaskId::Clock:
      return bits & EVENT_CLOCK_STOPPED;
    case TaskId::Storage:
      return bits & EVENT_STORAGE_STOPPED;
    case TaskId::Radar:
      return bits & EVENT_RADAR_STOPPED;
    case TaskId::Voltage:
      return bits & EVENT_VOLTAGE_STOPPED;
    case TaskId::Bluetooth:
      return bits & EVENT_BLUETOOTH_STOPPED;
    default:
      return false;
  }
}

bool taskIsExpected(TaskId task) {
#ifndef BLE_on
  if (task == TaskId::Bluetooth) {
    return false;
  }
#endif
  return task != TaskId::Count;
}
}  // namespace

void taskWatch(void *) {
  TickType_t lastSeen[static_cast<size_t>(TaskId::Count)];
  const TickType_t started = xTaskGetTickCount();
  for (TickType_t &tick : lastSeen) {
    tick = started;
  }

  for (;;) {
    Heartbeat heartbeat{};
    if (xQueueReceive(heartbeatQueue, &heartbeat,
                      pdMS_TO_TICKS(WATCHDOG_PERIOD)) == pdTRUE) {
      lastSeen[static_cast<size_t>(heartbeat.task)] = heartbeat.tick;
      while (xQueueReceive(heartbeatQueue, &heartbeat, 0) == pdTRUE) {
        lastSeen[static_cast<size_t>(heartbeat.task)] = heartbeat.tick;
      }
    }

    const TickType_t now = xTaskGetTickCount();
    const EventBits_t bits = xEventGroupGetBits(lifecycleEvents);
    for (size_t index = 0; index < static_cast<size_t>(TaskId::Count);
         ++index) {
      const TaskId task = static_cast<TaskId>(index);
      if (!taskIsExpected(task) || taskHasStopped(task, bits)) {
        continue;
      }
      if (now - lastSeen[index] > pdMS_TO_TICKS(WATCH_TIMER)) {
        Serial.printf("[Watchdog] Task %u missed its heartbeat\n",
                      static_cast<unsigned>(index));
        Serial.flush();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
      }
    }
  }
}
