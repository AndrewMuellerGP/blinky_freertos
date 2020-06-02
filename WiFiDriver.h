// Copyright (c) 2020 Confidential Information Georgia-Pacific Consumer Products
// Not for further distribution.  All rights reserved.

/**
 * Driver for the CC3135MOD Network Wifi Processor (NWP).  The driver utilizes
 * TI's SimpleLink driver to control
 * 
 */

/**
 * Start the SimpleLink library.
 *
 * @returns True if the SimpleLink library was started successfully.
 *          False if an error occured.
 */
bool WiFiDriver_StartSimpleLink();

/**
 * Initialize the WiFi driver.
 *
 * @returns True if the wifi driver was initialized successfully.  
 *          False if the wifi driver initialization failed.
 */
bool WiFiDriver_Init();

/**
 * Scan for available 2.4Ghz and 5Ghz channels
 *
 * TODO: optimize to only scan on a handful of channels to save time and power.
 *
 * @param scanSeconds The number of seconds to scan for Access Points
 * @param scanHidden True to scan for hidden access points, False to ignore them.
 */
void WiFiDriver_ScanStart(uint8_t scanSeconds, bool scanHidden);

/**
 * Turn off the scanning for access points.
 */
void WiFiDriver_ScanStop();

/**
 * Retreive the results of the Access Point Scan
 * NOTE: WiFiDriver_ScanStart() must have previously been called.
 *
 * @param numScanEntries The number of access points to search for.
 *
 * @return The number of access points found, or error code from the API command
 */
int16_t WiFiDriver_CollectScanResults(uint8_t numScanEntries);

/**
 * Connect to the specified SSID and Security Key
 *
 * @param ssid The WiFi access points SSID to connect to.
 * @param key The Security Key to use when connecting to the access points.
 *
 * @return True if the connection was successful.  False if the connection failed.
 */
bool WiFiDriver_Connect(const uint8_t* ssid, const uint8_t* key);

/**
 * Send the specified buffer of bytes out the existing wifi connection
 */
void WiFiDriver_Send(uint8_t* buffer);