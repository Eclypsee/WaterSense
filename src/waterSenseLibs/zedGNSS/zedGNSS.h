/**
 * @file zedGNSS.h
 * 
 */

#include <SPI.h> 
#include <Wire.h> 
#include <SD.h>
#include <iostream> 
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "sharedData.h"

#define fileBufferSize 16384 // Allocate 16KBytes of RAM for UBX message storage 



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
        String getDisplayTime(void);
        SFE_UBLOX_GNSS getGNSS();
};

void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct);
void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct);