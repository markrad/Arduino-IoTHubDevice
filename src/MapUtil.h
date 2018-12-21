#ifndef _MAPUTIL_H
#define _MAPUTIL_H

#include "azure_c_shared_utility/map.h"

class MapUtil
{
private:
    MAP_HANDLE _handle;
    bool _isOwned;

    MapUtil() {}
public:
    static MapUtil *CreateMap();
    MapUtil(MAP_HANDLE handle, bool isOwned = false);
    MapUtil(const MapUtil &other);
    ~MapUtil();

    MAP_HANDLE GetHandle() const { return _handle; }
    MAP_RESULT Add(const char *key, const char *value);
    MAP_RESULT AddOrUpdate(const char *key, const char *value);
    bool ContainsKey(const char *key) const;
    bool ContainsValue(const char *value) const;
    const char *GetValue(const char *key) const;
};

#endif // _MapUtil_H
