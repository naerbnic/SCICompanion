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
#include "GameFolderHelper.h"

class CompileLog;
class CResourceMap;
class GlobalCompiledScriptLookups;

bool ConvertScript(const GameFolderHelper& game_folder_helper, SCIVersion version, LangSyntax targetLanguage, ScriptId &scriptId, CompileLog &log, bool makeBak, GlobalCompiledScriptLookups *lookups = nullptr);
void ConvertGame(CResourceMap &resMap, LangSyntax targetLanguage, CompileLog &log);