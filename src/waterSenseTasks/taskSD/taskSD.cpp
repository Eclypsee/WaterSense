/**
 * @file taskSD.cpp
 * @author Alexander Dunn
 * @brief Main file for the SD task
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <Arduino.h>
#include "taskSD.h"
#include "setup.h"
#include "sharedData.h"
#include "waterSenseLibs/sdData/sdData.h"

/**
 * @brief The SD storage task
 * @details Creates relevant files on   the SD card and stores all data
 * 
 * @param params A pointer to task parameters
 */
void taskSD(void* params)
{
  SD_Data mySD(GPIO_NUM_5);
  File myFile;
  File GNSS;
  int i = 0;
  bool gnssDataReady = false;

  uint8_t buffer[SIZE];

  // Task Setup
  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    // Begin
    if (state == 0)
    {
      if (wakeReady.get())
      {
        // Check/create header files
        if ((wakeCounter % 1000) == 0)
        {
          mySD.writeHeader();
        }

        myFile = mySD.createFile(fixType.get(), wakeCounter, unixTime.get());
        GNSS = mySD.createGNSSFile();

        state = 1;
      }
    }

    // Check for data and populate buffer
    else if (state == 1)
    {
      // If data is available, untrip dataFlag go to state 2
      if (dataReady.get())
      {
        dataReady.put(false);
        state = 2;
      }

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get())
      {
        state = 3;
      }

      // If gnssDataFlag is tripped, go to state 5
      if (gnssDataReady) 
      {
        gnssDataReady = false;
        state = 5;
      }

      else if (gnssMeasureDone.get()) {
        gnssMeasureDone.put(false);
        state = 6;
      }

      while(!(writeBuffer.is_empty())) {
        writeBuffer.get(buffer[i++]);
        if(i == SIZE) {
          i = 0;
          gnssDataReady = true;
        }
      }
    }

    // Store data
    else if (state == 2)
    {
      // Get sonar data
      int16_t myDist = distance.get();

      // Get temp data
      float myTemp = temperature.get();

      // Get humidity data
      float myHum = humidity.get();

      // Get voltages
      float solarVoltage = solar.get();
      float batteryVoltage = battery.get();

      u_int32_t myTime = unixTime.get();

      // Write data to SD card
      mySD.writeData(myFile, myDist, myTime, myTemp, myHum, batteryVoltage, solarVoltage);
      // myFile.printf("%s, %d, %f, %f, %d\n", unixTime.get(), myDist, myTemp, myHum, myFix);

      // Print data to serial monitor
      // Serial.printf("%s, %d, %0.2f, %0.2f, %0.2f, %0.2f\n", displayTime.get(), myDist, myTemp, myHum, batteryVoltage, solarVoltage);
      // Serial.println(myTime);

      state = 1;
    }

    // Write Log
    else if (state == 3)
    {
      // If we have a fix, write data to the log
      if (fixType.get())
      {
        Serial.printf("Writing log file Time: %s\n", displayTime.get());
        uint32_t tim = unixTime.get();
        float lat = latitude.get();
        float lon = longitude.get();
        float alt = altitude.get();

        mySD.writeLog(tim, wakeCounter, lat, lon, alt);
      }

      state = 4;
    }

    // Sleep
    else if (state == 4)
    {
      // Close data file
      mySD.sleep(myFile);
      mySD.sleep(GNSS);
      sdSleepReady.put(true);
    }

    // Store GNSS data
    else if(state = 5)
    {
      mySD.writeGNSSData(GNSS, buffer);

      state = 1;
    }

    //close GNSS file when measurement stops
    else if(state = 6) {
      mySD.sleep(GNSS);
      state = 3;
    }

    sdCheck.put(true);
    vTaskDelay(SD_PERIOD);
  }
}