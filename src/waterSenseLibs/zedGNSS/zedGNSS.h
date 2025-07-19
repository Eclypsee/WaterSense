/**
 * @file zedGNSS.h
 * 
 */


#ifndef ZED_GNSS_H
#define ZED_GNSS_H

#include <SPI.h> 
#include <Wire.h> 
#include <iostream> 
#include <SparkFun_u-blox_GNSS_v3.h>
#include "sharedData.h"

#define fileBufferSize 20000 ///< Allocate 20KBytes of RAM (max buffer size 22.3kB) for UBX message storage  

class GNSS
{
    protected:
        // Protected data
        int sda, scl, clk;

    public:
        // Public data
        SFE_UBLOX_GNSS gnss;
        GNSS(int sda, int scl, int clk);
        void start();
        void start_no_survey();
        void getGNSSData();
        void setDisplayTime();
};

void newSFRBX(UBX_RXM_SFRBX_data_t *ubxDataStruct);
void newRAWX(UBX_RXM_RAWX_data_t *ubxDataStruct);

#endif //ZED_GNSS_H