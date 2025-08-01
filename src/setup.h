/**
 * @file setup.h
 * @author Evan Lee
 * @brief A file to contain all setup information
 * @version 0.1
 * @date 2025 peak
 * 
 * @copyright Copyright (c) 2023
 * 
 */

//-----------------------------------------------------------------------------------------------------||
//---------- Define Constants -------------------------------------------------------------------------||

/**
 * @brief Define this constant to enable continuous measurements
 * @details Writes data to the SD card at the specified read intervals but does not sleep
 * 
 */
#define CONTINUOUS


/**
 * @brief Define the pins for the DEFAULT i2c bus
 * @details I know radar has its own separate scl_radar... ignore that. let it have its own bus
 * 
 */
#define SCL 4 
#define SDA 3 
#define SCL2 12 
#define SDA2 13 
#define CLK 100000

#define MAX_FILESIZE 50*1024 //max size a file can be in kb
#define BT_TRANSF_SIZE 64*1024 //max size a file can be for bluetooth to transfer

/**
 * @brief Define this constant to enable variable duty cycle
 * @details If undefined, HI_READ and HI_ALLIGN are used
 * 
 */
//#define VARIABLE_DUTY ///< Define this constant to enable variable duty cycle
//----------------------||
#define HI_READ 60*5 //||
#define MID_READ 60*2 //||
#define LOW_READ 60*1 //||
//                    //||
#define HI_ALLIGN 10 //||
#define MID_ALLIGN 30 //||
#define LOW_ALLIGN 60 //||
//----------------------||

#define GNSS_READ_TIME 60 * 60 * 20 //in seconds. right nowit is 72,000 or 20 hours awake

#define GNSS_STANDALONE_SLEEP (uint64_t) 60 * 1000000///<us of sleep time

#define WAKE_CYCLES 15 ///< Number of wake cycles between reset checks
#define FIX_DELAY 60*2 ///< Seconds to wait for first GPS fix
// #define FIX_DELAY 1 ///< Seconds to wait for first GPS fix

#define WATCH_TIMER 30*1000 ///< ms of hang time before triggering a reset

#define MEASUREMENT_PERIOD 100 ///< Measurement task period in ms
#define SD_PERIOD 10 ///< SD task period in ms
#define CLOCK_PERIOD 100 ///< Clock task period in ms
#define SLEEP_PERIOD 100 ///< Sleep task period in ms
#define VOLTAGE_PERIOD 1000 ///< Voltage task period in ms
#define WATCHDOG_PERIOD 100 ///< Watchdog task period in ms
#define RADAR_TASK_PERIOD 100

// #define R1b 9.54 ///< Larger resistor for battery voltage divider
// #define R2b 2.96 ///< Smaller resistor for battery voltage divider
#define R1b 9.25 ///< Larger resistor for battery voltage divider
#define R2b 3.3 ///< Smaller resistor for battery voltage divider

#define R1s 10.0 ///< Resistor for solar panel voltage divider
#define R2s 10.0 ///< Resistor for solar panel voltage divider

#define sdWriteSize 8192 ///<Write data to the SD card in blocks of 8192 bytes

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Define Pins ------------------------------------------------------------------------------||

#define SD_CS GPIO_NUM_5 ///< SD card chip select pin
//miso mosi are default

#define ADC_PIN GPIO_NUM_6

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||