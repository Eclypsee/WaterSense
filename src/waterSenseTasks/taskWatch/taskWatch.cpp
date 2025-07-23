/**
 * @file taskWatch.cpp
 * @author Alexander Dunn
 * @brief Main file for the watchdog task
 * @version 0.1
 * @date 2023-02-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <Arduino.h>
#include "taskWatch.h"
#include "setup.h"
#include "sharedData.h"


/**
 * @brief The voltage task
 * @details Checks solar panel voltage and sets duty cycle
 * 
 * @param params A pointer to task parameters
 */
void taskWatch(void* params)
{
  // Task Setup
  bool clock = false;
  bool sd = false;
  bool voltage = false;
  bool sleep = false;
  bool measure = false;
  bool gnss = false;
  bool bluetooth = false;

  uint32_t taskTimer = millis();

  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
        if (wakeReady.get())
        {
          taskTimer = millis();
          state = 1;
        }
    }

    // Check Tasks
    else if (state == 1)
    {
      // Check tasks
      clock = clockCheck.get();
      sd = sdCheck.get();
      voltage = voltageCheck.get();
      sleep = sleepCheck.get();
      measure = radarCheck.get();
      bluetooth = bluetoothCheck.get();
      // If all tasks are good, reset the timer
      if (clock && sd && voltage && sleep && measure && bluetooth)
      {
          // Reset timer
          taskTimer = millis();

          // Reset checks
          clockCheck.put(false);
          sdCheck.put(false);
          voltageCheck.put(false);
          sleepCheck.put(false);
          radarCheck.put(false);
          bluetoothCheck.put(false);
      }
      // Otherwise, check the timer
      else if ((millis() - taskTimer) > WATCH_TIMER)
      {
        Serial.printf("Status - Clock: %s | SD: %s | Voltage: %s | Sleep: %s | Measure: %s | Bluetooth: %s\n",
          clock ? "true" : "false",
          sd ? "true" : "false",
          voltage ? "true" : "false",
          sleep ? "true" : "false",
          measure ? "true" : "false",
          bluetooth ? "true" : "false");
          state = 2;
      }
    }

    // Abort Program
    else if (state == 2)
    {
        Serial.printf("Watchdog Timer Tripped! Time: %s\n", displayTime.get());
        Serial.flush();
        assert(false);
        state = 0;
    }
    
    vTaskDelay(WATCHDOG_PERIOD);
  }
}