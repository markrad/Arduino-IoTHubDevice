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
    typedef void(*DeviceTwinCallback)(DEVICE_TWIN_UPDATE_STATE update_state, const char* payLoad, void* userContext);
    typedef void(*ReportedStateCallback)(IoTHubDevice &iotHubDevice, int status_code, void* userContext);

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

    struct ReportedStateUserContext
    {
        DLIST_ENTRY dlistEntry;
        IoTHubDevice *iotHubDevice;
        ReportedStateCallback reportedStateCallback;
        void *userContext;
        ReportedStateUserContext(IoTHubDevice *iotHubDevice, ReportedStateCallback reportedStateCallback, void *userContext) :
            iotHubDevice(iotHubDevice), reportedStateCallback(reportedStateCallback), userContext(userContext)
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
	const char *_certificate;
    int _startResult;

    MessageCallback _messageCallback;
    ConnectionStatusCallback _connectionStatusCallback;
    UnknownDeviceMethodCallback _unknownDeviceMethodCallback;
    DeviceTwinCallback _deviceTwinCallback;

    void *_messageCallbackUC;
    void *_connectionStatusCallbackUC;
    void *_unknownDeviceMethodCallbackUC;
    void *_deviceTwinCallbackUC;

    DLIST_ENTRY _outstandingEventList;
    DLIST_ENTRY _outstandingReportedStateEventList;
    std::map<std::string, DeviceMethodUserContext *> _deviceMethods;
    MapUtil *_parsedCS;

public:
    enum Protocol
    {
        MQTT,
    };

    IoTHubDevice(const char *connectionString, IoTHubDevice::Protocol protocol = IoTHubDevice::Protocol::MQTT);
    IoTHubDevice(const char *connectionString, 
                 const char *x509Certificate,
                 const char *x509PrivateKey,
                 IoTHubDevice::Protocol protocol = IoTHubDevice::Protocol::MQTT);
    ~IoTHubDevice();

    int Start();
    void Stop();

    const char *GetHostName();
    const char *GetDeviceId();
    const char *GetVersion();
    MessageCallback SetMessageCallback(MessageCallback messageCallback, void *userContext = NULL);
    ConnectionStatusCallback SetConnectionStatusCallback(ConnectionStatusCallback ConnectionStatusCallback, void *userContext = NULL);
    DeviceMethodCallback SetDeviceMethodCallback(const char *methodName, DeviceMethodCallback deviceMethodCallback, void *userContext = NULL);
    UnknownDeviceMethodCallback SetUnknownDeviceMethodCallback(UnknownDeviceMethodCallback unknownDeviceMethodCallback, void *userContext = NULL);
    DeviceTwinCallback SetDeviceTwinCallback(DeviceTwinCallback deviceTwinCallback, void *userContext = NULL);

    IOTHUB_CLIENT_LL_HANDLE GetHandle() const;
    bool WaitingEvents() { return !DList_IsListEmpty(&_outstandingEventList); }
    int WaitingEventsCount();
    bool GetLogging() { return _logging; }
    void SetLogging(bool value);
	const char *GetTrustedCertificate() { return _certificate; }
	void SetTrustedCertificate(const char *value);
    IOTHUB_CLIENT_STATUS GetSendStatus();
    IOTHUB_CLIENT_RESULT SendEventAsync(const std::string &message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const char *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const uint8_t *message, size_t length, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const IoTHubMessage *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendReportedState(const char* reportedState, ReportedStateCallback reportedStateCallback, void* userContext = NULL);

    void DoWork();
private:
    const char *_connectionString;
    const char *_x509Certificate;
    const char *_x509PrivateKey;
    IoTHubDevice::Protocol _protocol;

    // Cloud to device messages
    static IOTHUBMESSAGE_DISPOSITION_RESULT InternalMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContext);

    // Reported status_code
    static void InternalReportedStateCallback(int status_code, void* userContextCallback);

    // Connection status callback
    static void InternalConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* userContext);

    // Event confirmation callback (for each message sent)
    static void InternalEventConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext);

    // Device method callback
    static int InternalDeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *responseSize, void *userContext);

    // Device twin callback
    static void InternalDeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size, void* userContextCallback);

    static IOTHUB_CLIENT_TRANSPORT_PROVIDER GetProtocol(IoTHubDevice::Protocol protocol);
};

#endif // _IOTHUBDEVICE_H
