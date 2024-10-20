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
#include "ScriptStream.h"
#include "CompileLog.h"

namespace sci
{
    class Script;
}
class ScriptStream;
class SyntaxContext;

class SyntaxParser
{
public:
    virtual ~SyntaxParser() = default;
    virtual bool Parse(sci::Script& script, ScriptStreamIterator& stream,
        std::unordered_set<std::string> preProcessorDefines, ICompileLog* pError, bool addCommentsToOM,
        bool collectComments) const = 0;
    virtual bool Parse(sci::Script& script, ScriptStreamIterator& stream,
        std::unordered_set<std::string> preProcessorDefines, SyntaxContext& context) const = 0;
    virtual bool ParseHeader(sci::Script& script, ScriptStreamIterator& stream,
        std::unordered_set<std::string> preProcessorDefines, ICompileLog* pError, bool collectComments) const = 0;
};

bool SyntaxParser_Parse(sci::Script &script, ScriptStream &stream, std::unordered_set<std::string> preProcessorDefines, ICompileLog *pLog = nullptr, bool fParseComments = false, SyntaxContext *pContext = nullptr, bool addCommentsToOM = false);

std::unordered_set<std::string> PreProcessorDefinesFromSCIVersion(SCIVersion version);