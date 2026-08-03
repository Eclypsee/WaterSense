#include <Arduino.h>
#include "driver/gpio.h"
#include <time.h>

#include "sharedData.h"
#include "taskSleep.h"

namespace {
bool waitWithHeartbeat(uint32_t durationSeconds) {
  const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(durationSeconds * 1000UL);
  while (static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
    if (xEventGroupGetBits(lifecycleEvents) & EVENT_FATAL_ERROR) {
      return false;
    }
    #ifndef DEBUG_NO_BATTERY
    const BatterySnapshot battery = getBatterySnapshot();
    const BatteryHistory hist = getBatteryHistory();
    const time_t now = time(nullptr);
    if (battery.valid && isfinite(battery.percent) && battery.percent <= LOW_BATTERY_PERCENT) {
      Serial.printf("[Power] Low battery: %.1f%%; shutting down early\n", battery.percent);
      return false;
    }
    const bool historyRecent =
        !battery.valid &&
        isfinite(hist.percent) &&
        hist.unixTime >= 1700000000UL &&
        now >= static_cast<time_t>(hist.unixTime) &&
        static_cast<uint64_t>(now - static_cast<time_t>(hist.unixTime)) <= 2ULL;
    if (historyRecent && hist.percent <= LOW_BATTERY_PERCENT) {
      Serial.printf("[Power] Low battery fallback: %.1f%%; shutting down early\n", hist.percent);
      return false;
    }
    #endif
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return true;
}

uint64_t getAlignedSleepUs() {
  const time_t now = time(nullptr);
  const uint32_t intervalSeconds = SLEEP_ALIGN_MIN  * 60UL;
  static_assert(intervalSeconds > 0, "SLEEP_ALIGN_MIN must be greater than zero");
  if (now < 1700000000) {
    Serial.println("[Power] Clock invalid; using alignment interval for sleep");
    return static_cast<uint64_t>(intervalSeconds) * 1000000ULL;
  }
  const uint32_t sleepSec = intervalSeconds - (static_cast<uint64_t>(now) % intervalSeconds);

  return static_cast<uint64_t>(sleepSec) * 1000000ULL;
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
  const uint32_t activeSeconds = mode == SurveyMode::GnssRaw ? GNSS_READ_TIME : READ_TIME_S;

#ifdef CONTINUOUS
  Serial.println("[Power] Continuous mode: deep sleep disabled");
  for (;;) {
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
#else
  const bool completedActiveWindow = waitWithHeartbeat(activeSeconds);
  if (!completedActiveWindow) Serial.println("[Power] Active window ended early");
  xEventGroupSetBits(lifecycleEvents, EVENT_SHUTDOWN_REQUEST);

  const TickType_t shutdownDeadline = xTaskGetTickCount() + pdMS_TO_TICKS(SHUTDOWN_TIMEOUT_MS);
  while ((xEventGroupGetBits(lifecycleEvents) & EVENT_ALL_STOPPED) != EVENT_ALL_STOPPED) {
    if (static_cast<int32_t>(shutdownDeadline - xTaskGetTickCount()) <= 0) {
      Serial.printf("[Power] Shutdown timed out; event bits: 0x%08lx\n", static_cast<unsigned long>(xEventGroupGetBits(lifecycleEvents)));
      vTaskDelay(pdMS_TO_TICKS(100));
      esp_restart();
    }
    reportHeartbeat(TaskId::Sleep);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  const uint64_t sleepUs = getAlignedSleepUs();

  const gpio_num_t gnssPin = static_cast<gpio_num_t>(GNSS_EN_PIN);
  const gpio_num_t radarPin = static_cast<gpio_num_t>(RADAR_WAKE_PIN);
  gpio_set_direction(gnssPin, GPIO_MODE_OUTPUT);
  gpio_set_level(gnssPin, 0);
  gpio_set_direction(radarPin, GPIO_MODE_OUTPUT);
  gpio_set_level(radarPin, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_ERROR_CHECK(gpio_hold_en(gnssPin));
  ESP_ERROR_CHECK(gpio_hold_en(radarPin));
  gpio_deep_sleep_hold_en();

  Serial.printf("[Power] Sleeping for %llu seconds\n",
                static_cast<unsigned long long>(sleepUs / 1000000ULL));
  esp_sleep_enable_timer_wakeup(sleepUs);
  Serial.flush();
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_deep_sleep_start();
#endif
}
