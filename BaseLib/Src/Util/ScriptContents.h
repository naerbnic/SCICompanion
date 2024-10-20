#pragma once
#include <absl/status/statusor.h>

#include "ScriptId.h"

class ScriptContents
{
public:
    static absl::StatusOr<ScriptContents> FromScriptId(const ScriptId& script_id);
    static absl::StatusOr<ScriptContents> FromFile(const std::string& filename);
    static ScriptContents FromString(std::string contents);

    LangSyntax GetSyntax() const { return resolved_syntax_; }
    std::string_view GetContents() const { return contents_; }

private:
    ScriptContents(LangSyntax syntax, std::string contents);
    LangSyntax resolved_syntax_;
    std::string contents_;

};
