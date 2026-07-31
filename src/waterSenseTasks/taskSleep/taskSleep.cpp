#include <Arduino.h>

#include "sharedData.h"
#include "taskSleep.h"

namespace {
void waitWithHeartbeat(uint32_t durationSeconds) {
  const TickType_t deadline =
      xTaskGetTickCount() + pdMS_TO_TICKS(durationSeconds * 1000UL);
  while (static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_FATAL_ERROR) {
      return;
    }
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
}  // namespace

void taskSleep(void *) {
  const EventBits_t startupBits = EVENT_CLOCK_READY | EVENT_STORAGE_READY;
  while ((xEventGroupGetBits(lifecycleEvents) & startupBits) != startupBits) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_FATAL_ERROR) {
      xEventGroupSetBits(lifecycleEvents, EVENT_SHUTDOWN_REQUEST);
      break;
    }
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ++wakeCounter;
  const SurveyMode mode = getSurveyMode();
  const uint32_t activeSeconds =
      mode == SurveyMode::GnssRaw ? GNSS_READ_TIME : getReadTimeSeconds();

#ifdef CONTINUOUS
  Serial.println("[Power] Continuous mode: deep sleep disabled");
  for (;;) {
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
#else
  waitWithHeartbeat(activeSeconds);
  xEventGroupSetBits(lifecycleEvents, EVENT_SHUTDOWN_REQUEST);

  const TickType_t shutdownDeadline =
      xTaskGetTickCount() + pdMS_TO_TICKS(SHUTDOWN_TIMEOUT_MS);
  while ((xEventGroupGetBits(lifecycleEvents) & EVENT_ALL_STOPPED) !=
         EVENT_ALL_STOPPED) {
    if (static_cast<int32_t>(shutdownDeadline - xTaskGetTickCount()) <= 0) {
      Serial.printf("[Power] Shutdown timed out; event bits: 0x%08lx\n",
                    static_cast<unsigned long>(
                        xEventGroupGetBits(lifecycleEvents)));
      vTaskDelay(pdMS_TO_TICKS(100));
      esp_restart();
    }
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  const BatterySnapshot battery = getBatterySnapshot();
  if (battery.valid) {
    previousBatteryPercent = battery.percent;
  }

  const uint64_t requestedSleepUs =
      static_cast<uint64_t>(getReadTimeSeconds()) * 1000000ULL;
  const uint64_t alignmentCapUs =
      static_cast<uint64_t>(getAlignmentMinutes()) * 60ULL * 1000000ULL;
  const uint64_t sleepUs =
      requestedSleepUs > alignmentCapUs ? alignmentCapUs : requestedSleepUs;

  Serial.printf("[Power] Sleeping for %llu seconds\n",
                static_cast<unsigned long long>(sleepUs / 1000000ULL));
  esp_sleep_enable_timer_wakeup(sleepUs);
  Serial.flush();
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_deep_sleep_start();
#endif
}
