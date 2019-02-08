#include <AzureIoTSocket_WiFi.h>

// To use X.509 authentication instead of a connection string uncomment the following define for X509TEST.
// In order for this to work you will need to have written the X.509 key and certificate to SPIFFS and provided
// the file names below in X509CERT_FILENAME and X509KEY_FILENAME. To test with the ESP32 I used the utility 
// at https://github.com/me-no-dev/arduino-esp32fs-plugin to upload the appropriate files.
//#define X509TEST

#include <SPIFFS.h>

#include <IoTHubDevice.h>
#include <IoTHubMessage.h>
#include <MapUtil.h>
#include <parson.h>

#define SSID "<Your Wi-Fi SSID>"
#define PASSWORD "<Your Wi-Fi password here or NULL for none>"

// This file is provided in the data subdirectory and can be uploaded with the Arduino ESP32 filesystem uploader. See link above.
#define TRUSTED_CERTS_FILENAME "/trusted.cert.pem"

// Connection string
#ifdef X509TEST

// Names of the files held in SPIFFS
#define X509CERT_FILENAME "<Your device X.509 certificate file name here in pem format>"
#define X509KEY_FILENAME "<Your device X.509 key file name here in pem format>"

static const char *CONNECTIONSTRING = "<X.509 version of the connection string. Should contain x509=true>";
#else
// Regular connection string authentication used

static const char *CONNECTIONSTRING = "<Regular connection string>";
#endif

// Azure IoT Hub information
WiFiClient client;

// Used to test reconnection logic
bool KillWiFi = false;

// Used to monitor network status
bool CheckWiFi = false;

// LED that is flashed each time a message is sent - you may need to change this
static const int LED = 13;

// IoT Hub
IoTHubDevice *deviceHandle = NULL;

// Message rate per minute - this example is limited to a maximum of 60 due to the manner in which it is timed 
static const int MESSAGESPERMIN = 20;
static int currentMessagesPerMinute = MESSAGESPERMIN;

// Message received callback
IOTHUBMESSAGE_DISPOSITION_RESULT messageCallback(IoTHubDevice &iotHubDevice, IoTHubMessage &iotHubMessage, void *userContext)
{
  Serial.println("Message received");

  IOTHUBMESSAGE_DISPOSITION_RESULT result = IOTHUBMESSAGE_ACCEPTED;
  IOTHUBMESSAGE_CONTENT_TYPE contentType = iotHubMessage.GetContentType();
  const uint8_t *buffer;
  size_t size;

  switch (contentType)
  {
    case IOTHUBMESSAGE_BYTEARRAY:
      if (IOTHUB_MESSAGE_OK == (iotHubMessage.GetByteArray(&buffer, &size)))
      {
        Serial.print("Byte array message received length: ");
        Serial.println((int)size);
        
        for (int i = 0; i < size; i++)
        {
          // Assume the message is text
          Serial.print((char)buffer[i]);
        }
        Serial.println();

        if (size == 4 && 0 == memcmp(buffer, "kill", 4))
        {
          // Test reconnection logic by shutting off Wi-Fi
          // Wi-Fi is not killed here because it won't be able to tell the hub the message has been 
          // received which will cause the hub to resend the message as soon as the link is back
          // up thus causing a kill/reconnect loop
          Serial.println("Killing WiFi");
          KillWiFi = true;
        }
      }
      else
      {
        Serial.println("Warning - failed to retrieve byte array message");
      }
      break;
    case IOTHUBMESSAGE_STRING:
      Serial.println(iotHubMessage.GetCString());
      break;
    default:
      Serial.println("Warning - content type unknown");
      result = IOTHUBMESSAGE_REJECTED;
      break;
  }

  return result;
}

// Message acknowledgement callback
void eventConfirmationCallback(IoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext)
{
  Serial.print("Message response - ");

  switch (result)
  {
    case IOTHUB_CLIENT_CONFIRMATION_OK:
      Serial.println("OK");
      break;
    case IOTHUB_CLIENT_CONFIRMATION_BECAUSE_DESTROY:
      Serial.println("Because destroy");
      break;
    case IOTHUB_CLIENT_CONFIRMATION_MESSAGE_TIMEOUT:
      Serial.println("Timeout");
      break;
    case IOTHUB_CLIENT_CONFIRMATION_ERROR:
      Serial.println("Error");
      break;
    default:
      Serial.println("Warning: unknown result");
      break;
  }
}

