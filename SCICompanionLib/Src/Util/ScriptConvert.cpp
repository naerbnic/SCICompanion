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
#include "ScriptConvert.h"
#include "ScriptStream.h"
#include "ScriptOM.h"
#include "CompileContext.h"
#include "SyntaxParser.h"
#include "OutputCodeHelper.h"
#include <filesystem>
#include "GameFolderHelper.h"
#include "ResourceMap.h"
#include "CompiledScript.h"
#include <regex>

#include "ScriptContents.h"

using namespace sci;
using namespace std;
using namespace std::filesystem;

bool ConvertScript(SCIVersion version, LangSyntax targetLanguage, ScriptId& scriptId, CompileLog& log, bool makeBak, GlobalCompiledScriptLookups* lookups)
{
    bool success = false;
    auto contents = ScriptContents::FromScriptId(scriptId);
    if (targetLanguage != contents->GetSyntax())
    {
        // What we do
        // 1) Parse this script and generate a syntax tree.
        // 2) If successful, write to the file.
        // 3) Reload
        if (contents.ok())
        {
            StringLineSource lineSource(contents->GetContents());
            ScriptStream stream(&lineSource);
            // 1)
            sci::Script script(contents->GetSyntax(), scriptId);
            success = SyntaxParser_Parse(script, stream, PreProcessorDefinesFromSCIVersion(version), &log, true); // true: Parse comments
            if (success)
            {
                EnsurePublicsInExports(script);
                PrepForLanguage(targetLanguage, script, lookups);

                std::stringstream out;
                sci::SourceCodeWriter theCode(out, &script);
                theCode.indentChar = '\t';
                theCode.indentAmount = 1;
                script.OutputSourceCode(targetLanguage, theCode);

                // Copy the old file to .bak
                std::string bakPath;
                if (makeBak)
                {
                    bakPath = scriptId.GetFullPath() + ".bak";
                    copy_file(path(scriptId.GetFullPath()), path(bakPath), copy_options::overwrite_existing);
                }

                // Now save it
                ofstream newFile(scriptId.GetFullPath());
                newFile << out.str();
                if (newFile.good())
                {
                    // All went well
                    newFile.close();

                    if (!bakPath.empty())
                    {
                        std::stringstream ss;
                        ss << scriptId.GetFileName() << " was successfully converted. Old file backed up to " + bakPath;
                        log.ReportResult(CompileResult(ss.str()));
                    }

                    // 3) Switch to the new script
                    scriptId = scriptId.WithFullPath(scriptId.GetFullPath());

                    // Should be the new language now
                    assert(DetermineFileLanguage(scriptId.GetFullPath()) == targetLanguage);
                }
                else
                {
                    success = false;
                    log.ReportResult(CompileResult("There was an error writing to the file " + scriptId.GetFullPath()));
                }
            }
        }
        else
        {
            log.ReportResult(CompileResult("There was an error opening the file " + scriptId.GetFullPath()));
        }
    }
    return success;
}

void EnumScriptIds(vector<ScriptId> &scripts, const std::string &folder, const std::string &fileExt)
{
    path enumPath = path(folder);
    std::string searchString = "(\\w+)\\.";
    auto matchRSTRegex = std::regex(searchString + fileExt);
    if (exists(enumPath) && is_directory(enumPath)) {
        for (auto it = directory_iterator(enumPath); it != directory_iterator(); ++it)
        {
            const auto& file = it->path();
            std::smatch sm;
            std::string temp = file.filename().string();
            if (!is_directory(file) && std::regex_search(temp, sm, matchRSTRegex) && (sm.size() > 1))
            {
                scripts.push_back(ScriptId::FromFullFileName(file.string()));
            }
        }
    }
}

void ConvertGame(CResourceMap &map, LangSyntax targetLanguage, CompileLog &log)
{
    const GameFolderHelper &helper = map.Helper();
    std::string convertBak = "convert-bak";

    // Delete conversion bak folder.
    DeleteDirectory(AfxGetMainWnd()->GetSafeHwnd(), helper.GetSubFolder(convertBak));

    CopyFilesOver(AfxGetMainWnd()->GetSafeHwnd(), helper.GetPolyFolder(), helper.GetPolyFolder(&convertBak));
    CopyFilesOver(AfxGetMainWnd()->GetSafeHwnd(), helper.GetMsgFolder(), helper.GetMsgFolder(&convertBak));
    CopyFilesOver(AfxGetMainWnd()->GetSafeHwnd(), helper.GetSrcFolder(), helper.GetSrcFolder(&convertBak));

    // Script files
    vector<ScriptId> scripts = map.GetAllScripts();

    // Now all the shm and shp files, and sh files in the src folder.
    EnumScriptIds(scripts, helper.GetPolyFolder(), "shp");
    EnumScriptIds(scripts, helper.GetMsgFolder(), "shm");
    EnumScriptIds(scripts, helper.GetSrcFolder(), "sh");

    // Convert it all!
    GlobalCompiledScriptLookups lookups;
    if (lookups.Load(map.GetSCIVersion(), helper.GetResourceLoader()))
    {
        for (auto &scriptId : scripts)
        {
            ConvertScript(map.GetSCIVersion(), targetLanguage, scriptId, log, false, &lookups);
        }
    }
}
