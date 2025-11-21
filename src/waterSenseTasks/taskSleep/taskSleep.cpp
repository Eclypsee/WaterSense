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
#include <Wire.h>
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
      if (wakeReady.get() && fileCreated.get())
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
      uint32_t myReadTime = READ_TIME.get();
      while(inLongSurvey.get()!=1&&inLongSurvey.get()!=0){vTaskDelay(10);};//if long survey task is hanging
      if(inLongSurvey.get()==1){
        myReadTime =  GNSS_READ_TIME;
      }
      if (((millis() - runTimer) > myReadTime*1000) )//|| (batteryPercent.get()<10))//if battery percent is too low
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
      // If all tasks are ready to sleep, go to state 3
      if (radarSleepReady.get() && clockSleepReady.get() && sdSleepReady.get() && bluetoothSleepReady.get())
      {
        Serial.printf("Sleep state 2 -> 3 Time: %s\n", displayTime.get());
        state = 3;
      }
    }

    // Sleep
    else if (state == 3)
    {
      // Get sleep time
      // uint64_t mySleep = sleepTime.get();
      // uint64_t myAllign = MINUTE_ALLIGN.get();
      // mySleep /= 1000000;
      
      // Go to sleep    
      gpio_deep_sleep_hold_en();//sleep for calculated time
      Serial.printf("Read time: %d minutes\nMinute Allign: %d\n", READ_TIME.get()/60, MINUTE_ALLIGN.get());
      Serial.printf("Going to sleep for ");
      Serial.print(sleepTime.get()/1000000);
      Serial.println(" seconds");
      // Serial.printf(" seconds Time: %s\n", displayTime.get());

      if ((sleepTime.get()/1000000) > (MINUTE_ALLIGN.get()*60))
      {
        Serial.println("Sleeping for sleep time A ");
        Serial.println(MINUTE_ALLIGN.get()*60*1000000);
        prevBatteryPercent = batteryPercent.get();
        esp_sleep_enable_timer_wakeup(MINUTE_ALLIGN.get()*60*1000000);
      }

      else
      {
        Serial.println("Sleeping for sleep time B ");
        Serial.println(sleepTime.get());
        prevBatteryPercent = batteryPercent.get();
        esp_sleep_enable_timer_wakeup(sleepTime.get());
      }
      //Serial.println("deleting le buff");
     // delete[] myBuffer;
      //delete (GNSS*) globalGNSS;

      vTaskDelay(100);
      Serial.println("Entering deep sleep...sweet dreams");
      vTaskDelay(500);
      esp_deep_sleep_start();
    }

    sleepCheck.put(true);
    vTaskDelay(SLEEP_PERIOD);
  }
}