// Connection status callback
void connectionStatusCallback(IoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContext)
{
  Serial.print("Connection status changed - ");

  switch (result)
  {
    case IOTHUB_CLIENT_CONNECTION_AUTHENTICATED:
      Serial.print("Authenticated ");
      break;
    case IOTHUB_CLIENT_CONNECTION_UNAUTHENTICATED:
      Serial.print("unauthenticated ");
      break;
    default:
      Serial.print("Status unknown ");
      break;
  }

  switch (reason)
  {
    case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
      Serial.println("SAS token expired");
      break;
    case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
      Serial.println("Device disabled");
      break;
    case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
      Serial.println("Bad credentials");
      break;
    case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
      Serial.println("Retry expired");
      break;
    case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
      Serial.println("Network unavailable - Attempting to reconnect WiFi");
      CheckWiFi = true;
      break;
    case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
      Serial.println("Communication error");
      break;
    case IOTHUB_CLIENT_CONNECTION_OK:
      Serial.println("Connection OK");
      break;
    default:
      Serial.println("Reason unknown");
      break;
  }
}

// Specific call back for method name 'Test' - these are case sensitive
int deviceMethodCallback_Test(IoTHubDevice &iotHubDevice, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext)
{
  char *strPayload = (char *)malloc(size + 1);
  memcpy(strPayload, payload, size);
  strPayload[size] = '\0';
  
  const char *respStr = "{ ""result"" : 0 }";
  const int responseLen = strlen(respStr) + 1;
  
  Serial.println("Test invoked");
  Serial.println(strPayload);
  delete [] strPayload;

  *response = (unsigned char *)malloc(responseLen);
  *resp_size = responseLen;
  memcpy(*response, respStr, responseLen);
  
  return 200;
}

// Optional callback for unknown device method invocations
int unknownDeviceMethodCallback(IoTHubDevice &iotHubDevice, const char *methodName, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext)
{
  Serial.print("Unrecognized method called: ");
  Serial.println(methodName);

  int status = 501;
  const char* RESPONSE_STRING = "{ \"Response\": \"Unknown method requested.\" }";
  
  *resp_size = strlen(RESPONSE_STRING);
  
  if ((*response = (unsigned char *)malloc(*resp_size)) == NULL)
  {
    status = -1;
  }
  else
  {
    memcpy(*response, RESPONSE_STRING, *resp_size);
  }

  return status;
}

// Optional device twin callback
void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const char* payLoad, void* userContext)
{
  Serial.printf("Device Twin Callback: update_state=%S;payLoad=\r\n%s\r\n", ((update_state == DEVICE_TWIN_UPDATE_COMPLETE)? "Update Complete" : "Update Partial"), payLoad);
  JSON_Value *root_value = NULL;
  JSON_Object *root_object = NULL;
  JSON_Value* desired_messagePerMinute;

  root_value = json_parse_string(payLoad);
  root_object = json_value_get_object(root_value);

  if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
  {
    desired_messagePerMinute = json_object_dotget_value(root_object, "desired.messagePerMinute");
  }
  else
  {
    desired_messagePerMinute = json_object_dotget_value(root_object, "messagePerMinute");
  }

  if (desired_messagePerMinute != NULL)
  {
    currentMessagesPerMinute = json_value_get_number(desired_messagePerMinute); 
    Serial.printf("Modifying message rate to %d a minute (every %d seconds)\r\n", currentMessagesPerMinute, (60 / currentMessagesPerMinute));
    char *json = serializeToJson(currentMessagesPerMinute);
    deviceHandle->SendReportedState(json, reportedStateCallback);
    free(json);
  }
  else
  {
    Serial.println("No change to messagePerMinute");
  }
}

// Callback for reported state sends
void reportedStateCallback(IoTHubDevice &iotHubDevice, int status_code, void* userContext)
{
  Serial.print("Reported State result = ");
  Serial.println(status_code);
}

// Initialize the Wi-Fi - sample uses DHCP to acquire IP address, DNS server and Gateway
void initWiFi()
{
    // Attempt to connect to Wifi network:
    Serial.printf("Attempting to connect to SSID: %s.", SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(SSID, PASSWORD);
        Serial.print(".");
        delay(3000);
    }

    Serial.println();
    Serial.printf("Connected to wifi %s.\r\n", SSID);

    byte mac[6];
    
    WiFi.macAddress(mac);
    Serial.print("MAC: ");
    Serial.print(mac[5],HEX);
    Serial.print(":");
    Serial.print(mac[4],HEX);
    Serial.print(":");
    Serial.print(mac[3],HEX);
    Serial.print(":");
    Serial.print(mac[2],HEX);
    Serial.print(":");
    Serial.print(mac[1],HEX);
    Serial.print(":");
    Serial.println(mac[0],HEX);
}

// Uses parson library that is included in the SDK for JSON manipulation
char* serializeToJson(int messagesPerMinute)
{
    char* result;

    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);

    // Only reported properties:
    json_object_set_number(root_object, "messagePerMinute", messagesPerMinute);

    result = json_serialize_to_string(root_value);

    json_value_free(root_value);

    return result;
}

