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
  SD_Data mySD(SD_CS);
  File myFile;
  File GNSS;

  // Task Setup
  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    if(writeFinishedSD.get() && stopOperationSD.get()){
      state = 6;//SUSPEND SD OPERATIONS
    }
    // Begin
    if (state == 0)
    {
      if (wakeReady.get())
      {
        // Check/create header files
        if ((wakeCounter % 1000) == 0)
        {
          writeFinishedSD.put(false);
          mySD.writeHeader();
          writeFinishedSD.put(true);
        }



        #ifndef STANDALONE
          writeFinishedSD.put(false);
          myFile = mySD.createFile(fixType.get(), wakeCounter, unixTime.get());
          writeFinishedSD.put(true);
        #endif
        #ifndef NO_SURVEY
          writeFinishedSD.put(false);
          GNSS = mySD.createGNSSFile();
          writeFinishedSD.put(true);
        #endif

        fileCreated.put(true);

        state = 1;
      }
    }

    // Check for data and populate buffer
    else if (state == 1)
    {

      #ifndef NO_SURVEY
      // If gnssDataFlag is tripped, go to state 5
      if (gnssDataReady.get()) 
      {
        gnssDataReady.put(false);
        state = 5;
      }
      #endif

      #ifndef STANDALONE
      // If data is available, untrip dataFlag go to state 2
      if (dataReady.get())
      {
        dataReady.put(false);
        state = 2;
      }
      #endif

      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get() && !gnssDataReady.get())
      {
        state = 3;
      }
    }

    // Store data
    else if (state == 2)
    {
      writeFinishedSD.put(false);
      // Get sonar data
      int16_t myDist = distance.get();

      // Get temp data
      float myTemp = temperature.get();

      // Get humidity data
      float myHum = humidity.get();

      // Get voltages
      float solarVoltage = solar.get();
      float batteryVoltage = battery.get();

      uint32_t myTime = unixTime.get();

      // Write data to SD card
      String path = mySD.getDataFilePath();
      myFile = SD.open(path, FILE_APPEND, false);
      mySD.writeData(myFile, myDist, myTime, myTemp, myHum, batteryVoltage, solarVoltage);
      mySD.sleep(myFile);
      // myFile.printf("%s, %d, %f, %f, %d\n", unixTime.get(), myDist, myTemp, myHum, myFix);

      // Print data to serial monitor
      Serial.printf("%d, %d, %0.2f, %0.2f, %0.2f, %0.2f\n", myTime, myDist, myTemp, myHum, batteryVoltage, solarVoltage);
      Serial.println(myTime);

      writeFinishedSD.put(true);

      state = 1;
    }

    // Write Log
    else if (state == 3)
    {
      writeFinishedSD.put(false);
      // If we have a fix, write data to the log
      if (fixType.get())
      {
        Serial.printf("Writing log file Time: %s\n", displayTime.get());
        uint32_t tim = unixTime.get();
        int32_t lat = latitude.get();
        int32_t lon = longitude.get();
        int32_t alt = altitude.get();

        mySD.writeLog(tim, wakeCounter, lat, lon, alt);

        writeFinishedSD.put(true);
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
    else if(state == 5)
    {
      writeFinishedSD.put(false);
      
      String path = mySD.getGNSSFilePath();
      GNSS = SD.open(path, FILE_APPEND, false);
      mySD.writeGNSSData(GNSS, myBuffer);
      mySD.sleep(GNSS);


      Serial.println("GNSS data written to SD card");

      writeFinishedSD.put(true);

      state = 1;
    }
    else if(state == 6)//suspend sd operations(not sleep)
    {
      // Close data file
      mySD.sleep(myFile);
      mySD.sleep(GNSS);
      sdSleepReady.put(true);
      if(stopOperationSD.get() == false){
        state = 1;
      }
    }

    sdCheck.put(true);
    vTaskDelay(SD_PERIOD);
  }
}