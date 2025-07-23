/**
 * @file sdData.cpp
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include "sharedData.h"
#include <SdFat.h>
#include <utility>
#include "sdData.h"
SdFat SD;
/**
 * @brief A constructor for the SD_Data class
 * 
 * @param pin The pin used for the SD chip select
 * @return SD_Data 
 */
SD_Data :: SD_Data(gpio_num_t pin)
{
    // Update CS pin
    CS = pin;

    //pinMode(LED, OUTPUT);

    // Start SD stuff
    pinMode(CS, OUTPUT);

    // uint16_t longTimer = millis();
    uint16_t timer = millis();
    while (!SD.begin(SD_CS, SD_SCK_MHZ(10)))
    {
        // // Restart if more than 10 seconds
        // if ((millis() - longTimer) > 10000)
        // {
        //     assert(false);
        // }

        // // Blink LED
        // Serial.println("SD not found, blinking LED");
        // if ((millis() - timer) > 300)
        // {
        //     timer = millis();
        //     digitalWrite(LED, 1-digitalRead(LED));
        // }
    }
    //digitalWrite(LED, LOW);
}

String SD_Data :: getGNSSFilePath() {
    return GNSSFilePath;
}

String SD_Data :: getDataFilePath() {
    return DataFilePath;
}

/**
 * @brief A method to check and write header files to the SD card
 * 
 */
void SD_Data :: writeHeader()
{
    // Check if file exists and create one if not
    if (!SD.exists("/README.txt"))
    {
        ExFile read_me = SD.open("/README.txt", O_RDWR | O_CREAT | O_TRUNC);
        if(!read_me) return;

        // Create header with title, timestamp, and column names
        read_me.println("");
        read_me.printf(
            "Cal Poly Tide Sensor Ver. 3, Now With Radar AND BLE :)\n"
            "https://github.com/Eclypsee/WaterSense\n\n"
            "Data File format:\n"
            "UNIX Time (GMT), Distance (mm), External Temp (F), Humidity (%), Battery Voltage (V), Solar Panel Voltage (V)\n"
            "Current Battery Voltage: %f V\n", battery.get());
        read_me.close();

        SD.mkdir("/Data");
    }
}

/**
 * @brief Open a new file
 * 
 * @param hasFix Whether or not the GPS has a fix
 * @param wakeCounter The number of wake cycles
 * @param time The current unix timestamp
 * @return The opened file
 */
ExFile SD_Data :: createFile(uint32_t time)
{
    String fileName = "/Data/";

    // Filenames are at most 8 characters + 6("/Data/") + 4(".txt") + null terminator = 19

    fileName += String(time, HEX);
    fileName += ".txt";

    ExFile file = SD.open(fileName.c_str(), O_RDWR | O_CREAT | O_TRUNC);
    this->DataFilePath = fileName;
    assert(file);
    return file;
}


ExFile SD_Data :: createGNSSFile() 

{ 
  // Create or open a file called "RXM_RAWX.ubx" on the SD card. 
  // If the file already exists, the new data is appended to the end of the file. 

 

  SD.mkdir("/GNSS_Data"); 

  String fileName = "/GNSS_Data/"; 

  fileName += String(wakeCounter, HEX); 

  fileName += "_"; 

  fileName += String(unixTime.get()); 

  fileName += ".ubx"; 
  
  this->GNSSFilePath = fileName;

  ExFile dataFile = SD.open(fileName.c_str(), O_RDWR | O_CREAT | O_TRUNC); 

  if (!dataFile) 

  { 

    Serial.println("Failed to create UBX data file! Freezing..."); 

  }
  
  return dataFile;

} 

/**
 * @brief A method to write a log message to the SD card
 * 
 * @param unixTime The unix timestamp
 * @param wakeCounter The number of times the MCU has woken from deep sleep
 * @param latitude The latitude as measured by the GPS
 * @param longitude The longitude as measured by the GPS
 * @param altitude The altitude as measured by the GPS
 */
void SD_Data :: writeLog(uint32_t unixTime, uint32_t wakeCounter, float latitude, float longitude, float altitude)
{
    //Open log file and write to it
    ExFile logFile = SD.open("/logFile.txt", O_RDWR | O_CREAT | O_TRUNC);
    if(!logFile) return;

    if (logFile.fileSize() == 0) {
        logFile.println("Wake Count, UNIX Time (GMT), Latitude (decimal degrees), Longitude (decimal degrees), Altitude (meters above MSL)");
    }
    logFile.printf("%u, %u, %0.5f, %0.5f, %0.2f\n", wakeCounter, unixTime, latitude, longitude, altitude);
    logFile.close();
}

/**
 * @brief A method to take a write data to the SD card
 * 
 * @param data_file A reference to the data file to be written to
 * @param distance The distance measured by the SONAR sensor
 * @param unixTime The unix timestamp for when the data was recorded
 * @param temperature The current temperature measured by the temperature and humidity sensor
 * @param humidity The current humidity measured by the temperature and humidity sensor
 * @param solarVoltage Voltage of solar panel
 * @return sensorData An object containing all of the data
 */
void SD_Data :: writeData(ExFile &dataFile, int32_t distance, uint32_t unixTime, float batteryVoltage, float solarVoltage)
{
    dataFile.print(unixTime);
    dataFile.printf(", %d, %0.2f, %0.2f, %0.2f, %0.2f\n", distance, batteryVoltage, solarVoltage);
}

/**
 * @brief A method to take a write GNSS data to the SD card
 * 
 * @param data_file A reference to the data file to be written to
 * @param buffer A reference to the block of data to be written to the .ubx file
 */
void SD_Data :: writeGNSSData(ExFile &dataFile, uint8_t buffer[SIZE])
{
    dataFile.write(buffer, SIZE);
}

/**
 * @brief A method to close the current file and put the device to sleep
 * 
 * @param dataFile The file to close
 */
void SD_Data :: sleep(ExFile &dataFile)
{
    dataFile.close();
}