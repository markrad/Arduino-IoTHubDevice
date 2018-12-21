#include <stdexcept>

#include "IoTHubDevice.h"

#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>
#include <azure_c_shared_utility/connection_string_parser.h>
#include "iothub_client_version.h"

using namespace std;

IoTHubDevice::IoTHubDevice(const string &connectionString, Protocol protocol) :
    _messageCallback(NULL),
    _messageCallbackUC(NULL),
    _connectionStatusCallback(NULL),
    _connectionStatusCallbackUC(NULL),
    _unknownDeviceMethodCallback(NULL),
    _unknownDeviceMethodCallbackUC(NULL),
    _logging(false)
{
    platform_init();
    _deviceHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString.c_str(), GetProtocol(protocol));

    if (_deviceHandle == NULL)
        throw runtime_error("Failed to create IoT hub handle");

    IOTHUB_CLIENT_RESULT result;
    
    result = IoTHubClient_LL_SetConnectionStatusCallback(GetHandle(), InternalConnectionStatusCallback, this);

    if (result != IOTHUB_CLIENT_OK)
        throw runtime_error("Failed to set up connection status callback");

    result = IoTHubClient_LL_SetMessageCallback(GetHandle(), InternalMessageCallback, this);

    if (result != IOTHUB_CLIENT_OK)
        throw runtime_error("Failed to set up message callback");

    result =  IoTHubClient_LL_SetDeviceMethodCallback(GetHandle(), InternalDeviceMethodCallback, this);

    if (result != IOTHUB_CLIENT_OK)
        throw runtime_error("Failed to set up device method callback");

    DList_InitializeListHead(&_outstandingEventList);
    _parsedCS = new MapUtil(connectionstringparser_parse_from_char(connectionString.c_str()), true);
}

IoTHubDevice::~IoTHubDevice()
{
    IoTHubClient_LL_Destroy(GetHandle());
    platform_deinit();

    while (!DList_IsListEmpty(&_outstandingEventList))
    {
        PDLIST_ENTRY work = _outstandingEventList.Flink;
        DList_RemoveEntryList(work);
        delete work;
    }

    for (map<std::string, DeviceMethodUserContext *>::iterator it = _deviceMethods.begin(); it != _deviceMethods.end(); it++)
    {
        delete it->second;
    }

    delete _parsedCS;
}

IoTHubDevice::MessageCallback IoTHubDevice::SetMessageCallback(MessageCallback messageCallback, void *userContext)
{
    MessageCallback temp = _messageCallback;
    _messageCallback = messageCallback;
    _messageCallbackUC = userContext;

    return temp;
}

const char *IoTHubDevice::GetHostName()
{
  return _parsedCS->GetValue("HostName");
}

const char *IoTHubDevice::GetDeviceId()
{
  return _parsedCS->GetValue("DeviceId");
}

const char *IoTHubDevice::GetVersion()
{
  return IoTHubClient_GetVersionString();
}

IoTHubDevice::ConnectionStatusCallback IoTHubDevice::SetConnectionStatusCallback(ConnectionStatusCallback connectionStatusCallback, void *userContext)
{
    ConnectionStatusCallback temp = _connectionStatusCallback;
    _connectionStatusCallback = connectionStatusCallback;
    _connectionStatusCallbackUC = userContext;
}

IoTHubDevice::DeviceMethodCallback IoTHubDevice::SetDeviceMethodCallback(const char *methodName, DeviceMethodCallback deviceMethodCallback, void *userContext)
{
    DeviceMethodUserContext *deviceMethodUserContext = new DeviceMethodUserContext(deviceMethodCallback, userContext);
    map<std::string, DeviceMethodUserContext *>::iterator it;
    DeviceMethodCallback temp = NULL;

    it = _deviceMethods.find(methodName);

    if (it == _deviceMethods.end())
    {
        _deviceMethods[methodName] = deviceMethodUserContext;
    }
    else
    {
        temp = it->second->deviceMethodCallback;
        delete it->second;

        if (deviceMethodCallback != NULL)
        {
            it->second = deviceMethodUserContext;
        }
        else
        {
            _deviceMethods.erase(it);
        }
    }

    return temp;
}

IoTHubDevice::UnknownDeviceMethodCallback IoTHubDevice::SetUnknownDeviceMethodCallback(UnknownDeviceMethodCallback unknownDeviceMethodCallback, void *userContext)
{
    UnknownDeviceMethodCallback temp = _unknownDeviceMethodCallback;
    _unknownDeviceMethodCallback = unknownDeviceMethodCallback;
    _unknownDeviceMethodCallbackUC = userContext;

    return temp;
}

IOTHUB_CLIENT_RESULT IoTHubDevice::SendEventAsync(const string &message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    IOTHUB_CLIENT_RESULT result;

    result = SendEventAsync(message.c_str(), eventConfirmationCallback, userContext);

    return result;
}

