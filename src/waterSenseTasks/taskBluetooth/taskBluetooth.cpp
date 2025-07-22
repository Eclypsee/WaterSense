/**
 * @file taskBluetooth.cpp
 * @author Evan Lee
 * @brief Main file for the Bluetooth advertising task
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <Arduino.h>
#include <ArduinoBLE.h>
#include "taskBluetooth.h"
#include "setup.h"
#include "sharedData.h"
#include "waterSenseLibs/bluetooth/bluetooth.h"

// Declare external global instance
extern BluetoothFileManager bluetoothFileManager;

/**
 * @brief The Bluetooth task
 * @details Advertises for 100ms every 10 seconds (9.9s off, 0.1s on)
 * 
 * Write filelist.txt to SD card, be careful with race conditions
 * 
 * @param params A pointer to task parameters
 */
void taskBluetooth(void* params)
{

  // BLE Service and Characteristics (global scope)
  BLEService dataService("12345678-1234-5678-1234-56789abcdef0");
  BLEStringCharacteristic fileRequestChar("12345678-1234-5678-1234-56789abcdef2", BLEWrite, 50);
  BLECharacteristic fileChunkChar("12345678-1234-5678-1234-56789abcdef3", BLENotify, 110);
  BLEStringCharacteristic checksumChar("12345678-1234-5678-1234-56789abcdef4", BLERead | BLEWrite, 50);
  BLEStringCharacteristic statusChar("12345678-1234-5678-1234-56789abcdef5", BLENotify, 50);

  // File transfer variables
  String requestedFile = "";
  int offset = 0;
  uint32_t calculatedChecksum = 0;
  uint32_t receivedChecksum = 0;
  bool transferComplete = false;

  // Task Setup
  uint8_t state = 0;
  UBaseType_t originalPriority = uxTaskPriorityGet(NULL);

  // Wait for Serial to be ready
  vTaskDelay(pdMS_TO_TICKS(2000));
  Serial.println("Bluetooth task started, awaiting wakeready");

  // Task Loop
  while (true)
  {
      if(state == 0) {
        if(true) {
          writeFinishedSD.put(true);
          // Reset file transfer state variables
          offset = 0;
          requestedFile = "";
          calculatedChecksum = 0;
          transferComplete = false;

          // Initialize BLE
          BLE.begin();
          
          // Initialize Bluetooth file manager
          bluetoothFileManager.begin();

          // Set up BLE advertising
          BLE.setLocalName("WaterSense");
          BLE.setAdvertisedServiceUuid("12345678-1234-5678-1234-56789abcdef0");
          
          // Add characteristics to the service
          dataService.addCharacteristic(fileRequestChar);
          dataService.addCharacteristic(fileChunkChar);
          dataService.addCharacteristic(checksumChar);
          dataService.addCharacteristic(statusChar);

          BLE.addService(dataService);
          
          // Signal that Bluetooth is always ready to sleep
          bluetoothSleepReady.put(true);

          Serial.println("Bluetooth task initialized with SD card file transfer interface");

          state = 1;
        }
      }

      else if(state == 1) {//ADVERTISE
        
        //resume normal SD operations
        stopOperationSD.put(false);

        //tell watchdog I am alive
        bluetoothCheck.put(true);
        // Wait 9.9 seconds using vTaskDelay
        vTaskDelay(pdMS_TO_TICKS(9900));
        
        // Start advertising
        BLE.advertise();
        Serial.println("Bluetooth advertising started");
        // Check for connection during advertising
        BLEDevice central = BLE.central();
        if (central) {
          Serial.print("Connected to: ");
          Serial.println(central.address());
          state = 2;
          vTaskPrioritySet(NULL, 20); // Increase priority when connected
          bluetoothSleepReady.put(false); // Prevent sleep while connected
          stopOperationSD.put(true);//stop SD operation after writes finished
          while(writeFinishedSD.get()!=true){
            vTaskDelay(pdMS_TO_TICKS(20));
            bluetoothCheck.put(true);
          }
          vTaskDelay(pdMS_TO_TICKS(100));//delay a bit to let SD task wrap up. 
        }

        // Stop advertising after 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
        BLE.stopAdvertise();
        Serial.println("Bluetooth advertising stopped");
        bluetoothSleepReady.put(true);
      }

      else if(state == 2) {//CONNECTED
        if (BLE.connected()) {
          // Check for file request
          if (fileRequestChar.written()) {
            requestedFile = fileRequestChar.value();
            offset = 0;
            transferComplete = false;
            
            // Check if client is requesting file list
            if (requestedFile == "filelist.txt") {
              Serial.println("File list requested - generating filelist.txt");
              
              // Generate the file list
              if (bluetoothFileManager.generateFileList()) {
                statusChar.writeValue(String("FILELISTS_MADE ")+FILELIST_COUNT.get());
              } else {
                Serial.println("Failed to generate filelist.txt");
                statusChar.writeValue("FILELISTS_FAILED");
                state = 5;
              }
            } else {
              // Load the requested file
              if (bluetoothFileManager.loadFile(requestedFile)) {
                calculatedChecksum = bluetoothFileManager.getCurrentChecksum();
                Serial.print("File requested: ");
                Serial.println(requestedFile);
                Serial.print("File loaded: ");
                Serial.print(bluetoothFileManager.getFileData().length());
                Serial.println(" bytes");
                Serial.print("Calculated checksum: ");
                Serial.println(calculatedChecksum);
                state = 3;
                bluetoothSleepReady.put(false);
              } else {
                Serial.print("Failed to load file: ");
                statusChar.writeValue("FILE_LOAD_FAILED");
                Serial.println(requestedFile);
                state = 5;
              }
            }
          }
        } else {
          // Disconnected
          state = 1;
          vTaskPrioritySet(NULL, originalPriority); // Restore original priority
          Serial.println("Bluetooth disconnected - returning to normal priority");
        }
      }

      else if(state == 3) {//TRANSFER
        if (BLE.connected()) {
          String fileData = bluetoothFileManager.getFileData();
          
          if (offset < fileData.length()) {
            // Send next chunk
            int chunkSize = (fileData.length() - offset < 100) ? (fileData.length() - offset) : 100;
            String chunk = fileData.substring(offset, offset + chunkSize);
            fileChunkChar.writeValue(chunk.c_str());
            offset += chunkSize;
            vTaskDelay(pdMS_TO_TICKS(100)); // Throttle notifications
            bluetoothSleepReady.put(false);
          } else {
            // Transfer complete, send checksum for verification
            Serial.println("Transfer complete. Waiting for checksum verification...");
            statusChar.writeValue("TRANSFER_COMPLETE");
            checksumChar.writeValue(String(calculatedChecksum));
            state = 4;
          }
        } else {
          // Disconnected during transfer
          Serial.println("Bluetooth disconnected during transfer");
          
          // Reset transfer state variables
          offset = 0;
          requestedFile = "";
          calculatedChecksum = 0;
          transferComplete = false;
          
          // Clear loaded file to free memory
          bluetoothFileManager.clearFile();
          
          // Return to advertise state
          state = 1;
          vTaskPrioritySet(NULL, originalPriority);
        }
      }

      else if(state == 4) {//VERIFY
        if (BLE.connected()) {
          // Check if client sent back checksum
          if (checksumChar.written()) {
            String checksumStr = checksumChar.value();
            
            // Convert string to uint32_t safely
            char* endPtr;
            receivedChecksum = strtoul(checksumStr.c_str(), &endPtr, 10);
            
            // Check if conversion was successful
            if (*endPtr == '\0') {
              Serial.print("Received checksum: ");
              Serial.println(receivedChecksum);
              
              if (receivedChecksum == calculatedChecksum) {
                Serial.println("Checksum verified! Transfer successful.");
                statusChar.writeValue("TRANSFER_SUCCESS");
                transferComplete = true;
                state = 2;
              } else {
                Serial.println("Checksum mismatch! Restarting transfer...");
                statusChar.writeValue("RETRY_TRANSFER");
                // Reset for retry
                offset = 0;
                requestedFile = "";
                calculatedChecksum = 0;
                transferComplete = false;
                state = 2;
              }
            } else {
              Serial.println("Invalid checksum format received");
              statusChar.writeValue("RETRY_TRANSFER");
              // Reset for retry
              offset = 0;
              requestedFile = "";
              calculatedChecksum = 0;
              transferComplete = false;
              state = 2;
            }
          }
        } else {
          Serial.println("Bluetooth disconnected during verify");
          
          // Reset transfer state variables
          offset = 0;
          requestedFile = "";
          calculatedChecksum = 0;
          transferComplete = false;
          
          // Clear loaded file to free memory
          bluetoothFileManager.clearFile();
          
          // Return to advertise state
          state = 1;
          vTaskPrioritySet(NULL, originalPriority);
        }
      }

      else if(state == 5) {//ERROR
        Serial.println("BLE Error state - sending error message to client");
        statusChar.writeValue("BLE_ERR");
        
        // Reset file transfer state variables
        requestedFile = "";
        offset = 0;
        calculatedChecksum = 0;
        receivedChecksum = 0;
        transferComplete = false;
        
        // Clear any loaded file
        bluetoothFileManager.clearFile();
        
        // Send error message to client
        statusChar.writeValue("Error: File transfer failed");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a bit before returning to connected state
        state = 2;
        bluetoothSleepReady.put(false);
      }
    
    
    // Handle BLE events more frequently for better responsiveness
    BLE.poll();
    
    // Delay for better BLE responsiveness while reducing CPU usage
    vTaskDelay(pdMS_TO_TICKS(15));
    bluetoothCheck.put(true);
  }
}