#include "ResourceTypes.h"

#include <cstdint>

ResourceTypeFlags ResourceTypeToFlag(ResourceType dwType)
{
    return (ResourceTypeFlags)(1 << (int)dwType);
}

ResourceType ResourceFlagToType(ResourceTypeFlags dwFlags)
{
    uint32_t dwType = (uint32_t)dwFlags;
    int iShifts = 0;
    while (dwType > 1)
    {
        dwType = dwType >> 1;
        iShifts++;
    }
    return (ResourceType)iShifts;
}
