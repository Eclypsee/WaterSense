/**
 * @file bluetooth.h
 * @author Evan Lee
 * @brief Header file for Bluetooth file management library
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>
#include <SdFat.h>


class BluetoothFileManager {
private:
    String fileList;
    String currentFileData;
    String currentFileName;
    bool fileLoaded;
    uint32_t currentChecksum;
    
    /**
     * @brief Get file list from SD card
     * @return String Comma-separated list of files
     */
    String getFileListFromSD();
    
    /**
     * @brief Load file data from SD card
     * @param fileName Name of file to load
     * @return bool True if file loaded successfully
     */
    bool loadFileFromSD(const String& fileName);
    
    /**
     * @brief Calculate checksum for given data
     * @param data String data to calculate checksum for
     * @return uint32_t Checksum value
     */
    uint32_t calculateChecksum(const String& data);

public:
    /**
     * @brief Constructor
     */
    BluetoothFileManager();
    
    /**
     * @brief Initialize the file manager
     * @return bool True if initialization successful
     */
    bool begin();
    
    /**
     * @brief Get the current file list
     * @return String Current file list
     */
    String getFileList();
    
    /**
     * @brief Load a specific file
     * @param fileName Name of file to load
     * @return bool True if file loaded successfully
     */
    bool loadFile(const String& fileName);
    
    /**
     * @brief Get the currently loaded file data
     * @return String File data
     */
    String getFileData();
    
    /**
     * @brief Get the currently loaded file name
     * @return String File name
     */
    String getCurrentFileName();
    
    /**
     * @brief Check if a file is currently loaded
     * @return bool True if file is loaded
     */
    bool isFileLoaded();
    
    /**
     * @brief Get the checksum of the currently loaded file
     * @return uint32_t Checksum value
     */
    uint32_t getCurrentChecksum();
    
    /**
     * @brief Refresh the file list from SD card
     */
    void refreshFileList();
    
    /**
     * @brief Clear the currently loaded file data
     */
    void clearFile();
    
    /**
     * @brief Generate file list as text file
     * @return bool True if file list generated successfully
     */
    bool generateFileList();
};

// Global instance
extern BluetoothFileManager bluetoothFileManager;

#endif // BLUETOOTH_H 