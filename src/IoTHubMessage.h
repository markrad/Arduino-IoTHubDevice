#ifndef _IOTMESSAGE_H
#define _IOTMESSAGE_H

#include <string>
#include <cstdint>

#include <AzureIoTHub.h>

#include "MapUtil.h"

class IoTHubMessage
{
private:
    IOTHUB_MESSAGE_HANDLE _handle;
    bool _isOwned;

    IoTHubMessage();

public:
    IoTHubMessage(const std::string &message);
    IoTHubMessage(const char *message);
    IoTHubMessage(const uint8_t *message, size_t length);
    IoTHubMessage(IOTHUB_MESSAGE_HANDLE handle);
    IoTHubMessage(const IoTHubMessage &other);
    ~IoTHubMessage();

    IOTHUB_MESSAGE_HANDLE GetHandle() const { return _handle; }
    const char *GetCString() const;
    const std::string GetString() const;
    IOTHUB_MESSAGE_RESULT GetByteArray(const uint8_t **buffer, size_t *size) const;
    IOTHUBMESSAGE_CONTENT_TYPE GetContentType() const;
    IOTHUB_MESSAGE_RESULT SetContentTypeSystemProperty(const char *contentType);
    const char *GetContentTypeSystemProperty() const;
    MapUtil *GetProperties();
    IOTHUB_MESSAGE_RESULT SetProperty(const char *key, const char *value);
    const char *GetProperty(const char *key) const;
    IOTHUB_MESSAGE_RESULT SetMessageId(const char *messageId);
    const char *GetMessageId() const;
    IOTHUB_MESSAGE_RESULT SetCorrelationId(const char *correlationId);
    const char *GetCorrelationId() const;
};

#endif // _IOTMESSAGE_H
