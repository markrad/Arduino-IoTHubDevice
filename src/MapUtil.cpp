#include <stdexcept>

#include "MapUtil.h"

using namespace std;

MapUtil *MapUtil::CreateMap()
{
    MAP_HANDLE work;

    if (NULL == (work = Map_Create(NULL)))
        throw runtime_error("Failed to create map");
    
    return new MapUtil(work, true);
}

MapUtil::MapUtil(MAP_HANDLE handle, bool isOwned)
{
    _isOwned = isOwned;
    _handle = handle;
}

MapUtil::MapUtil(const MapUtil &other)
{
    _isOwned = true;
    _handle = Map_Clone(other.GetHandle());

    if (_handle == NULL)
        throw runtime_error("Failed to clone map");
}

MapUtil::~MapUtil()
{
    if (_isOwned) 
        Map_Destroy(GetHandle());
}

MAP_RESULT MapUtil::Add(const char *key, const char *value)
{
    return Map_Add(GetHandle(), key, value);
}

MAP_RESULT MapUtil::AddOrUpdate(const char *key, const char *value)
{
    return Map_AddOrUpdate(GetHandle(), key, value);
}

bool MapUtil::ContainsKey(const char *key) const
{
    bool found;
    MAP_RESULT result = Map_ContainsKey(GetHandle(), key, &found);

    if (result != MAP_OK)
        throw runtime_error("Failed to check for key");
    else
        return found;
}

bool MapUtil::ContainsValue(const char *value) const
{
    bool found;
    MAP_RESULT result = Map_ContainsValue(GetHandle(), value, &found);

    if (result != MAP_OK)
        throw runtime_error("Failed to check for value");
    else
        return found;
}

const char *MapUtil::GetValue(const char *key) const
{
    return Map_GetValueFromKey(GetHandle(), key);
}

