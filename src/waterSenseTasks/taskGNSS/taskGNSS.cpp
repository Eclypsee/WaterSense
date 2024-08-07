/**
 * @file taskSD.cpp
 * @author Toma Grundler and Armaan Oberai
 * @brief Main file for the GNSS task
 * @version 0.1
 * @date 2024-07-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */


#include "taskGNSS.h"
#include "setup.h"
#include "sharedData.h"
#include "waterSenseLibs/zedGNSS/zedGNSS.h"


void taskGNSS(void *params) {

  uint8_t myBuffer[sdWriteSize];           // Use myBuffer to hold the data while we write it to SD card 
  int numSFRBX = 0; // Keep count of how many SFRBX message groups have been received (see note above) 
  int numRAWX = 0;
  uint64_t runTimer;

  //state setup
  uint8_t state = 0;
  SFE_UBLOX_GNSS gnss = myGNSS.get().getGNSS();
  uint16_t myReadTime;

  //task loop
  while (true) {
      
      //check if GNSS is configured
      if (state = 0) {
        if(gnssPowerSave.get()) {
          state = 3;
        }
        if(gnssInit.get())) {
          runTimer = millis();
          state = 1;
        }
      }

      else if(state = 1) {
        gnss.checkUblox(); // See if new data is available. Process bytes as they come in. 
        gnss.checkCallbacks(); 
        if(gnss.fileBufferAvailable() >= sdWriteSize) {
              gnss.extractFileBufferData(myBuffer, sdWriteSize); // Extract exactly sdWriteSize bytes from the UBX file buffer and put them into myBuffer
              for(int i = 0; i < sdWriteSize; i++) {
                writeBuffer.put(myBuffer[i]);
              }
              gnss.checkUblox(); // Check for the arrival of new data and process it. 
              gnss.checkCallbacks(); // Check if any callbacks are waiting to be processed. 
        }

        myReadTime = GNSS_READ_TIME.get();
        
        //if time has elapsed, terminate measurements
        else if(millis() - runTimer > (myReadTime*1000)) {
          state = 2;
        }
      }
      
      //spit out serial log at end of 12/24-hour session
      else if(state = 2) {
         myGNSS.get().stopLogging();
         uint16_t maxBufferBytes = gnss.getMaxFileBufferAvail(); // Get how full the file buffer has been (not how full it is now) 
         if (maxBufferBytes > ((fileBufferSize / 5) * 4)) // Warn the user if fileBufferSize was more than 80% full
            { 
              Serial.println(F("Warning: the file buffer has been over 80% full. Some data may have been lost."));
            } 
          
          uint16_t remainingBytes = gnss.fileBufferAvailable(); // Check if there are any bytes remaining in the file buffer 
          while (remainingBytes > 0) // While there is still data in the file buffer 
          { 
            uint16_t bytesToWrite = remainingBytes; // Write the remaining bytes to SD card sdWriteSize bytes at a time 
            if (bytesToWrite > sdWriteSize) 
              { 
                bytesToWrite = sdWriteSize; 
              }

            gnss.extractFileBufferData(myBuffer, bytesToWrite); // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer 
            for(int i = 0; i < sdWriteSize; i++) {
              writeBuffer.put(myBuffer[i]);
              if(i > bytesToWrite * 2) {
                break;
              }
            }
            remainingBytes -= bytesToWrite; // Decrement remainingBytes 
          }
          
          gnssMeasureDone.put(true);
          
          Serial.print(F("Number of message groups received: SFRBX: ")); // Print how many message groups have been received (see note above) 
          Serial.print(numSFRBX); 
          Serial.print(F(" RAWX: ")); 
          Serial.println(numRAWX); 
          Serial.print(F("Num of writes: ")); // Print how many times we have written to the SD card 
          Serial.print(numSFRBX); 
          Serial.print(F("Num of Wakeups: ")); // Print how many times we have written files to the SD card 
          Serial.println(wakeCounter);

          state = 3; 
      }

      else if(state = 3) {
        gnssPowerSave.put(true);
        vTaskDelay(GNSS_SLEEP);
        gnssPowerSave.put(false);
        state = 0;
      }

    gnssCheck.put(true);
    vTaskDelay(GNSS_PERIOD);
}

 