#ifndef _IOTHUBDEVICE_H
#define _IOTHUBDEVICE_H

#include <string>
#include <map>

#include "IoTHubMessage.h"

#include <AzureIoTHub.h>
#include "azure_c_shared_utility/doublylinkedlist.h"

class IoTHubDevice
{
public:
    typedef IOTHUBMESSAGE_DISPOSITION_RESULT (*MessageCallback)(IoTHubDevice &iotHubDevice, IoTHubMessage &iotHubMessage, void *userContext);
    typedef void (*EventConfirmationCallback)(IoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext);
    typedef void (*ConnectionStatusCallback)(IoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContext);
    typedef int (*DeviceMethodCallback)(IoTHubDevice &iotHubDevice, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext);
    typedef int (*UnknownDeviceMethodCallback)(IoTHubDevice &iotHubDevice, const char *methodName, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext);

private:

    struct MessageUserContext
    {
        DLIST_ENTRY dlistEntry;
        IoTHubDevice *iotHubDevice;
        EventConfirmationCallback eventConfirmationCallback;
        void *userContext;
        MessageUserContext(IoTHubDevice *iotHubDevice, EventConfirmationCallback eventConfirmationCallback, void *userContext) :
            iotHubDevice(iotHubDevice), eventConfirmationCallback(eventConfirmationCallback), userContext(userContext)
        {
            dlistEntry = { 0 };
        }
    };

    struct DeviceMethodUserContext
    {
        DeviceMethodCallback deviceMethodCallback;
        void *userContext;
        DeviceMethodUserContext(DeviceMethodCallback deviceMethodCallback, void *userContext) :
            deviceMethodCallback(deviceMethodCallback), userContext(userContext)
        {
        }
    };
    
    IOTHUB_CLIENT_LL_HANDLE _deviceHandle;
    bool _logging;

    MessageCallback _messageCallback;
    ConnectionStatusCallback _connectionStatusCallback;
    UnknownDeviceMethodCallback _unknownDeviceMethodCallback;

    void *_messageCallbackUC;
    void *_connectionStatusCallbackUC;
    void *_unknownDeviceMethodCallbackUC;

    DLIST_ENTRY _outstandingEventList;
    std::map<std::string, DeviceMethodUserContext *> _deviceMethods;
    MapUtil *_parsedCS;

public:
    enum Protocol
    {
        MQTT,
    };

    IoTHubDevice(const std::string &connectionString, IoTHubDevice::Protocol protocol = IoTHubDevice::Protocol::MQTT);
    ~IoTHubDevice();

    const char *GetHostName();
    const char *GetDeviceId();
    const char *GetVersion();
    MessageCallback SetMessageCallback(MessageCallback messageCallback, void *userContext = NULL);
    ConnectionStatusCallback SetConnectionStatusCallback(ConnectionStatusCallback ConnectionStatusCallback, void *userContext = NULL);
    DeviceMethodCallback SetDeviceMethodCallback(const char *methodName, DeviceMethodCallback deviceMethodCallback, void *userContext = NULL);
    UnknownDeviceMethodCallback SetUnknownDeviceMethodCallback(UnknownDeviceMethodCallback unknownDeviceMethodCallback, void *userContext = NULL);

    IOTHUB_CLIENT_LL_HANDLE GetHandle() const { return _deviceHandle; }
    bool WaitingEvents() { return !DList_IsListEmpty(&_outstandingEventList); }
    int WaitingEventsCount();
    bool GetLogging() { return _logging; }
    void SetLogging(bool value);
    IOTHUB_CLIENT_RESULT SendEventAsync(const std::string &message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const char *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const uint8_t *message, size_t length, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const IoTHubMessage *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);

    void DoWork();
private:
    // Cloud to device messages
    static IOTHUBMESSAGE_DISPOSITION_RESULT InternalMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContext);

    // Connection status callback
    static void InternalConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* userContext);

    // Event confirmation callback (for each message sent)
    static void InternalEventConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext);

    // Device method callback
    static int InternalDeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *responseSize, void *userContext);
    
    static IOTHUB_CLIENT_TRANSPORT_PROVIDER GetProtocol(IoTHubDevice::Protocol protocol);
};

#endif // _IOTHUBDEVICE_H
