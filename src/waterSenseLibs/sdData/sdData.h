/**
 * @file sdData.h
 * @author Alexander Dunn
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <Arduino.h>
#include <SdFat.h>
#include <utility>
#include "setup.h"

#define SIZE sdWriteSize

extern SdFat SD;
class SD_Data
{
    protected:
        // Protected data
        gpio_num_t CS;
        //gpio_num_t LED = GPIO_NUM_2;
        String GNSSFilePath = "";
        String DataFilePath = "";

    public:
        // Public data

        SD_Data(gpio_num_t pin); ///< A constructor for the SD_Data class

        String getGNSSFilePath();
        
        String getDataFilePath();

        void writeHeader(void); ///< A method to check and format header files

        /// A method to open a new file
        ExFile createFile(uint32_t time);

        // A method to write raw satellite data to .ubx files
        ExFile createGNSSFile();

        /// A method to write a log message
        void writeLog(uint32_t unixTime, uint32_t wakeCounter, float latitude, float longitude, float altitude);

        /// A method to write data to the sd card
        void writeData(ExFile &data_file, int32_t distance, uint32_t unixTime, float batteryVoltage, float solarVoltage);

        /// A method to write GNSS data to SD card
        void writeGNSSData(ExFile &dataFile, uint8_t buffer[SIZE]);

        void sleep(ExFile &dataFile); ///< A method to close the current file and put the device to sleep
};