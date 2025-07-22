/**
 * @file main.cpp
 * @author Alexander Dunn
 * @brief The main waterSense software file
 * @version 0.1
 * @date 2022-12-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include <Wire.h>
#include "setup.h"
#include "sharedData.h"
#include "waterSenseTasks/taskMeasure/taskMeasure.h"
#include "waterSenseTasks/taskSD/taskSD.h"
#include "waterSenseTasks/taskClockGNSS/taskClockGNSS.h"
#include "waterSenseTasks/taskClock2/taskClock2.h"
#include "waterSenseTasks/taskSleep/taskSleep.h"
#include "waterSenseTasks/taskVoltage/taskVoltage.h"
#include "waterSenseTasks/taskWatch/taskWatch.h"
#include "waterSenseTasks/taskRadar/taskRadar.h"
#include "waterSenseTasks/taskBluetooth/taskBluetooth.h"

//-----------------------------------------------------------------------------------------------------||
//---------- Shares & Queues --------------------------------------------------------------------------||

// Non-volatile Variables
RTC_DATA_ATTR uint32_t wakeCounter = 0; ///< A counter representing the number of wake cycles
RTC_DATA_ATTR uint32_t lastKnownUnix = 0;
RTC_DATA_ATTR uint32_t unixRtcStart = 0;
RTC_DATA_ATTR bool internal = false;

// Watchdog Checks
Share<bool> clockCheck("Clock Working");
Share<bool> sleepCheck("Sleep Working");
Share<bool> measureCheck("Measure Working");
Share<bool> voltageCheck("Voltage Working");
Share<bool> sdCheck("SD Working");
Share<bool> radarCheck("Radar Working");
Share<bool> bluetoothCheck("Bluetooth Working");

// Flags
Share<bool> dataReady("Data Ready"); ///< A shared variable to indicate availability of new ultrasonic measurements
Share<bool> sleepFlag("Sleep Flag"); ///< A shared variable to trigger sleep operations
Share<bool> clockSleepReady("Clock Sleep Ready"); ///< A shared variable to indicate the clock is ready to sleep
Share<bool> sonarSleepReady("Sonar Sleep Ready"); ///< A shared variable to indicate the sonar sensor is ready to sleep
Share<bool> tempSleepReady("Temp Sleep Ready"); ///< A shared variable to indicate the temp sensor is ready to sleep
Share<bool> sdSleepReady("SD Sleep Ready"); ///< A shared variable to indicate the SD card is ready to sleep
Share<bool> bluetoothSleepReady("Bluetooth Sleep Ready"); ///< A shared variable to indicate the Bluetooth is ready to sleep
Share<bool> gnssPowerSave("GNSS Power Save");
Share<bool> gnssMeasureDone("GNSS Positioning Measurment Done");
Share<bool> gnssDataReady("GNSS buffer ready");
Share<bool> fileCreated("SD files created");
Share<bool> stopOperationSD("Stop SD Operations");///< A shared variable to STOP ALL SD operations
Share<bool> writeFinishedSD("Write Finished");///< A shared variable to indicate SD has finished writing

// Shares from GPS Clock
Share<int32_t> latitude("Latitude"); ///< The current latitude [Decimal degrees]
Share<int32_t> longitude("Longitude"); ///< The current longitude [Decimal degrees]
Share<int32_t> altitude("Altitude"); ///< The current altitude [meters above MSL]
Share<bool> fixType("Fix Type"); ///< The current fix type
Share<uint32_t> unixTime("Unix Time"); ///< The current Unix timestamp relative to GMT
Share<String> displayTime("Display Time"); ///< The current time of day relative to GMT
Share<bool> wakeReady("Wake Ready"); ///< Indicates whether or not the device is ready to wake
Share<uint64_t> sleepTime("Sleep Time"); ///< The number of microseconds to sleep

// Shares from sensors
Share<int16_t> distance("Distance"); ///< The distance measured by the ultrasonic sensor in millimeters
Share<float> temperature("Temperature"); ///< The temperature in Fahrenheit
Share<float> humidity("Humidity"); ///< The relative humidity in %

// Shares from radar
Share<bool> radarSleepReady("Radar Sleep");
Share<int> radarDistance("Radar Distance");
Share<bool> radarDataReady("Radar Data Ready");

//Shares from GNSS
Share<int> numSFRBX("Number of SFRBX msgs"); ///<SFRBX msgs received by GNSS module
Share<int> numRAWX("Number of RAWX msgs"); ///<RAWX msgs received by GNSS module
uint8_t *myBuffer = new uint8_t[sdWriteSize]; /// <Buffer to copy to SD card

// Duty Cycle
Share<float> solar("Solar Voltage"); ///< The solar panel voltage
Share<float> battery("Battery Voltage"); ///< The input voltage to the MCU
Share<uint32_t> READ_TIME("Read Time"); ///< The current read time in seconds
Share<uint16_t> MINUTE_ALLIGN("Minute Allign"); ///< The current minute allignment

//Bluetooth Shares
Share<uint16_t> FILELIST_COUNT("Filelist Count"); //number of filelists generated STARTS FROM Filelist1.txt
//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||








//-----------------------------------------------------------------------------------------------------||
//---------- Program ----------------------------------------------------------------------------------||

void setup()
{
  
  // Setup
  // setCpuFrequencyMhz(80);
  Serial.begin(115200);

  while (!Serial) {}
  Serial.println("\n\n\n\n");

  READ_TIME.put((uint32_t) HI_READ);
  MINUTE_ALLIGN.put((uint16_t) HI_ALLIGN);
  gnssPowerSave.put(false);
  gnssMeasureDone.put(false);
  gnssDataReady.put(false);
  clockSleepReady.put(false);
  sdSleepReady.put(false);
  tempSleepReady.put(false);
  sonarSleepReady.put(false);
  bluetoothSleepReady.put(false);
  fileCreated.put(false);
  stopOperationSD.put(false);
  writeFinishedSD.put(false);

  Wire.setPins(SDA, SCL);
  Wire.begin();
  Wire.setClock((uint32_t) CLK);


  #ifdef RADAR
    temperature.put(0);
    humidity.put(0);
  #endif

  xTaskCreate(taskSD, "SD Task", 8192, NULL, 8, NULL);
  Setup tasks
  #ifndef LEGACY
      #ifdef STANDALONE
        xTaskCreate(taskClockGNSS, "Clock Task", 8192, NULL, 5, NULL);
      #else
        xTaskCreate(taskClockGNSS, "Clock Task", 8192, NULL, 7, NULL);
      #endif
  #endif
wakeReady.put(true);


  xTaskCreate(taskSleep, "Sleep Task", 8192, NULL, 1, NULL);
  xTaskCreate(taskVoltage, "Voltage Task", 8192, NULL, 1, NULL);
  xTaskCreate(taskWatch, "Watchdog Task", 8192, NULL, 10, NULL);
  xTaskCreate(taskBluetooth, "Bluetooth Task", 8192, NULL, 4, NULL);

  #ifdef LEGACY
     xTaskCreate(taskClock2, "Clock Task", 8192, NULL, 5, NULL);
  #endif
  
  #ifndef STANDALONE
    #ifdef RADAR
      xTaskCreate(taskRadar, "Radar Task", 8192, NULL, 6, NULL);
    #else
      xTaskCreate(taskMeasure, "Measurement Task", 8192, NULL, 6, NULL);
    #endif
  #endif
}

void loop()
{
  // Loop
}

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||