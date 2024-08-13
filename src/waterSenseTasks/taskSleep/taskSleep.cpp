/**
 * @file taskSleep.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <Arduino.h>
#include "taskSleep.h"
#include "setup.h"
#include "sharedData.h"

/**
 * @brief The sleep task
 * @details Sets the sleep time and triggers sleep
 * 
 * @param params A pointer to task parameters
 */
void taskSleep(void* params)
{
  // Task Setup
  uint8_t state = 0;
  uint64_t runTimer = millis();

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
      if (wakeReady.get())
      {
        // Start run timer
        runTimer = millis();

        // Increment wake counter
        wakeCounter++;

        // Make sure sleep flag is not set
        sleepFlag.put(false);

        Serial.printf("Wakeup number %d Time: %s\n", wakeCounter, displayTime.get());

        Serial.printf("Sleep state 0 -> 1 Time: %s\n", displayTime.get());
        state = 1;
      }
    }

    // Wait
    else if (state == 1)
    {
      // If runTimer, go to state 2
      uint16_t myReadTime = READ_TIME.get();
      #ifdef STANDALONE
        myReadTime = GNSS_READ_TIME.get();
      #endif
      if (((millis() - runTimer) > myReadTime*1000))
      {
        Serial.printf("Sleep state 1 -> 2 Time: %s\n", displayTime.get());

        // Set sleep flag
        sleepFlag.put(true);
        state = 2;
      }
    }

    // Initiate Sleep
    else if (state == 2)
    {
      #ifdef STANDALONE
        sonarSleepReady.put(true);
        tempSleepReady.put(true);
      #endif
      // If all tasks are ready to sleep, go to state 3
      if (sonarSleepReady.get() && tempSleepReady.get() && clockSleepReady.get() && sdSleepReady.get())
      {
        Serial.printf("Sleep state 2 -> 3 Time: %s\n", displayTime.get());
        state = 3;
      }
    }

    // Sleep
    else if (state == 3)
    {
      // Get sleep time
      uint64_t mySleep = sleepTime.get();
      uint64_t myAllign = MINUTE_ALLIGN.get();
      // mySleep /= 1000000;

      // Go to sleep    
      gpio_deep_sleep_hold_en();

      #ifdef CONTINUOUS // If set to continuous mode, sleep for 1uS
        Serial.printf("Writing to sd card Time: %s\n", displayTime.get());
        esp_sleep_enable_timer_wakeup(1);

      #else // If not set to continuous mode, sleep for calculated time
        Serial.printf("Read time: %d minutes\nMinute Allign: %d\n", READ_TIME.get()/60, myAllign);
        Serial.printf("%s: Going to sleep for ", displayTime.get());
        Serial.print(mySleep/1000000);
        Serial.printf(" seconds Time: %s\n", displayTime.get());

        if ((mySleep/1000000) > (myAllign*60))
        {
          esp_sleep_enable_timer_wakeup(myAllign*60*1000000);
        }

        else
        {
          esp_sleep_enable_timer_wakeup(mySleep);
        }
      #endif
      
      esp_deep_sleep_start();

      state = 0;
    }

    sleepCheck.put(true);
    vTaskDelay(SLEEP_PERIOD);
  }
}