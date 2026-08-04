#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <sys/time.h>
#include <cstdlib>

#include "sharedData.h"
#include "taskClockGNSS2.h"
#include "waterSenseLibs/zedGNSS/zedGNSS.h"

namespace {
bool setSystemUnixTime(uint32_t unixTime) {
  if (unixTime < MIN_VALID_UNIX_TIME)return false;
  timeval tv{};
  tv.tv_sec = static_cast<time_t>(unixTime);
  tv.tv_usec = 0;
  if(settimeofday(&tv, nullptr) == 0){
    return true;
  }
  Serial.println("[Clock] Failed to set system ESP32 time");
  return false;
}

bool beginRtc(RTC_DS3231 &rtc) {
  for (uint8_t attempt = 0; attempt < HARDWARE_RETRY_COUNT; ++attempt) {
    bool found = false;
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
        pdTRUE) {
      found = rtc.begin(&Wire);
      xSemaphoreGive(i2cMutex);
    }
    if (found) {
      Serial.printf("[RTC] Detected on attempt %u\n", attempt + 1);
      return true;
    }
    Serial.printf("[RTC] Detection attempt %u failed\n", attempt + 1);
    reportHeartbeat(TaskId::Clock);
    vTaskDelay(pdMS_TO_TICKS(HARDWARE_RETRY_DELAY_MS));
  }
  return false;
}

uint32_t readRtc(RTC_DS3231 &rtc) {
  uint32_t result = 0;
  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
      pdTRUE) {
    result = rtc.now().unixtime();
    xSemaphoreGive(i2cMutex);
  }
  return result;
}

bool rtcTimeIsValid(RTC_DS3231 &rtc, uint32_t unixTime) {
  bool lostPower = true;
  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) ==
      pdTRUE) {
    lostPower = rtc.lostPower();
    xSemaphoreGive(i2cMutex);
  }
  // Reject the DS3231 reset/default era as an operational timestamp.
  return !lostPower && unixTime >= MIN_VALID_UNIX_TIME;
}

bool adjustRtc(RTC_DS3231 &rtc, uint32_t unixTime) {
  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) !=
      pdTRUE) {
    return false;
  }
  rtc.adjust(DateTime(unixTime));
  xSemaphoreGive(i2cMutex);
  return true;
}
}  // namespace

void taskClockGNSS2(void *) {
  RTC_DS3231 rtc;
  GNSS gnss;
  bool rtcAvailable = beginRtc(rtc);
  uint32_t rtcUnix = rtcAvailable ? readRtc(rtc) : 0;
  bool rtcValid = rtcAvailable && rtcTimeIsValid(rtc, rtcUnix);
  if(rtcValid)setSystemUnixTime(rtcUnix);

  const bool monthElapsed = lastFixedUnix == 0 || (rtcValid && rtcUnix >= lastFixedUnix && (rtcUnix - lastFixedUnix) >= GNSS_MONTH_SECONDS);

  bool surveyRequested = false;
#ifdef GNSS_ON
  surveyRequested = wakeCounter == 0 || !rtcValid || monthElapsed;
#endif

  setSurveyMode(surveyRequested ? SurveyMode::GnssRaw : SurveyMode::Normal);
  ClockSnapshot clock{rtcValid ? rtcUnix : 0, 0, 0, 0, false};
  setClockSnapshot(clock);
  // Storage must begin consuming raw buffers while fix acquisition is in
  // progress. A zero timestamp explicitly means that no valid clock source is
  // available yet.
  xEventGroupSetBits(lifecycleEvents, EVENT_CLOCK_READY);
  bool gnssRunning = false;

  if (surveyRequested) {
    gnssRunning = gnss.begin();
    if (!gnssRunning && !rtcValid) {
      signalFatalError("clock", "neither RTC nor GNSS is available");
    }
  }else{
    gnss.beginIdle();
  }

  if (gnssRunning) {//wait for first fix
    const TickType_t fixDeadline = xTaskGetTickCount() + pdMS_TO_TICKS(FIX_DELAY * 1000UL);
    do {
      GnssFix fix{};
      gnss.poll(fix);
      gnss.drainFullBuffers();
      if (fix.valid) {
        clock = {fix.unixTime, fix.latitudeE7, fix.longitudeE7, fix.altitudeMslMm, true};
        setClockSnapshot(clock);
        lastFixedUnix = fix.unixTime;
        setSystemUnixTime(lastFixedUnix);
        if (rtcAvailable && adjustRtc(rtc, fix.unixTime)) {
          rtcUnix = fix.unixTime;
          rtcValid = true;
        }
        break;
      }
      reportHeartbeat(TaskId::Clock);
      vTaskDelay(pdMS_TO_TICKS(CLOCK_PERIOD));
    } while (static_cast<int32_t>(fixDeadline - xTaskGetTickCount()) > 0 && !(xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST));
  }

  if (clock.unixTime == 0 && rtcValid) {
    clock.unixTime = readRtc(rtc);
  }
  setClockSnapshot(clock);

  TickType_t lastRtcUpdate = 0;
  while (!(xEventGroupGetBits(lifecycleEvents) & EVENT_SHUTDOWN_REQUEST)) {
    bool fixValid = false;
    if (gnssRunning) {
      GnssFix fix{};
      gnss.poll(fix);
      gnss.drainFullBuffers();
      if (fix.valid) {
        clock = {fix.unixTime, fix.latitudeE7, fix.longitudeE7, fix.altitudeMslMm, true};
        setClockSnapshot(clock);
        fixValid = true;

        const time_t systemNow = time(nullptr);
        const int64_t error = static_cast<int64_t>(fix.unixTime) - static_cast<int64_t>(systemNow);
        if (std::llabs(error) > 2) {
          setSystemUnixTime(fix.unixTime);
        }
      }
    }else if (!rtcAvailable) {
      rtcAvailable = beginRtc(rtc);
      if (rtcAvailable) {
        rtcUnix = readRtc(rtc);
        rtcValid = rtcTimeIsValid(rtc, rtcUnix);
        if (rtcValid) {
          setSystemUnixTime(rtcUnix);
          clock.unixTime = rtcUnix;
          setClockSnapshot(clock);
        }
      }
    }
    if (!fixValid && rtcAvailable && xTaskGetTickCount() - lastRtcUpdate >= pdMS_TO_TICKS(1000)) {
      rtcUnix = readRtc(rtc);
      rtcValid = rtcTimeIsValid(rtc, rtcUnix);
      if (rtcValid) {
        clock.unixTime = rtcUnix;
        setClockSnapshot(clock);
      }
      lastRtcUpdate = xTaskGetTickCount();
    }

    reportHeartbeat(TaskId::Clock);
    vTaskDelay(pdMS_TO_TICKS(CLOCK_PERIOD));
  }

  if (gnssRunning) {
    gnss.flushBuffers();
  }
  xEventGroupSetBits(lifecycleEvents, EVENT_GNSS_DONE);

  if (gnssRunning && !gnss.shutdown()) {
    Serial.println("[GNSS] Receiver did not acknowledge power-off");
  }
  xEventGroupSetBits(lifecycleEvents, EVENT_CLOCK_STOPPED);
  reportHeartbeat(TaskId::Clock);
  vTaskSuspend(nullptr);
}
