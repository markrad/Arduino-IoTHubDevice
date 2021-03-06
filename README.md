# Arduino-IoTHubDevice
A thin C++ class to wrap the Azure IoT SDK.

The purpose is to simplify interaction with an Azure IoT hub from an Arduino based device. This code has only been tested on the ESP32 but should work on an ESP8266 also.

It is not a complete implementation. Currently supported features are:
* Device to cloud messages
* Cloud to device messages
* Direct messages with the ability to specify a specific function for a specific method name
* Device twin messages
* SDK debug logging can be enabled
* Provides access to the Azure IoT SDK version
* Parses device identity and hub name from the connection string and provides functions to acquire them
* Allows for simply sending a string as a message
* Allows creation of a message and attaching of custom properties etc.
* All callbacks can be passed to the class instance 

Using the Arduino libraries that utilize MbedTLS then the following are available:
* X.509 authentication
* Provide trusted certificates for server validation

An example sketch is provided in the examples subdirectory.

This library depends upon the Azure IoT libraries:
* AzureIoTHub
* AzureIoTProtocol_MQTT
* AzureIoTProtocol_HTTP
* AzureIoTUtility
* One of the socket layers (WIP)
