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
  ESP32Time myRTC(1);

  lastKnownUnix = myGNSS.get().getGNSS().getUnixEpoch();

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
      // Update the GPS
      myGPS.update(myClock);

      // If new data is available, go to state 2
      if (myGPS.newData)
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
      myGPS.update(myClock);

      latitude.put(myGPS.latitude);
      longitude.put(myGPS.longitude);
      altitude.put(myGPS.altitude);
      fixType.put(myGPS.fixType);


      // If we've switched to the internal clock, use it!
      if (internal)
      {
        unixTime.put(myGPS.getUnixInternal(myRTC));
        displayTime.put(myGPS.getDisplayInternal(myRTC));
      }

      // Otherwise, if the GPS has a fix, use it to set the time
      else if (myGPS.fixType)
      {
        unixTime.put(myGPS.getUnixTime(myClock));
        displayTime.put(myGPS.getDisplayTime(myClock));
      }

      // Otherwise show zero
      else
      {
        unixTime.put(String(0));
        displayTime.put("NaT");
      }

      Serial.println("GPS Clock2 2 -> 1, good GPS fix!");
      
      state = 1;

      // Switch to the internal RTC if we have a good fix
      if (myGPS.fixType)
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

      // Calculate sleep time
      if (myGPS.fixType)
      {
        sleepTime.put(myGPS.getSleepInternal(myRTC, myAllign, myRead));
      }

      else
      {
        sleepTime.put(myGPS.getSleepTime(myClock, myAllign, myRead));
      }
      
      Serial.println("GPS Clock2 3, GPS going to sleep");

      // Disable GPS
      myGPS.sleep(myClock);
      clockSleepReady.put(true);
    }

    // Get fix
    else if (state == 4)
    {
      Serial.println("GPS Clock2 4 -> 1, GPS getting fix (blink)");
      // wakeCounter = 0;
      wakeReady.put(myGPS.getFix(myClock, FIX_DELAY));

      state = 1;
    }

    // Switch to internal RTC
    else if (state == 5)
    {
      // Update internal clock
      myGPS.updateInternal(myClock, myRTC);
      unixRtcStart = myGPS.internalStart;
      lastKnownUnix = myGPS.lastGpsUnix;

      // Turn off GPS
      myGPS.sleep(myClock);

      state = 6;
    }

    // Read from RTC
    else if (state == 6)
    {
      unixTime.put(myGPS.getUnixInternal(myRTC));
      displayTime.put(myGPS.getDisplayInternal(myRTC));
      
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