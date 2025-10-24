/**
 * @file taskSD.cpp
 * @author Alexander Dunn, Evan Lee
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
  ExFile myFile;
  ExFile GNSS;

  // Task Setup
  uint8_t state = 0;

  // Task Loop
  while (true)
  {
    if(writeFinishedSD.get() && BluetoothConnected.get()){
      state = 6;//SUSPEND SD OPERATIONS
      continue;
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


        if(inLongSurvey.get()==1){
          writeFinishedSD.put(false);
          GNSS = mySD.createGNSSFile();
          writeFinishedSD.put(true);
        }
        writeFinishedSD.put(false);
        myFile = mySD.createFile(unixTime.get());
        writeFinishedSD.put(true);

        fileCreated.put(true);

        state = 1;
      }
    }

    // Check for data and populate buffer
    else if (state == 1)
    {
      uint32_t gnssDataReadyValue = gnssDataReady.get();
      if(inLongSurvey.get()==1){
        if (gnssDataReadyValue) //hangs whyyyyyyy?
        {//store gnss data, move on
          gnssDataReady.put(false);
          writeFinishedSD.put(false);
      
          String path = mySD.getGNSSFilePath();
    
          ExFile checkFile = SD.open(path.c_str(), O_RDONLY);
          if (checkFile && checkFile.size() >= MAX_FILESIZE) {
            Serial.println("GNSS file too large, creating new file");
            checkFile.close();
            GNSS = mySD.createGNSSFile(); // This should update the internal path
            path = mySD.getGNSSFilePath(); // Get the new path
          } else if (checkFile) {
              checkFile.close();
          }
    
          GNSS = SD.open(path.c_str(), O_RDWR | O_CREAT | O_APPEND);
          mySD.writeGNSSData(GNSS, myBuffer);
          mySD.sleep(GNSS);
    
          Serial.println("GNSS data written to SD card");
    
          writeFinishedSD.put(true);
    
        }
      }
      if(dataReady.get()){
        dataReady.put(false);
        state = 2;
      }
      // If sleepFlag is tripped, go to state 3
      if (sleepFlag.get() && !gnssDataReadyValue)
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

      // Get voltages
      float batteryP = batteryPercent.get();
      float batteryVoltage = battery.get();

      uint32_t myTime = unixTime.get();

      // Write data to SD card
      String path = mySD.getDataFilePath();

      ExFile checkFile = SD.open(path.c_str(), O_RDONLY);
      if (checkFile && checkFile.size() >= MAX_FILESIZE) {
        checkFile.close();
        myFile = mySD.createFile(unixTime.get()); // This should update the internal path
        path = mySD.getDataFilePath(); // Get the new path
      } else if (checkFile) {
          checkFile.close();
      }

      myFile = SD.open(path.c_str(), O_RDWR | O_CREAT | O_APPEND);
      mySD.writeData(myFile, myDist, myTime, batteryVoltage, batteryP);
      mySD.sleep(myFile);

      // Print data to serial monitor
      Serial.printf("%d, %d, %0.2f, %0.2f, %0.2f, %0.2f\n", myTime, myDist, batteryVoltage, batteryP);
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

    else if(state == 6)//suspend sd operations(not sleep)
    {
      // Close data file
      mySD.sleep(myFile);
      mySD.sleep(GNSS);
      sdSleepReady.put(true);
      if(BluetoothConnected.get() == false){
        state = 1;
      }
    }

    sdCheck.put(true);
    vTaskDelay(SD_PERIOD);
  }
}