// Read a file from SPIFFS
uint8_t *readFile(const char *filename)
{
  File filehandle = SPIFFS.open(filename, "r");
  
  if (!filehandle)
  {
    Serial.printf("Failed to open %s\r\n", filename);
    errorSpin();
  }

  size_t filesize = filehandle.size() + 1;

  if (filesize < 2)
  {
    Serial.printf("Failed to get size for %s\r\n", filename);
    errorSpin();
  }

  uint8_t *filecontent = new uint8_t[filesize];

  if (filecontent == NULL)
  {
    Serial.printf("Failed to allocate memory for %s\r\n", filename);
    errorSpin();
  }

  size_t readbytes = filehandle.read(filecontent, filesize);

  *(filecontent + filesize - 1) = '\0';
  filehandle.close();
  Serial.printf("Read %d bytes from file %s\r\n", readbytes, filename);

  return filecontent;
}

// Called when a terminal error occurs
void errorSpin()
{
  long i = 0;

  while (true)
  {
    if (i++ % 20 == 0)
    {
       Serial.println("Error spin - unable to continue");
    }
    
    delay(1000);
  }
}

void setup() 
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Starting");
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  initWiFi();
  initTime();

  // Create IoT device
  if(!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    errorSpin();
  }

// Read trusted certificate - this is required to allow MbedTLS to validate the server certificate
  uint8_t *trustedCert = readFile(TRUSTED_CERTS_FILENAME);
  
#ifdef X509TEST
// Read X.509 certificate and key from SPIFFS
  uint8_t *X509cert = readFile(X509CERT_FILENAME);
  uint8_t *X509key = readFile(X509KEY_FILENAME);

  deviceHandle = new IoTHubDevice(CONNECTIONSTRING, (char *)X509cert, (char *)X509key, IoTHubDevice::Protocol::MQTT);
#else
// No X.509 authentication - use regular connection string
  deviceHandle = new IoTHubDevice(CONNECTIONSTRING, IoTHubDevice::Protocol::MQTT);
#endif

  if (0 != deviceHandle->Start())
  {
    Serial.println("Failed to start IoT device");
    errorSpin();
  }

  Serial.print("IoT SDK version = ");
  Serial.println(deviceHandle->GetVersion());
  Serial.print("Host Name = ");
  Serial.println(deviceHandle->GetHostName());
  Serial.print("Device ID = ");
  Serial.println(deviceHandle->GetDeviceId());

  // Set up event callbacks
  deviceHandle->SetMessageCallback(messageCallback, NULL);
  deviceHandle->SetConnectionStatusCallback(connectionStatusCallback, NULL);
  deviceHandle->SetDeviceMethodCallback("Test", deviceMethodCallback_Test, NULL);
  deviceHandle->SetUnknownDeviceMethodCallback(unknownDeviceMethodCallback, NULL);
  deviceHandle->SetDeviceTwinCallback(deviceTwinCallback, NULL);

  // Set logging state
  bool logging = false;
  deviceHandle->SetLogging(logging);

  // Pass trusted certificates
  deviceHandle->SetTrustedCertificate((const char *)trustedCert);
}

void loop() 
{
  time_t last = 0;
  time_t now;
  IOTHUB_CLIENT_RESULT result;
  int ledOffIn = 0;

  Serial.println("Sending device status");
  char *json = serializeToJson(currentMessagesPerMinute);
  deviceHandle->SendReportedState(json, reportedStateCallback);
  free(json);
  
  while (true)
  {
    if (ledOffIn > 0 && --ledOffIn == 0)
    {
      // Turn off LED after counter hits zero
      digitalWrite(LED, LOW);
    }
    
    // Perform reconnection test
    if (KillWiFi)
    {
      KillWiFi = false;
      
      WiFi.disconnect();
      delay(10);
    }

    // See if SDK has reported a network issue
    if (CheckWiFi)
    {
      CheckWiFi = false;
      WiFi.disconnect();
      initWiFi();
    }

    now = get_time(NULL);

    // Send a message periodically
    if ((60 / currentMessagesPerMinute) <= now - last)
    {
      // Don't keep sending messages if they are being queued to avoid running out of memory
      if (deviceHandle->WaitingEventsCount() > 5)
      {
        Serial.printf("No response from last %d messages\r\n", deviceHandle->WaitingEventsCount());
        if (WiFi.status() != WL_CONNECTED)
        {
          Serial.println("Restarting Wi-Fi");
          initWiFi();
        }
      }
      else
      {
        // Send the current epoch and free memory to the hub and turn on the LED
        String msg = "{ ""Time"" : " + String((long)now) + ", ""FreeMem"" : " + String(ESP.getFreeHeap()) + " }";
        result = deviceHandle->SendEventAsync(msg.c_str(), eventConfirmationCallback, NULL); 
  
        if (result != IOTHUB_CLIENT_OK)
        {
          Serial.println("Failed to send message");
        }
        else
        {
          digitalWrite(LED, HIGH);
          ledOffIn = 8;
          Serial.print("Sent message ");
          Serial.println(msg);
        }
      }

      last = now;
    }

    // Must call DoWork very frequently
    deviceHandle->DoWork();
    delay(10);
  }
}
