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

// Our parser global variables
std::unique_ptr<StudioSyntaxParser> g_studio;
std::unique_ptr<SCISyntaxParser> g_sci;

void InitializeSyntaxParsers()
{
    g_studio = std::make_unique<StudioSyntaxParser>();
    g_studio->Load();
    g_sci = std::make_unique<SCISyntaxParser>();
    g_sci->Load();
}

static SyntaxParser* GetSyntaxParser(LangSyntax lang)
{
    if (lang == LangSyntaxStudio)
    {
        return g_studio.get();
    }
    else if (lang == LangSyntaxSCI)
    {
        return g_sci.get();
    }
    assert(false);
}

bool SyntaxParser_ParseAC(sci::Script &script, CCrystalScriptStream::const_iterator &streamIt, std::unordered_set<std::string> preProcessorDefines, SyntaxContext *pContext)
{
    auto* parser = GetSyntaxParser(script.Language());

    if (!script.IsHeader())
    {
        parser->Parse(script, streamIt, preProcessorDefines, *pContext);
    }
    return false;
}

bool SyntaxParser_Parse(sci::Script &script, CCrystalScriptStream &stream, std::unordered_set<std::string> preProcessorDefines, ICompileLog *pLog, bool fParseComments, SyntaxContext *pContext, bool addCommentsToOM)
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
