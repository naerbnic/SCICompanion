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

#include <memory>
#include <string>

#include "Version.h"

class ResourceEntity;

// fwd decl
class ResourceBlob;
std::unique_ptr<ResourceEntity> CreateResourceFromResourceData(const ResourceBlob &data, bool fallbackOnException = true);

void ExportResourceAsBitmap(const ResourceEntity &resourceEntity);

struct SCI_RESOURCE_INFO
{
    std::string pszSampleFolderName;
    std::string pszTitleDefault;         // Name of the resource in the editor, and the header in game.ini
    std::string pszFileFilter_SCI0;
    std::string pszFileFilter_SCI1;
    std::string pszNameMatch_SCI0;
    std::string pszNameMatch_SCI1;
};

extern SCI_RESOURCE_INFO g_resourceInfo[18];
SCI_RESOURCE_INFO &GetResourceInfo(ResourceType type);
ResourceType ValidateResourceType(ResourceType type);
std::string GetFileDialogFilterFor(ResourceType type, SCIVersion version);
std::string GetFileNameFor(const ResourceId& resource_id, const SCIVersion& version);
std::string GetFileNameFor(const ResourceBlob &blob);
bool MatchesResourceFilenameFormat(const std::string &filename, ResourceType type, SCIVersion version, int *numberOut, std::string &nameOut);
bool MatchesResourceFilenameFormat(const std::string &filename, SCIVersion version, int *numberOut, std::string &nameOut);

extern const char Base36AudioPrefix;
extern const char Base36SyncPrefix;

std::string FigureOutResourceName(const std::string& iniFileName, ResourceType type, int iNumber, uint32_t base36Number);
