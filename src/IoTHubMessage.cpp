#include <stdexcept>

#include "IoTHubMessage.h"

using namespace std;

IoTHubMessage::IoTHubMessage()
{
    throw runtime_error("Default constructor for IoTHubMessage should not be called");
}

IoTHubMessage::IoTHubMessage(const std::string &message) : IoTHubMessage(message.c_str())
{

}

IoTHubMessage::IoTHubMessage(const char *message)
{
    _isOwned = true;
    _handle = IoTHubMessage_CreateFromString(message);

    if (_handle == NULL)
        throw runtime_error("Failed to create IoTHubMessage instance");
}

IoTHubMessage::IoTHubMessage(const uint8_t *message, size_t length)
{
    _isOwned = true;
    _handle = IoTHubMessage_CreateFromByteArray(message, length);

    if (_handle == NULL)
        throw runtime_error("Failed to create IoTHubMessage instance");
}

IoTHubMessage::IoTHubMessage(IOTHUB_MESSAGE_HANDLE handle)
{
    _isOwned = false;
    _handle = handle;
}

IoTHubMessage::IoTHubMessage(const IoTHubMessage &other)
{
    _handle = IoTHubMessage_Clone(other.GetHandle());

    if (_handle == NULL)
        throw runtime_error("Failed to create IoTHubMessage instance");
}

IoTHubMessage::~IoTHubMessage()
{
    if (_isOwned)
        IoTHubMessage_Destroy(GetHandle());
}

const char *IoTHubMessage::GetCString() const
{
    if (GetContentType() == IOTHUBMESSAGE_STRING)
    {
        return IoTHubMessage_GetString(GetHandle());
    }
    else 
    {
        return "";
    }
}

const string IoTHubMessage::GetString() const
{
    return string(IoTHubMessage_GetString(GetHandle()));
}

IOTHUB_MESSAGE_RESULT IoTHubMessage::GetByteArray(const uint8_t **buffer, size_t *size) const
{
    return IoTHubMessage_GetByteArray(GetHandle(), buffer, size);
}

IOTHUBMESSAGE_CONTENT_TYPE IoTHubMessage::GetContentType() const
{
    return IoTHubMessage_GetContentType(GetHandle());
}

IOTHUB_MESSAGE_RESULT IoTHubMessage::SetContentTypeSystemProperty(const char *contentType)
{
    return IoTHubMessage_SetContentTypeSystemProperty(GetHandle(), contentType);
}

const char *IoTHubMessage::GetContentTypeSystemProperty() const
{
    return IoTHubMessage_GetContentTypeSystemProperty(GetHandle());
}

MapUtil *IoTHubMessage::GetProperties()
{
    return new MapUtil(IoTHubMessage_Properties(GetHandle()));
}

IOTHUB_MESSAGE_RESULT IoTHubMessage::SetProperty(const char *key, const char *value)
{
    return IoTHubMessage_SetProperty(GetHandle(), key, value);
}

const char *IoTHubMessage::GetProperty(const char *key) const
{
    return IoTHubMessage_GetProperty(GetHandle(), key);
}

IOTHUB_MESSAGE_RESULT IoTHubMessage::SetMessageId(const char *messageId)
{
    return IoTHubMessage_SetMessageId(GetHandle(), messageId);
}

const char *IoTHubMessage::GetMessageId() const
{
    return IoTHubMessage_GetMessageId(GetHandle());
}

IOTHUB_MESSAGE_RESULT IoTHubMessage::SetCorrelationId(const char *correlationId)
{
    return IoTHubMessage_SetCorrelationId(GetHandle(), correlationId);
}

const char *IoTHubMessage::GetCorrelationId() const
{
    return IoTHubMessage_GetCorrelationId(GetHandle());
}

