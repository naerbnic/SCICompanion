/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/
#include "ResourceRecency.h"

#include <cassert>

#include "ResourceBlob.h"

static uint64_t _GetLookupKey(const ResourceLocation& resource_location)
{
    // These should always be already-added resources, so they should never be -1
    assert(resource_location.GetNumber() != -1);
    assert(resource_location.GetPackageHint() != -1);
    uint64_t first = resource_location.GetNumber() * 256 * 256 + resource_location.GetPackageHint();
    uint64_t low = resource_location.GetBase36();
    return low + (first << 32);
}

template<typename _T, typename _K>
bool contains(const _T &container, const _K &key)
{
    return container.find(key) != container.end();
}

void ResourceRecency::AddResourceToRecency(const ResourceDescriptor& resource_descriptor, bool fAddToEnd)
{
    _idJustAdded = -1;
    uint64_t iKey = _GetLookupKey(resource_descriptor.GetResourceLocation());

    ResourceIdArray* pidList = EnsureRecencyList(resource_descriptor);

    // Add this resource.
    if (fAddToEnd)
    {
        // This is called when we're enumerating.
        pidList->push_back(resource_descriptor.GetChecksum());
    }
    else
    {
        // This is called when the user adds a resource.
        _idJustAdded = resource_descriptor.GetChecksum();
        pidList->insert(pidList->begin(), resource_descriptor.GetChecksum());
    }
}

void ResourceRecency::DeleteResourceFromRecency(const ResourceDescriptor& resource_descriptor)
{
    auto pidList = GetRecencyList(resource_descriptor);
    if (pidList.has_value())
    {
        auto& recencyList = *pidList.value();
        // Find this item's id.
        for (ResourceIdArray::iterator it = recencyList.begin(); it != recencyList.end(); ++it)
        {
            if (*it == resource_descriptor.GetChecksum())
            {
                // Found it. Remove it.
                recencyList.erase(it);
                break;
            }
        }
    }
}

//
// Is this the most recent resource of this type.
//
bool ResourceRecency::IsResourceMostRecent(const ResourceDescriptor& resource_descriptor) const
{
    if (resource_descriptor.GetNumber() == -1)
    {
        // The resource is not saved - so consider it the "most recent" version of itself.
        return true;
    }

    auto recencyListOpt = GetRecencyList(resource_descriptor);
    uint64_t iKey = _GetLookupKey(resource_descriptor.GetResourceLocation());

    RecencyMap::const_iterator found = _resourceRecency[(int)resource_descriptor.GetType()].find(iKey);
    if (!recencyListOpt.has_value())
    {
        // If we didn't find anything, then consider it the most recent.
        // (for some items, like audio resources, we don't calculate recency)
        return true;
    }

    const auto& recencyList = *recencyListOpt.value();
    // (should always have at least one element)
    return !recencyList.empty() && (*recencyList.begin() == resource_descriptor.GetChecksum());
}

bool ResourceRecency::WasResourceJustAdded(const ResourceDescriptor& resource_descriptor) const
{
    return (resource_descriptor.GetChecksum() == _idJustAdded);
}

void ResourceRecency::ClearResourceType(int iType)
{
    assert(iType < NumResourceTypes);

    RecencyMap &map = _resourceRecency[iType];
    // Before removing all, we must de-allocate each array we created.
    map.clear();
}

void ResourceRecency::ClearAllResourceTypes()
{
    for (int i = 0; i < NumResourceTypes; i++)
    {
        ClearResourceType(i);
    }
}
ResourceRecency::ResourceIdArray* ResourceRecency::EnsureRecencyList(const ResourceDescriptor& resource_descriptor)
{
    uint64_t iKey = _GetLookupKey(resource_descriptor.GetResourceLocation());

    ResourceIdArray* pidList;
    return &_resourceRecency[(int)resource_descriptor.GetType()][iKey];
}

std::optional<const ResourceRecency::ResourceIdArray*> ResourceRecency::GetRecencyList(
    const ResourceDescriptor& resource_descriptor) const
{
    uint64_t iKey = _GetLookupKey(resource_descriptor.GetResourceLocation());

    auto found = _resourceRecency[(int)resource_descriptor.GetType()].find(iKey);
    if (found != _resourceRecency[(int)resource_descriptor.GetType()].end())
    {
        return &found->second;
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<ResourceRecency::ResourceIdArray*> ResourceRecency::GetRecencyList(
    const ResourceDescriptor& resource_descriptor)
{
    uint64_t iKey = _GetLookupKey(resource_descriptor.GetResourceLocation());

    auto found = _resourceRecency[(int)resource_descriptor.GetType()].find(iKey);
    if (found != _resourceRecency[(int)resource_descriptor.GetType()].end())
    {
        return &found->second;
    }
    else
    {
        return std::nullopt;
    }
}
