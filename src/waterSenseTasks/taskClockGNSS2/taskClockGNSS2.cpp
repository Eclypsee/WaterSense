/**
 * @file taskClock.cpp
 * @author Evan Lee
 * @brief Main file for the clock task
 * @version 0.1
 * @date 2023-02-05
 * @copyright Copyright (c) 2023
 */

#include <Arduino.h>
#include "taskClockGNSS2.h"
#include "setup.h"
#include "sharedData.h"
#include "waterSenseLibs/gpsClock/gpsClock.h"
#include "waterSenseLibs/zedGNSS/zedGNSS.h"
#include <Wire.h>
#include <RTClib.h>
#include <ESP32Time.h>

/**
 * @brief The clock task
 * @details Runs the GPS clock in the background to maintain timestamps
 * 
 * @param params A pointer to task parameters
 */
void taskClockGNSS2(void* params)
{
  GNSS myGNSS = GNSS(SDA, SCL, CLK);
  uint8_t state = 0;
  RTC_DS3231 ada_rtc;
  while (true)
  {
    //radarSleepReady.put(true);radarCheck.put(true);//for testing WITHOUT radar
    bluetoothSleepReady.put(true);bluetoothCheck.put(true);//for testing without bluetooth
    // Begin
    if (state == 0)
    {
      while(!ada_rtc.begin(&Wire)){
        Serial.println("Exernal RTC not found");
        vTaskDelay(200);
      }
      Serial.println("GNSSv2 Wakeup, begin enabling GNSS");
      if (wakeCounter == 0||(ada_rtc.now().unixtime()-lastFixedUTX) >= 2592000UL)//check to see if 1 month passed
      {
        Serial.println("Initiating Monthly 20 hour survey");
        lastFixedUTX = ada_rtc.now().unixtime();
        myGNSS.start(); 
        inLongSurvey.put(1);
        vTaskDelay(CLOCK_PERIOD);
        state = 2;
      }
      else
      {
        Serial.println("Getting Timestamp from internal RTC");
        inLongSurvey.put(0);
        vTaskDelay(CLOCK_PERIOD);
        wakeReady.put(true);

        state = 1;
      }
    }
    else if (state == 1)
    {
      unixTime.put(ada_rtc.now().unixtime());
      displayTime.put(String(ada_rtc.now().unixtime()));
      clockSleepReady.put(true);
      sleepTime.put((uint64_t) (READ_TIME.get() * 1000000));
    }
    // Update
    else if (state == 2)
    {
      bool locFix = myGNSS.gnss.getGnssFixOk();
      bool timeValid = myGNSS.gnss.getTimeValid();
      bool dateValid = myGNSS.gnss.getDateValid();
      fixType.put(locFix&&timeValid&&dateValid);
      while(fixType.get() != true) {
          myGNSS.gnss.factoryReset(); // Cold start - clears position data
          vTaskDelay(5000);
          myGNSS.start();
          Serial.println("Cold Starting...");
          //clockCheck.put(true);
      }
      unixTime.put(myGNSS.gnss.getUnixEpoch());
      myGNSS.setDisplayTime();
      Serial.println("GNSSv2 2, Unix Time: " + String(myGNSS.gnss.getUnixEpoch()));
      ada_rtc.adjust(DateTime(myGNSS.gnss.getUnixEpoch()));
      vTaskDelay(500);
      // wakeReady.put(true);//have wakeready be set by zedgnss.cpp
      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        Serial.println("GNSSv2 1 -> 3, sleepFlag ready");
        latitude.put(myGNSS.gnss.getHighResLatitude());
        longitude.put(myGNSS.gnss.getHighResLongitude());
        altitude.put(myGNSS.gnss.getAltitudeMSL() / (int32_t) 1000);
        state = 3;
      }
      myGNSS.getGNSSData();//GET CURRENT GNSSDATA
    }

    // Sleep
    else if (state == 3)
    {
      //FLUSH REMAINING GNSS DATA/////////////////////////////////////////////////////////////
      uint16_t maxBufferBytes = myGNSS.gnss.getMaxFileBufferAvail(); // Get how full the file buffer has been (not how full it is now) 
      if (maxBufferBytes > ((fileBufferSize / 5) * 4)){// Warn the user if fileBufferSize was more than 80% full 
            Serial.println(F("Warning: the file buffer has been over 80% full. Some data may have been lost."));
      } 
      uint16_t remainingBytes = myGNSS.gnss.fileBufferAvailable(); // Check if there are any bytes remaining in the file buffer 
      while (remainingBytes > 0){ // While there is still data in the file buffer 
          uint16_t bytesToWrite = remainingBytes; // Write the remaining bytes to SD card sdWriteSize bytes at a time 
          if (bytesToWrite > sdWriteSize){ 
              bytesToWrite = sdWriteSize; 
            }
          myGNSS.gnss.extractFileBufferData(myBuffer, bytesToWrite); // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer 
        remainingBytes -= bytesToWrite; // Decrement remainingBytes 
      }
      ///////////////////////////////////////////////////////////////////////////////////////
      unixTime.put(myGNSS.gnss.getUnixEpoch());
      myGNSS.setDisplayTime();

      // Calculate sleep time
      sleepTime.put((uint64_t) (READ_TIME.get() * 1000000));//after surveying for 20 hours, gnss task sleeps for a bit and wakes up as RTC task

      Serial.println("GNSSv2 3, GPS going to sleep");

      myGNSS.gnss.end();
      vTaskDelay(500);
      while(myGNSS.gnss.powerOff(0) != true);//powers off indefinitely until next month

      vTaskDelay(1000);

      clockSleepReady.put(true);
      state = 4;
    }
    if(state == 4) {
      Serial.println("GNSSv2 4, sleeping ");
      vTaskDelay(2000);
    }
    clockCheck.put(true);
    vTaskDelay(CLOCK_PERIOD);
  }
}