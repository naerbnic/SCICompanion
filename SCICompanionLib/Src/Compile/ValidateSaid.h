#pragma once
#include "ResourceMap.h"

struct Vocab000;
class CompileLog;

void ValidateSaids(CResourceMap& resource_map, const SCIVersion& version, CompileLog &log, const Vocab000 &vocab000);