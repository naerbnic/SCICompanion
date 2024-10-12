#pragma once

#include <string>
#include <Windows.h>

#include "ResourceTypes.h"

int ResourceNumberFromFileName(PCTSTR pszFileName);

// Returns "n004" for 4.
std::string default_reskey(int iNumber, uint32_t base36Number);
