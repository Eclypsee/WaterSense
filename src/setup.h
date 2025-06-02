/**
 * @file setup.h
 * @author Alexander Dunn
 * @brief A file to contain all setup information
 * @version 0.1
 * @date 2023-02-05
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
 * @brief Define this constant to enable standalone GNSS measurements (no sonar or temp)
 * @details Writes data to the SD card with minimal sleep time
 * 
 */
//#define STANDALONE

/**
 * @brief Define this constant to disable GNSS measurements (only used as clock)
 * @details Writes data to the SD card with minimal sleep time
 * 
 */
#define NO_SURVEY

/**
 * @brief Define this constant to enable v3 Adafruit Ultimate Breakout GPS (no sonar or temp)
 * @details enables taskClock2
 */
//#define LEGACY

/**
 * @brief Define this constant to enable Radar instead of Ultrasonic
 * @details enables taskRadar
 * 
 */

#define RADAR

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

#define GNSS_READ_TIME 60 * 60 * 2

#define GNSS_STANDALONE_SLEEP (uint64_t) 60 * 1000000///<us of sleep time

#define WAKE_CYCLES 15 ///< Number of wake cycles between reset checks
#define FIX_DELAY 60*2 ///< Seconds to wait for first GPS fix
// #define FIX_DELAY 1 ///< Seconds to wait for first GPS fix

#define WATCH_TIMER 15*1000 ///< ms of hang time before triggering a reset

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

#define sdWriteSize 8192 ///<cWrite data to the SD card in blocks of 512 bytes

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||










//-----------------------------------------------------------------------------------------------------||
//---------- Define Pins ------------------------------------------------------------------------------||

#define LED GPIO_NUM_2

#define SD_CS GPIO_NUM_5 ///< SD card chip select pin

#define SONAR_RX GPIO_NUM_14 ///< Sonar sensor receive pin
#define SONAR_TX GPIO_NUM_32 ///< Sonar sensor transmit pin

/**
 * @brief Sonar sensor enable pin
 * @details Sonar measurements are disbale when this pin is pulled low
 * 
 */
#define SONAR_EN GPIO_NUM_33

#define GPS_RX GPIO_NUM_16 ///< GPS receive pin
#define GPS_TX GPIO_NUM_17 ///< GPS transmit pin

/**
 * @brief GPS enable pin
 * @details GPS measurements are disabled when this pin is pulled low
 * 
 */
#define GPS_EN GPIO_NUM_27

#define TEMP_SENSOR_ADDRESS 0x44 ///< Temperature and humidity sensor hex address

/**
 * @brief Temperature/humidity sensor enable pin
 * @details Temperature and humidity measurements are disabled when this pin is pulled low
 * 
 */
#define TEMP_EN GPIO_NUM_15

#define ADC_PIN GPIO_NUM_26

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||