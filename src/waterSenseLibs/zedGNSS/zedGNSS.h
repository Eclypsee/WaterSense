/**
 * @file zedGNSS.h
 * 
 */


#ifndef ZED_GNSS_H
#define ZED_GNSS_H

#include <SPI.h> 
#include <Wire.h> 
#include <SD.h>
#include <iostream> 
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "sharedData.h"

#define fileBufferSize 16384 ///< Allocate 16KBytes of RAM for UBX message storage  

class GNSS
{
    protected:
        // Protected data
        SFE_UBLOX_GNSS gnss;
        int sda, scl, clk;

    public:
        // Public data
        GNSS(int sda, int scl, int clk);
        void begin();
        void stopLogging();
        String getDisplayTime();
        SFE_UBLOX_GNSS getGNSS();
        void powerSaveSelect(bool on);
};

void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct);
void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct);

#endif //ZED_GNSS_H