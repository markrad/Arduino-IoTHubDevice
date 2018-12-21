#include <IoTHubDevice.h>
#include <IoTHubMessage.h>
#include <MapUtil.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

// Wi-Fi information
static const char *SSID = "<Your SSID>";
static const char *PASSWORD = "<Your Password of set to NULL for none>";
WiFiClient client;

// Used to test reconnection logic
bool KillWiFi = false;

// Connection string
static const char *CONNECTIONSTRING = "<Your Device Connection String>";

// LED that is flashed each time a message is sent
static const int LED = 13;

// IoT Hub
IoTHubDevice *deviceHandle = NULL;

// Message rate per minute - this example is limited to a maximum of 60 due to the manner in which it is timed 
static const int MESSAGESPERMIN = 20;

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
        Serial.println(size);
        
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
      Serial.println("Warning: unknonwn result");
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
      WiFi.disconnect();
      initWiFi();
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

void initTimeFromWiFi() 
{  
  const int MIN_EPOCH = 40 * 365 * 24 * 3600;
  time_t epochTime;

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Fetching time from NTP");
  
  epochTime = time(NULL);
  
  while (epochTime < MIN_EPOCH)
  {
    Serial.print(".");
    delay(2000);
    epochTime = time(NULL);
  }

  Serial.println();
  Serial.print("Fetched NTP epoch time is: ");
  Serial.println(epochTime);
}

void setup() 
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  initWiFi();
  initTimeFromWiFi();

  // Create IoT device
  deviceHandle = new IoTHubDevice(CONNECTIONSTRING, IoTHubDevice::Protocol::MQTT);
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

  // Set logging state
  deviceHandle->SetLogging(false);
}

void loop() 
{
  time_t last = 0;
  time_t now;
  IOTHUB_CLIENT_RESULT result;
  int ledOffIn = 0;
  
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
      
      //for (int i = 0; i < 3; i++)
      //{
        //deviceHandle->DoWork();
        //delay(10);
      //}
      WiFi.disconnect();
      delay(10);
    }

    now = get_time(NULL);

    // Send a message periodically
    if ((60 / MESSAGESPERMIN) <= now - last)
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
        String msg = "{ ""Time"" : " + String(now) + ", ""FreeMem"" : " + String(ESP.getFreeHeap()) + " }";
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