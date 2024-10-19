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
#include "stdafx.h"
#include "SyntaxParser.h"
#include "StudioSyntaxParser.h"
#include "SCISyntaxParser.h"

static const SyntaxParser* GetSyntaxParser(LangSyntax lang)
{
    // Our parser global variables
    static const auto* g_studio = CreateStudioSyntaxParser().release();
    static const auto* g_sci = CreateSCISyntaxParser().release();
    if (lang == LangSyntaxStudio)
    {
        return g_studio;
    }
    else if (lang == LangSyntaxSCI)
    {
        return g_sci;
    }
    assert(false);
}

bool SyntaxParser_ParseAC(sci::Script &script, ScriptStreamIterator &streamIt, std::unordered_set<std::string> preProcessorDefines, SyntaxContext *pContext)
{
    auto* parser = GetSyntaxParser(script.Language());

    if (!script.IsHeader())
    {
        parser->Parse(script, streamIt, preProcessorDefines, *pContext);
    }
    return false;
}

bool SyntaxParser_Parse(sci::Script &script, ScriptStream &stream, std::unordered_set<std::string> preProcessorDefines, ICompileLog *pLog, bool fParseComments, SyntaxContext *pContext, bool addCommentsToOM)
{
    auto* parser = GetSyntaxParser(script.Language());
    if (script.IsHeader())
    {
        return parser->ParseHeader(script, stream.begin(), preProcessorDefines, pLog, fParseComments);
    }
    else
    {
        if (pContext)
        {
            // Someone is doing a partial compile (e.g. tooltips) and supply their own context.
            return parser->Parse(script, stream.begin(), preProcessorDefines, *pContext);
        }
        else
        {
            // Or maybe someone either wants error logs:
            return parser->Parse(script, stream.begin(), preProcessorDefines, pLog, addCommentsToOM, fParseComments);
        }
    }
}
