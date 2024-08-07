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
  myGNSS.put(GNSS(SDA, SCL, CLK));
  GNSS gnss = myGNSS.get();
  gnss.begin();
  gnssInit.put(true);

  ESP32Time myRTC(1);

  bool saved = false;

  lastKnownUnix = gnss.getGNSS().getUnixEpoch();

  uint8_t state = 0;

  while (true)
  {
    // Begin
    if (state == 0)
    {
      Serial.println("GPS Clock2 Wakeup, begin enabling GPS");
      if ((wakeCounter % WAKE_CYCLES) == 0)
      {
        Serial.println("GPS Clock2 0 -> 4 Wakeup, getting fix");
        wakeReady.put(false);
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

      // If new data is available, go to state 2
      if (gnss.getGNSS().getFixType() > 2)
      {
        Serial.println("GPS Clock2 1 -> 2, dataFlag ready");
        state = 2;
      }

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        Serial.println("GPS Clock2 1 -> 3, sleepFlag ready");
        state = 3;
      }
    }

    // Read from GPS
    else if (state == 2)
    {
      latitude.put(gnss.getGNSS().getHighResLatitude());
      longitude.put(gnss.getGNSS().getHighResLongitude());
      altitude.put(gnss.getGNSS().getAltitudeMSL());
      fixType.put(gnss.getGNSS().getGnssFixOk());


      // If we've switched to the internal clock, use it!
      if (internal)
      {
        unixTime.put(myRTC.getLocalEpoch());
        displayTime.put(myRTC.getTimeDate());
      }

      // Otherwise, if the GPS has a fix, use it to set the time
      else if (gnss.getGNSS().getGnssFixOk())
      {
        unixTime.put(gnss.getGNSS().getUnixEpoch());
        displayTime.put(gnss.getDisplayTime());
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
      if (!(gnss.getGNSS().getGnssFixOk()))
      {
        Serial.printf("Switching to internal RTC! Time: %s\n", displayTime.get());
        internal = true;
        state = 5;
      }
    }

    // Sleep
    else if (state == 3)
    {
      uint16_t myAllign = MINUTE_ALLIGN.get();
      uint16_t myRead = READ_TIME.get();

      #ifdef STANDALONE
        myRead = GNSS_STANDALONE_SLEEP;
      #endif

      // Calculate sleep time
      sleepTime.put(myRead);

      Serial.println("GPS Clock2 3, GPS going to sleep");

      #ifdef STANDALONE
        // Disable GNSS
        while (gnss.getGNSS().powerOff(sleepTime.get()) != true)
        {

        };
      #endif
      
      clockSleepReady.put(true);
    }

    // Get fix
    else if (state == 4)
    {
      Serial.println("GPS Clock2 4 -> 1, GPS getting fix (blink)");
      // wakeCounter = 0;
      wakeReady.put(myGNSS.get().getGNSS().getGnssFixOk());

      state = 1;
    }

    // Switch to internal RTC
    else if (state == 5)
    {
      // Update internal clock
      lastKnownUnix = gnss.getGNSS().getUnixEpoch();

      state = 6;
    }

    // Read from RTC
    else if (state == 6)
    {
      unixTime.put(myRTC.getEpoch());
      displayTime.put(myRTC.getDateTime());
      
      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        state = 3;
      }
    }

    clockCheck.put(true);
    vTaskDelay(CLOCK_PERIOD);
  }
}