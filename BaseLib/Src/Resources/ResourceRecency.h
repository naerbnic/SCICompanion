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
#pragma once

#include <vector>
#include <unordered_map>

#include "ResourceTypes.h"

//
// Contains information regarding which resources are the "current ones"
//

class ResourceRecency
{
public:
    ResourceRecency() { _idJustAdded = -1; }
    ~ResourceRecency() { ClearAllResourceTypes(); } // Clean up
    //
    // We added a new resource to the view.
    // fAddToEnd: add to end (least recent).  Otherwise, it is added as the most recent.
    //
    void AddResourceToRecency(const ResourceDescriptor& resource_descriptor, bool fAddToEnd = false);

    //
    // Someone deleted a resource from the view.
    //
    void DeleteResourceFromRecency(const ResourceDescriptor& resource_descriptor);

    //
    // Is this resource the most recent one of the type/number/id?
    // e.g. the one that will be used by the game.
    //
    bool IsResourceMostRecent(const ResourceDescriptor& resource_descriptor) const;

    //
    // Did the user just add this resource?
    //
    bool WasResourceJustAdded(const ResourceDescriptor& resource_descriptor) const;

    //
    // Called when we reload the resources of a particular type
    //
    void ClearResourceType(int iType);
    void ClearAllResourceTypes();

private:
    //
    // A map for each of the n different resource types.
    // Each map contains values which are arrays of the unique resource ids
    //
    typedef std::vector<int> ResourceIdArray;
    // hmm... each entry in this array has a bunch of resource keys (combo of number/package) that maps to lists of
    // resource ids.
	typedef std::unordered_map<uint64_t, ResourceIdArray> RecencyMap;

    ResourceIdArray* EnsureRecencyList(const ResourceDescriptor& resource_descriptor);
    std::optional<const ResourceIdArray*> GetRecencyList(const ResourceDescriptor& resource_descriptor) const;
    std::optional<ResourceIdArray*> GetRecencyList(const ResourceDescriptor& resource_descriptor);

    RecencyMap _resourceRecency[NumResourceTypes];

    int _idJustAdded;
};