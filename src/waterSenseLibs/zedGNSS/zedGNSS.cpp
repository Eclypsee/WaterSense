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

void GNSS :: begin() {
    Wire.begin(this->sda, this->scl); 
    Wire.setClock(this->clk);
    while (Serial.available()) // Make sure the Serial buffer is empty 
    { 

        Serial.read(); 

    } 
    // Serial.println(F("Press any key to start logging.")); 
    // while (!Serial.available()) // Wait for the user to press a key 
    // { 
    //   ; // Do nothing 
    // };

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
        while (1); 
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
    
    unixTime.put(gnss.getUnixEpoch());
    displayTime.put(this->getDisplayTime());
    altitude.put(gnss.getAltitude());
    latitude.put(gnss.getLatitude());
    longitude.put(gnss.getLongitude());
    fixType.put(gnss.getFixType());
    wakeReady.put(gnss.getGnssFixOk());
}

void GNSS :: stopLogging() {
  gnss.logRXMSFRBX(false);
  gnss.logRXMRAWX(false);
}

void GNSS :: powerSaveSelect(bool on) {
  gnss.powerSaveMode(on);
}

String GNSS :: getDisplayTime() {
  char buffer[30]; // Buffer to hold the formatted string
  snprintf(buffer, sizeof(buffer), "%02u %02u %04u   %02u:%02u:%02u:%03u",
             gnss.getMonth(), gnss.getDay(), gnss.getYear(),
             gnss.getHour(), gnss.getMinute(), gnss.getSecond(), gnss.getMillisecond());
  return buffer;
}

SFE_UBLOX_GNSS GNSS :: getGNSS() {
  return gnss;
}


void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct) 
{ 
  numSFRBX.put(numSFRBX.get() + 1); // Increment the count 
} 

void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct) 
{ 
  numRAWX.put(numRAWX.get() + 1); // Increment the count 
} 
