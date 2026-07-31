#include "sharedData.h"

#include <cmath>

RTC_DATA_ATTR uint32_t wakeCounter = 0;
RTC_DATA_ATTR uint32_t lastFixedUnix = 0;
RTC_DATA_ATTR float previousBatteryPercent = NAN;

EventGroupHandle_t lifecycleEvents = nullptr;
QueueHandle_t measurementQueue = nullptr;
QueueHandle_t gnssReadyQueue = nullptr;
QueueHandle_t gnssFreeQueue = nullptr;
QueueHandle_t heartbeatQueue = nullptr;
SemaphoreHandle_t stateMutex = nullptr;
SemaphoreHandle_t i2cMutex = nullptr;
SemaphoreHandle_t sdMutex = nullptr;

namespace {
ClockSnapshot clockState{0, 0, 0, 0, false};
BatterySnapshot batteryState{NAN, NAN, false};
SurveyMode surveyMode = SurveyMode::Unknown;
uint32_t readTimeSeconds = HI_READ;
uint16_t alignmentMinutes = HI_ALLIGN;
GnssBuffer gnssBuffers[GNSS_BUFFER_COUNT];

template <typename Function>
bool withStateLock(Function function) {
  if (stateMutex == nullptr ||
      xSemaphoreTake(stateMutex, pdMS_TO_TICKS(STATE_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }
  function();
  xSemaphoreGive(stateMutex);
  return true;
}
}  // namespace

bool sharedDataBegin() {
  lifecycleEvents = xEventGroupCreate();
  measurementQueue = xQueueCreate(MEASUREMENT_QUEUE_LENGTH,
                                  sizeof(MeasurementRecord));
  gnssReadyQueue = xQueueCreate(GNSS_BUFFER_COUNT, sizeof(GnssBuffer *));
  gnssFreeQueue = xQueueCreate(GNSS_BUFFER_COUNT, sizeof(GnssBuffer *));
  heartbeatQueue = xQueueCreate(HEARTBEAT_QUEUE_LENGTH, sizeof(Heartbeat));
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();
  sdMutex = xSemaphoreCreateMutex();

  if (!lifecycleEvents || !measurementQueue || !gnssReadyQueue ||
      !gnssFreeQueue || !heartbeatQueue || !stateMutex || !i2cMutex ||
      !sdMutex) {
    return false;
  }

  for (size_t index = 0; index < GNSS_BUFFER_COUNT; ++index) {
    GnssBuffer *buffer = &gnssBuffers[index];
    buffer->length = 0;
    if (xQueueSend(gnssFreeQueue, &buffer, 0) != pdTRUE) {
      return false;
    }
  }
  return true;
}

ClockSnapshot getClockSnapshot() {
  ClockSnapshot copy{};
  withStateLock([&] { copy = clockState; });
  return copy;
}

void setClockSnapshot(const ClockSnapshot &snapshot) {
  if (!withStateLock([&] { clockState = snapshot; })) {
    signalFatalError("shared state", "clock state mutex timeout");
    return;
  }
   Serial.printf("[Clock] unix=%lu pos=%s lat=%.7f lon=%.7f alt=%.3f\n",
      static_cast<unsigned long>(snapshot.unixTime),
      snapshot.positionValid ? "valid" : "unknown",
      snapshot.positionValid ? snapshot.latitudeE7 / 1e7 : 0.0,
      snapshot.positionValid ? snapshot.longitudeE7 / 1e7 : 0.0,
      snapshot.positionValid ? snapshot.altitudeMslMm / 1000.0 : 0.0);
}

BatterySnapshot getBatterySnapshot() {
  BatterySnapshot copy{NAN, NAN, false};
  withStateLock([&] { copy = batteryState; });
  return copy;
}

void setBatterySnapshot(const BatterySnapshot &snapshot) {
  if (!withStateLock([&] { batteryState = snapshot; })) {
    signalFatalError("shared state", "battery state mutex timeout");
  }
}

SurveyMode getSurveyMode() {
  SurveyMode copy = SurveyMode::Unknown;
  withStateLock([&] { copy = surveyMode; });
  return copy;
}

void setSurveyMode(SurveyMode mode) {
  if (!withStateLock([&] { surveyMode = mode; })) {
    signalFatalError("shared state", "survey mode mutex timeout");
  }
}

uint32_t getReadTimeSeconds() {
  uint32_t copy = HI_READ;
  withStateLock([&] { copy = readTimeSeconds; });
  return copy;
}

void setReadTimeSeconds(uint32_t seconds) {
  withStateLock([&] { readTimeSeconds = seconds; });
}

uint16_t getAlignmentMinutes() {
  uint16_t copy = HI_ALLIGN;
  withStateLock([&] { copy = alignmentMinutes; });
  return copy;
}

void setAlignmentMinutes(uint16_t minutes) {
  withStateLock([&] { alignmentMinutes = minutes; });
}

void reportHeartbeat(TaskId task) {
  if (!heartbeatQueue) {
    return;
  }
  Heartbeat heartbeat{task, xTaskGetTickCount()};
  xQueueSend(heartbeatQueue, &heartbeat, 0);
}

void signalFatalError(const char *subsystem, const char *message) {
  Serial.printf("[FATAL][%s] %s\n", subsystem, message);
  if (lifecycleEvents) {
    xEventGroupSetBits(lifecycleEvents, EVENT_FATAL_ERROR |
                                       EVENT_SHUTDOWN_REQUEST);
  }
}
