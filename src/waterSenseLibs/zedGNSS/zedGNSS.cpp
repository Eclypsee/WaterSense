/**
 * @file zedGNSS.cpp
 * @author Armaan Oberai and Toma Grundler
 * @version 0.1
 * @date 2024-07-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "zedGNSS.h"

GNSS :: GNSS(int sda, int scl, int clk) {
  this->sda = sda;
  this->scl = scl;
  this->clk = clk;
}

void GNSS :: start() {
    //gnss.enableDebugging();
    gnss.setFileBufferSize(fileBufferSize);
    //Serial.printf("File Buffer Size: %zu", gnss.fileBufferAvailable());
    while (gnss.begin(Wire, 0x42) == false) // Connect to the u-blox module using Wire port 
    { 
      Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing.")); 
    }

    gnss.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise) 
    gnss.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); // Save (only) the communications port settings to flash and BBR
    gnss.setNavigationFrequency(1); // Produce one navigation solution per second (that's plenty for Precise Point Positioning) 
    Serial.println("Nav freq set");
    gnss.setAutoRXMSFRBXcallbackPtr(&newSFRBX); // Enable automatic RXM SFRBX messages with callback to newSFRBX 
    gnss.logRXMSFRBX(); // Enable RXM SFRBX data logging 
    Serial.println("log RXM");
    gnss.setAutoRXMRAWXcallbackPtr(&newRAWX); // Enable automatic RXM RAWX messages with callback to newRAWX 
    gnss.logRXMRAWX(); // Enable RXM RAWX data logging 
    gnss.setHighPrecisionMode();
    Serial.println("high precision mode");
    gnss.getTimeDOP();
    gnss.saveConfiguration();
    Serial.println("Time DOP");
    

    Serial.println("Getting Unix Epoch...");
    unixTime.put(gnss.getUnixEpoch());
    Serial.println("Got Unix Epoch");

    Serial.println("Setting display time...");
    setDisplayTime();
    Serial.println("Set display time");

    Serial.println("Getting altitude...");
    altitude.put(gnss.getAltitude());
    Serial.println("Got altitude");

    Serial.println("Getting latitude...");
    latitude.put(gnss.getLatitude());
    Serial.println("Got latitude");

    Serial.println("Getting longitude...");
    longitude.put(gnss.getLongitude());
    Serial.println("Got longitude");

    Serial.println("Getting fix type...");
    fixType.put(gnss.getGnssFixOk());
    Serial.println("Got fix type");
    Serial.printf("GNSS successfully initialized Fix Type: %hhu Good Fix: %d\n", fixType.get(), wakeReady.get());
}

void GNSS :: start_no_survey() {
  while (gnss.begin(Wire, 0x42) == false) // Connect to the u-blox module using Wire port 
  { 
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing.")); 
  }

  gnss.setI2COutput(COM_TYPE_UBX); // Set the I2C port to output UBX only (turn off NMEA noise) 
  gnss.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); // Save (only) the communications port settings to flash and BBR
  gnss.getTimeDOP();
  gnss.saveConfiguration();
  Serial.println("Time DOP");
  

  unixTime.put(gnss.getUnixEpoch());
  fixType.put(gnss.getGnssFixOk());
  Serial.printf("GNSS successfully initialized Fix Type: %hhu Good Fix: %d\n", fixType.get(), wakeReady.get());
}

void GNSS :: getGNSSData() {
        unixTime.put(gnss.getUnixEpoch());
        if(gnss.checkUblox() == false) {
          return;
        }
        if(gnss.fileBufferAvailable() >= (sdWriteSize)) {
              gnss.extractFileBufferData(myBuffer, sdWriteSize); // Extract exactly sdWriteSize bytes from the UBX file buffer and put them into myBuffer
              gnssDataReady.put(true);
              // for(int i = 0; i < sdWriteSize; i++) {
              //   writeBuffer.put(myBuffer[i]);
              //   unixTime.put(gnss.getUnixEpoch());
              // }
              Serial.println("GNSS Buffer populated in queue");
              gnss.checkUblox(); // Check for the arrival of new data and process it. 
              return;
        }
        return;
}



void GNSS :: setDisplayTime() {
  String s = String(unixTime.get());
  displayTime.put(s);
}

void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct) 
{ 
  numSFRBX.put(numSFRBX.get() + 1); // Increment the count 
} 

void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct) 
{ 
  numRAWX.put(numRAWX.get() + 1); // Increment the count 
}

void printPVTdata(UBX_NAV_PVT_data_t *ubxDataStruct)
{
    Serial.println();

    Serial.print(F("Time: ")); // Print the time
    uint8_t hms = ubxDataStruct->hour; // Print the hours
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F(":"));
    hms = ubxDataStruct->min; // Print the minutes
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F(":"));
    hms = ubxDataStruct->sec; // Print the seconds
    if (hms < 10) Serial.print(F("0")); // Print a leading zero if required
    Serial.print(hms);
    Serial.print(F("."));
    unsigned long millisecs = ubxDataStruct->iTOW % 1000; // Print the milliseconds
    if (millisecs < 100) Serial.print(F("0")); // Print the trailing zeros correctly
    if (millisecs < 10) Serial.print(F("0"));
    Serial.print(millisecs);

    long latitude = ubxDataStruct->lat; // Print the latitude
    Serial.print(F(" Lat: "));
    Serial.print(latitude);

    long longitude = ubxDataStruct->lon; // Print the longitude
    Serial.print(F(" Long: "));
    Serial.print(longitude);
    Serial.print(F(" (degrees * 10^-7)"));

    long altitude = ubxDataStruct->hMSL; // Print the height above mean sea level
    Serial.print(F(" Height above MSL: "));
    Serial.print(altitude);
    Serial.println(F(" (mm)"));
}