IOTHUB_CLIENT_RESULT IoTHubDevice::SendEventAsync(const char *message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    IoTHubMessage *hubMessage = new IoTHubMessage(message);
    IOTHUB_CLIENT_RESULT result;

    result =  SendEventAsync(hubMessage, eventConfirmationCallback, userContext);

    delete hubMessage;

    return result;
}

IOTHUB_CLIENT_RESULT IoTHubDevice::SendEventAsync(const uint8_t *message, size_t length, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    IoTHubMessage *hubMessage = new IoTHubMessage(message, length);
    IOTHUB_CLIENT_RESULT result;

    result =  SendEventAsync(hubMessage, eventConfirmationCallback, userContext);

    delete hubMessage;

    return result;
}

IOTHUB_CLIENT_RESULT IoTHubDevice::SendEventAsync(const IoTHubMessage *message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    MessageUserContext *messageUC = new MessageUserContext(this, eventConfirmationCallback, userContext);
    IOTHUB_CLIENT_RESULT result;

    result = IoTHubClient_LL_SendEventAsync(GetHandle(), message->GetHandle(), InternalEventConfirmationCallback, messageUC);

    if (result == IOTHUB_CLIENT_OK)
    {
        DList_InsertTailList(&_outstandingEventList, &(messageUC->dlistEntry));
    }

    return result;
}

void IoTHubDevice::DoWork()
{
    IoTHubClient_LL_DoWork(GetHandle());
}

int IoTHubDevice::WaitingEventsCount()
{
    PDLIST_ENTRY listEntry = &_outstandingEventList;
    
    int count = 0;

    while (listEntry->Flink != &_outstandingEventList)
    {
        count++;
        listEntry = listEntry->Flink;
    }

    return count;
}

void IoTHubDevice::SetLogging(bool value)
{
  _logging = value;
  IoTHubClient_LL_SetOption(GetHandle(), OPTION_LOG_TRACE, &_logging);
}

IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubDevice::InternalMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContext)
{
    IoTHubDevice *that = (IoTHubDevice *)userContext;
    IOTHUBMESSAGE_DISPOSITION_RESULT result = IOTHUBMESSAGE_REJECTED;

    if (that->_messageCallback != NULL)
    {
        IoTHubMessage *msg = new IoTHubMessage(message);

        result = that->_messageCallback(*that, *msg, that->_messageCallbackUC);

        delete msg;
    }

    return result;
}

void IoTHubDevice::InternalConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* userContext)
{
    IoTHubDevice *that = (IoTHubDevice *)userContext;

    if (that->_connectionStatusCallback != NULL)
    {
        that->_connectionStatusCallback(*that, result, reason, that->_connectionStatusCallbackUC);
    }
}

void IoTHubDevice::InternalEventConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext)
{
    MessageUserContext *messageUC = (MessageUserContext *)userContext;

    if (messageUC->eventConfirmationCallback != NULL)
    {
        messageUC->eventConfirmationCallback(*(messageUC->iotHubDevice), result, messageUC->userContext);
    }

    DList_RemoveEntryList(&(messageUC->dlistEntry));    
    delete messageUC;
}

int IoTHubDevice::InternalDeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *responseSize, void *userContext)
{
    IoTHubDevice *that = (IoTHubDevice *)userContext;
    map<std::string, DeviceMethodUserContext *>::iterator it;
    int status = 999;

    it = that->_deviceMethods.find(methodName);

    if (it != that->_deviceMethods.end())
    {
        status = it->second->deviceMethodCallback(*that, payload, size, response, responseSize, it->second->userContext);
    }
    else if (that->_unknownDeviceMethodCallback != NULL)
    {
        status = that->_unknownDeviceMethodCallback(*that, methodName, payload, size, response, responseSize, that->_unknownDeviceMethodCallbackUC);
    }
    else
    {
        status = 501;
        const char* RESPONSE_STRING = "{ \"Response\": \"Unknown method requested.\" }";
  
        *responseSize = strlen(RESPONSE_STRING);
  
        if ((*response = (unsigned char *)malloc(*responseSize)) == NULL)
        {
            status = -1;
        }
        else
        {
            memcpy(*response, RESPONSE_STRING, *responseSize);
        }
    }

    return status;    
}

IOTHUB_CLIENT_TRANSPORT_PROVIDER IoTHubDevice::GetProtocol(IoTHubDevice::Protocol protocol)
{
    IOTHUB_CLIENT_TRANSPORT_PROVIDER result = NULL;

	// Currently only MQTT is supported on Arduino - no WebSockets and no proxy
    switch (protocol)
    {
    case Protocol::MQTT:
        result = MQTT_Protocol;
        break;
    default:
        break;
    }

    return result;
}
