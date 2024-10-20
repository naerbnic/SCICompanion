#include "ScriptContents.h"

#include "WindowsUtils.h"

absl::StatusOr<ScriptContents> ScriptContents::FromScriptId(const ScriptId& script_id)
{
    return ScriptContents::FromFile(script_id.GetFullPath());
}

absl::StatusOr<ScriptContents> ScriptContents::FromFile(const std::string& filename)
{
    auto contents = ReadTextFileContents(filename);
    if (!contents.ok())
    {
        return contents.status();
    }
    auto syntax = DetermineLanguage(*contents);
    return ScriptContents(syntax, std::move(contents).value());
}

ScriptContents ScriptContents::FromString(std::string contents)
{
    return ScriptContents(DetermineLanguage(contents), contents);
}

inline ScriptContents::ScriptContents(LangSyntax syntax, std::string contents) : resolved_syntax_(syntax),
    contents_(std::move(contents))
{
}
