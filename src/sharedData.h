/**
 * @file sharedData.h
 * @author Alexander Dunn
 * @brief A file to contain all shared variables
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "waterSenseLibs/shares/taskshare.h"
#include "waterSenseLibs/shares/taskqueue.h"
#include "setup.h"

//-----------------------------------------------------------------------------------------------------||
//---------- Shares & Queues --------------------------------------------------------------------------||

// Non-Volatile Memory
extern RTC_DATA_ATTR uint32_t wakeCounter; ///< A counter representing the number of wake cycles
extern RTC_DATA_ATTR uint32_t lastKnownUnix;
extern RTC_DATA_ATTR uint32_t unixRtcStart;
extern RTC_DATA_ATTR bool internal;

// Watchdog Checks
extern Share<bool> clockCheck;
extern Share<bool> sleepCheck;
extern Share<bool> measureCheck;
extern Share<bool> voltageCheck;
extern Share<bool> sdCheck;
extern Share<bool> radarCheck;

// Flags
extern Share<bool> dataReady;
extern Share<bool> sleepFlag;
extern Share<bool> clockSleepReady;
extern Share<bool> sonarSleepReady;
extern Share<bool> tempSleepReady;
extern Share<bool> radarSleepReady;
extern Share<bool> radarDataReady;
extern Share<bool> sdSleepReady;
extern Share<bool> bluetoothSleepReady;
extern Share<bool> gnssPowerSave;
extern Share<bool> gnssDataReady;
extern Share<bool> fileCreated;

// Shares from GNSS
extern Share<int32_t> latitude;
extern Share<int32_t> longitude;
extern Share<int32_t> altitude;
extern Share<bool> fixType;
extern Share<uint32_t> unixTime;
extern Share<String> displayTime;
extern Share<bool> wakeReady;
extern Share<uint64_t> sleepTime;

// Shares from sensors
extern Share<int16_t> distance;
extern Share<float> temperature;
extern Share<float> humidity;
extern Share<int> radarDistance;

//Shares from GNSS
extern Share<int> numSFRBX;
extern Share<int> numRAWX;
extern uint8_t *myBuffer;

// Duty Cycle
extern Share<float> solar;
extern Share<float> battery;
extern Share<uint32_t> READ_TIME;
extern Share<uint16_t> MINUTE_ALLIGN;

#endif //SHARED_DATA_H

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||