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

#include "waterSenseLibs/zedGNSS/zedGNSS.h"

 

#define SCL 22 

#define SDA 21 

#define CS 5 

#define CLK 400000

 

File dataFile; 

#define sdWriteSize 512      // Write data to the SD card in blocks of 512 bytes 

#define fileBufferSize 16384 // Allocate 16KBytes of RAM for UBX message storage 

uint8_t *myBuffer;           // Use myBuffer to hold the data while we write it to SD card 

 

unsigned long lastPrint; // Record when the last Serial print took place 

unsigned char receivedChar; 

 

int numSFRBX = 0; // Keep count of how many SFRBX message groups have been received (see note above) 

int numRAWX = 0; 

int numWrite = 0; // Keep count of how many times we have written to the SD card 

int numFile = 0;  // Keep count of how many times we have written to the SD card 

 

void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct) 

{ 

  numSFRBX++; // Increment the count 

} 

 

void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct) 

{ 

  numRAWX++; // Increment the count 

} 

 



 

void setup() 

{

 

  // Serial.println(F("u-blox NEO-M8P-2 base station example")); 

  // Wire.begin(); 

 

  Wire.begin(SDA, SCL); 

 

  Wire.setClock(CLK); 

 

  /////////////////////////////////////////////////////////// Raw data recording setup /////////////////////////////////////////////////////////////////////////////////// 

 

  while (Serial.available()) // Make sure the Serial buffer is empty 

  { 

    Serial.read(); 

  } 

 

  // Serial.println(F("Press any key to start logging.")); 

  // while (!Serial.available()) // Wait for the user to press a key 

  // { 

  //   ; // Do nothing 

  // } 

 

  delay(100); // Wait, just in case multiple characters were sent 

 

  while (Serial.available()) // Empty the Serial buffer 

  { 

    Serial.read(); 

  } 

 

  gnss.disableUBX7Fcheck(); 

 

  gnss.setFileBufferSize(fileBufferSize); 

 

  if (gnss.begin() == false) // Connect to the u-blox module using Wire port 

 

  { 

 

    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing.")); 

 

    while (1) 

      ; 

  } 

 

  gnss.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise) 

 

  gnss.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); // Save (only) the communications port settings to flash and BBR 

 

  gnss.setNavigationFrequency(1); // Produce one navigation solution per second (that's plenty for Precise Point Positioning) 

 

  gnss.setAutoRXMSFRBXcallbackPtr(&newSFRBX); // Enable automatic RXM SFRBX messages with callback to newSFRBX 

 

  gnss.logRXMSFRBX(); // Enable RXM SFRBX data logging 

 

  gnss.setAutoRXMRAWXcallbackPtr(&newRAWX); // Enable automatic RXM RAWX messages with callback to newRAWX 

 

  gnss.logRXMRAWX(); // Enable RXM RAWX data logging 

 

  gnss.setHighPrecisionMode(); 

 

  gnss.getTimeDOP();
  

 

  Serial.println("Initializing SD card..."); 

 

  // See if the card is present and can be initialized: 

 

  if (!SD.begin(CS)) 

 

  { 

 

    Serial.println("Card failed, or not present. Freezing..."); 

 

    // don't do anything more: 

 

    while (1) 

      ; 

  } 

 

  Serial.println("SD card initialized."); 

 

  // Create or open a file called "RXM_RAWX.ubx" on the SD card. 

 

  // If the file already exists, the new data is appended to the end of the file. 

 

  createGNSSFile(); 

 

  myBuffer = new uint8_t[sdWriteSize]; // Create our own buffer to hold the data while we write it to SD card 

 

  Serial.println(F("Press x to stop logging.")); 

 

  lastPrint = millis(); // Initialize lastPrint 

 

  while (Serial.available()) // Empty the Serial buffer 

 

  { 

 

    Serial.read(); 

  } 

} 

 

void loop() 

 

{ 

 

  gnss.checkUblox(); // See if new data is available. Process bytes as they come in. 

 

  gnss.checkCallbacks(); 

 

  while (gnss.fileBufferAvailable() >= sdWriteSize) // Check to see if we have at least sdWriteSize waiting in the buffer 

 

  { 

 

    gnss.extractFileBufferData(myBuffer, sdWriteSize); // Extract exactly sdWriteSize bytes from the UBX file buffer and put them into myBuffer 

 

    dataFile.write(myBuffer, sdWriteSize); // Write exactly sdWriteSize bytes from myBuffer to the ubxDataFile on the SD card 

 

    numWrite++; // Increment the write counter 

 

    // In case the SD writing is slow or there is a lot of data to write, keep checking for the arrival of new data 

 

    gnss.checkUblox(); // Check for the arrival of new data and process it. 

 

    gnss.checkCallbacks(); // Check if any callbacks are waiting to be processed. 

  } 

 

  if (numWrite > 0 && numWrite > 100 && !gnss.fileBufferAvailable()) // Every 100 writes, close the file and open a new one 

 

  { 

 

    dataFile.close(); // Close the data file 

    createFile();     // Create a new data file 

    numWrite = 0;     // Reset the write counter 

  } 

 

  if (millis() > (lastPrint + 1000)) // Print the message count once per second 

 

  { 

 

    Serial.print(F("Number of message groups received: SFRBX: ")); // Print how many message groups have been received (see note above) 

 

    Serial.print(numSFRBX); 

 

    Serial.print(F(" RAWX: ")); 

 

    Serial.println(numRAWX); 

 

    Serial.print(F("Num of writes: ")); // Print how many times we have written to the SD card 

 

    Serial.print(numSFRBX); 

 

    Serial.print(F("Num of Wakeups: ")); // Print how many times we have written files to the SD card 

 

    Serial.println(wakeCounter); 

 

    uint16_t maxBufferBytes = gnss.getMaxFileBufferAvail(); // Get how full the file buffer has been (not how full it is now) 

 

    // Serial.print(F("The maximum number of bytes which the file buffer has contained is: ")); // It is a fun thing to watch how full the buffer gets 

 

    // Serial.println(maxBufferBytes); 

 

    if (maxBufferBytes > ((fileBufferSize / 5) * 4)) // Warn the user if fileBufferSize was more than 80% full 

 

    { 

 

      Serial.println(F("Warning: the file buffer has been over 80% full. Some data may have been lost.")); 

    } 

 

    lastPrint = millis(); // Update lastPrint 

  } 

 

  if (Serial.available()) // Check if the user wants to stop logging 

 

  { 

 

    Serial.println(F("Logging terminated")); 

 

    uint16_t remainingBytes = gnss.fileBufferAvailable(); // Check if there are any bytes remaining in the file buffer 

 

    while (remainingBytes > 0) // While there is still data in the file buffer 

 

    { 

 

      uint16_t bytesToWrite = remainingBytes; // Write the remaining bytes to SD card sdWriteSize bytes at a time 

 

      if (bytesToWrite > sdWriteSize) 

 

      { 

 

        bytesToWrite = sdWriteSize; 

      } 

 

      gnss.extractFileBufferData(myBuffer, bytesToWrite); // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer 

 

      dataFile.write(myBuffer, bytesToWrite); // Write bytesToWrite bytes from myBuffer to the ubxDataFile on the SD card 

 

      remainingBytes -= bytesToWrite; // Decrement remainingBytes 

    } 

 

    dataFile.close(); // Close the data file 

  } 

 

  delay(250); // Don't pound too hard on the I2C bus 

} 

 