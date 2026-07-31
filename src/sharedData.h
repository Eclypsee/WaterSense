#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "setup.h"

// Lifecycle bits. A task sets its STOPPED bit only after releasing hardware and
// publishing all pending data.
constexpr EventBits_t EVENT_CLOCK_READY       = BIT0;
constexpr EventBits_t EVENT_STORAGE_READY     = BIT1;
constexpr EventBits_t EVENT_SHUTDOWN_REQUEST  = BIT2;
constexpr EventBits_t EVENT_CLOCK_STOPPED     = BIT3;
constexpr EventBits_t EVENT_STORAGE_STOPPED   = BIT4;
constexpr EventBits_t EVENT_RADAR_STOPPED     = BIT5;
constexpr EventBits_t EVENT_BLUETOOTH_STOPPED = BIT6;
constexpr EventBits_t EVENT_GNSS_DONE         = BIT7;
constexpr EventBits_t EVENT_BLE_CONNECTED     = BIT8;
constexpr EventBits_t EVENT_FATAL_ERROR       = BIT9;
constexpr EventBits_t EVENT_VOLTAGE_STOPPED   = BIT10;

constexpr EventBits_t EVENT_ALL_STOPPED =
    EVENT_CLOCK_STOPPED | EVENT_STORAGE_STOPPED |
    EVENT_RADAR_STOPPED | EVENT_BLUETOOTH_STOPPED |
    EVENT_VOLTAGE_STOPPED;

enum class SurveyMode : uint8_t {
  Unknown,
  Normal,
  GnssRaw
};

struct ClockSnapshot {
  uint32_t unixTime;
  int32_t latitudeE7;
  int32_t longitudeE7;
  int32_t altitudeMslMm;
  bool positionValid;
};

struct BatterySnapshot {
  float voltage;
  float percent;
  bool valid;
};

struct MeasurementRecord {
  uint32_t unixTime;
  int32_t distanceMm;
  float batteryVoltage;
  float batteryPercent;
};

struct GnssBuffer {
  size_t length;
  uint8_t data[sdWriteSize];
};

enum class TaskId : uint8_t {
  Clock,
  Storage,
  Radar,
  Sleep,
  Voltage,
  Bluetooth,
  Count
};

struct Heartbeat {
  TaskId task;
  TickType_t tick;
};

extern RTC_DATA_ATTR uint32_t wakeCounter;
extern RTC_DATA_ATTR uint32_t lastFixedUnix;
extern RTC_DATA_ATTR float previousBatteryPercent;

extern EventGroupHandle_t lifecycleEvents;
extern QueueHandle_t measurementQueue;
extern QueueHandle_t gnssReadyQueue;
extern QueueHandle_t gnssFreeQueue;
extern QueueHandle_t heartbeatQueue;
extern SemaphoreHandle_t stateMutex;
extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t sdMutex;

bool sharedDataBegin();

ClockSnapshot getClockSnapshot();
void setClockSnapshot(const ClockSnapshot &snapshot);
BatterySnapshot getBatterySnapshot();
void setBatterySnapshot(const BatterySnapshot &snapshot);
SurveyMode getSurveyMode();
void setSurveyMode(SurveyMode mode);
uint32_t getReadTimeSeconds();
void setReadTimeSeconds(uint32_t seconds);
uint16_t getAlignmentMinutes();
void setAlignmentMinutes(uint16_t minutes);

void reportHeartbeat(TaskId task);
void signalFatalError(const char *subsystem, const char *message);

#endif
