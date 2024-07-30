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

#define SCL 22 
#define SDA 21 
#define CS 5 
#define CLK 400000
#define fileBufferSize 16384 // Allocate 16KBytes of RAM for UBX message storage 


class GNSS
{
    protected:
        // Protected data
        SFE_UBLOX_GNSS gnss;

    public:
        // Public data
        void begin();
    
    void begin(void);
    String getDisplayTime(void);
};