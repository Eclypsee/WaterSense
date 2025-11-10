/**
 * @file taskVoltage.cpp
 * @author Evan Lee
 * @brief Main file for the voltage measurement task
 * @version 0.1
 * @date 2023-02-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <Arduino.h>
#include "taskVoltage.h"
#include "setup.h"
#include "sharedData.h"
#include "Adafruit_MAX1704X.h"


/**
 * @brief The voltage task
 * @details Checks solar panel voltage and sets duty cycle
 * 
 * @param params A pointer to task parameters
 */
//MAX17048_I2CADDR_DEFAULT 0x36 ///< MAX17048 default i2c address
void taskVoltage(void* params)
{
  // Task Setup
  uint8_t state = 0;
  Adafruit_MAX17048 maxlipo;
  // Task Loop
  while (true)
  {
    // Measure voltage
    if(state==0){
      if(wakeReady.get()){
        Serial.println(F("\nAdafruit MAX17048 simple demo"));
        while (!maxlipo.begin(&Wire)) {
          Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
          delay(1000);
        }
        Serial.print(F("Found MAX17048"));
        Serial.print(F(" with Chip ID: 0x")); 
        Serial.println(maxlipo.getChipID(), HEX);
        state = 1;
      }
    }
    if (state == 1)
    {
      float cellVoltage = maxlipo.cellVoltage();
      if (isnan(cellVoltage)) {
        Serial.println("Failed to read cell voltage, check battery is connected!");
        delay(2000);
        return;
      }
      Serial.print(F("Batt Voltage: ")); Serial.print(cellVoltage, 3); Serial.println(" V");
      Serial.print(F("Batt Percent: ")); Serial.print(maxlipo.cellPercent(), 1); Serial.println(" %");
      Serial.println();
      battery.put(cellVoltage);
      batteryPercent.put(maxlipo.cellPercent());
    }
    
    voltageCheck.put(true);
    vTaskDelay(VOLTAGE_PERIOD);
  }
}