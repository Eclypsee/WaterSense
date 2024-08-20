/**
 * @file taskClock.cpp
 * @author Alexander Dunn
 * @brief Main file for the clock task
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <Arduino.h>
#include "taskClock2.h"
#include "setup.h"
#include "sharedData.h"
#include "waterSenseLibs/gpsClock/gpsClock.h"
#include "waterSenseLibs/zedGNSS/zedGNSS.h"

/**
 * @brief The clock task
 * @details Runs the GPS clock in the background to maintain timestamps
 * 
 * @param params A pointer to task parameters
 */
void taskClock2(void* params)
{
  uint64_t runTimer;
  uint16_t myReadTime;

  GNSS* globalGNSS = new GNSS(SDA, SCL, CLK);
  GNSS myGNSS = *globalGNSS;
  myGNSS.start();

  ESP32Time myRTC(1);

  lastKnownUnix = myGNSS.gnss.getUnixEpoch();
  Serial.printf("%zu\n", lastKnownUnix);

  uint8_t state = 0;

  while (true)
  {
    // Begin
    if (state == 0)
    {
      Serial.println("GPS Clock2 Wakeup, begin enabling GNSS");
      if (!(wakeReady.get()))
      {
        Serial.println("GPS Clock2 0 -> 4 Wakeup, getting fix");
        state = 4;
      }

      else
      {
        Serial.println("GPS Clock2 0 -> 1, Update GPS");
        wakeReady.put(true);
        state = 1;
      }
    }

    // Update
    else if (state == 1)
    {
      while(myGNSS.gnss.getGnssFixOk() != true) {
        unixTime.put(myGNSS.gnss.getUnixEpoch());
      }
      
      fixType.put(true);
      

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        Serial.println("GPS Clock2 1 -> 3, sleepFlag ready");
        fixType.put(myGNSS.gnss.getGnssFixOk());
        latitude.put(myGNSS.gnss.getHighResLatitude());
        longitude.put(myGNSS.gnss.getHighResLongitude());
        altitude.put(myGNSS.gnss.getAltitudeMSL());
        state = 3;
      }

      #ifndef STANDALONE
      // If new data is available, go to state 2
      else if (dataReady.get())
      {
        Serial.println("GPS Clock2 1 -> 2, dataFlag ready");
        state = 2;
      }
      #endif

      if(!(gnssPowerSave.get()) && !(gnssDataReady.get())) {
        delay(100);
        wakeReady.put(true);
        state = 7;
      }

    }

    // Read from GPS
    else if (state == 2)
    {
      // If we've switched to the internal clock, use it!
      if (internal)
      {
        unixTime.put(myRTC.getLocalEpoch());
        displayTime.put(myRTC.getTimeDate());
      }

      // Otherwise, if the GPS has a fix, use it to set the time
      else if (myGNSS.getGNSS().getFixType())
      {
        unixTime.put(myGNSS.gnss.getUnixEpoch());
        displayTime.put(myGNSS.getDisplayTime());
      }

      // Otherwise show zero
      else
      {
        unixTime.put(0);
        displayTime.put("NaT");
      }

      Serial.println("GPS Clock2 2 -> 1, good GPS fix!");
      
      state = 1;

      // Switch to the internal RTC if we have a good fix
      if (!(myGNSS.getGNSS().getGnssFixOk()))
      {
        Serial.printf("Switching to internal RTC! Time: %s\n", displayTime.get());
        internal = true;
        state = 5;
      }
    }

    // Sleep
    else if (state == 3)
    {
      myGNSS.stopLogging();
      uint16_t maxBufferBytes = myGNSS.gnss.getMaxFileBufferAvail(); // Get how full the file buffer has been (not how full it is now) 
      if (maxBufferBytes > ((fileBufferSize / 5) * 4)) // Warn the user if fileBufferSize was more than 80% full
         { 
            Serial.println(F("Warning: the file buffer has been over 80% full. Some data may have been lost."));
         } 
          
      uint16_t remainingBytes = myGNSS.gnss.fileBufferAvailable(); // Check if there are any bytes remaining in the file buffer 
      while (remainingBytes > 0) // While there is still data in the file buffer 
       { 
          uint16_t bytesToWrite = remainingBytes; // Write the remaining bytes to SD card sdWriteSize bytes at a time 
          if (bytesToWrite > sdWriteSize) 
            { 
              bytesToWrite = sdWriteSize; 
            }

          myGNSS.gnss.extractFileBufferData(myBuffer, bytesToWrite); // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer 
        remainingBytes -= bytesToWrite; // Decrement remainingBytes 
      }
          
      // Serial.print(F("Number of message groups received: SFRBX: ")); // Print how many message groups have been received (see note above) 
      // Serial.print(numSFRBX.get()); 
      // Serial.print(F(" RAWX: ")); 
      // Serial.println(numRAWX.get()); 
      // Serial.print(F("Num of writes: ")); // Print how many times we have written to the SD card 
      // Serial.print(numSFRBX.get()); 
      // Serial.print(F("Num of Wakeups: ")); // Print how many times we have written files to the SD card 
      // Serial.println(wakeCounter);
      uint16_t myAllign = MINUTE_ALLIGN.get();
      uint16_t myRead = READ_TIME.get();

      #ifdef STANDALONE
        myRead = GNSS_STANDALONE_SLEEP;
      #endif

      displayTime.put(myGNSS.getDisplayTime());
      // Calculate sleep time
      sleepTime.put(myRead);

      Serial.println("GPS Clock2 3, GPS going to sleep");

      // Disable GNSS
      while (myGNSS.gnss.powerOff(sleepTime.get()) != true)
      {

      };

      clockSleepReady.put(true);
    }

    // Get fix
    else if (state == 4)
    {
      myGNSS.start();


      wakeReady.put(true);

      //Serial.printf("GPS Clock2 4 -> 1, GPS getting fix (blink) %hhu\n", myGNSS.getGNSS().getFixType());

      state = 1;
    }

    // Switch to internal RTC
    else if (state == 5)
    {
      // Update internal clock
      lastKnownUnix = myGNSS.getGNSS().getUnixEpoch();

      state = 6;
    }

    // Read from RTC
    else if (state == 6)
    {
      unixTime.put(myRTC.getEpoch());
      displayTime.put(myRTC.getDateTime());

      state = 1;
      
      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        state = 3;
      }
    }

    //GNSS data retrieve
    else if(state = 7)
    {
      Serial.println("GPS Clock2 1 -> 7, GNSS retrieve data begin");
      myGNSS.getGNSSData();
      
      state = 1;
    }

    clockCheck.put(true);
    vTaskDelay(CLOCK_PERIOD);
  }
}