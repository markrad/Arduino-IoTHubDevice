# Arduino-IoTHubDevice Examples

These examples were executed on a [Adafruit HUZZAH32 ESP32 Feather Board](https://www.adafruit.com/product/3405). The example that uses Ethernet used the [Adafruit Ethernet FeatherWing](https://www.adafruit.com/product/3201).

You will need to provide your SSID and password for the Wi-Fi version. The code expects to find the trusted certificates loaded into SPIFFS. For the ESP32, this can be accomplished with the [Arduino ESP32 filesystem uploader](https://github.com/me-no-dev/arduino-esp32fs-plugin). The current version of the trusted certificates can be found in the data subdirectory.

To use X.509 authentication, you will also need to add the X.509 certificate and key to the SPIFFS file system and modify the defines to reflect their file names.