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

namespace sci
{
    class Script;
}
class CompiledScript;
class DecompileLookups;
class ILookupNames;
class GameFolderHelper;
struct Vocab000;
sci::Script *Decompile(const GameFolderHelper &helper, const SCIVersion& version, const CompiledScript &compiledScript, DecompileLookups &lookups, const Vocab000 *pWords);