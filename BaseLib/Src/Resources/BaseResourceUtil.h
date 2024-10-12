#pragma once

#include <string>
#include <optional>

#include "ResourceTypes.h"

int ResourceNumberFromFileName(const char *pszFileName);

// Returns "n004" for 4.
std::string default_reskey(int iNumber, std::optional<uint32_t> base36Number = std::nullopt);
std::string default_reskey(const ResourceNum& resource_id);
