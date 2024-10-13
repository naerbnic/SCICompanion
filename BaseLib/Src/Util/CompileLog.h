#pragma once

#include <string>
#include "ResourceTypes.h"
#include "ScriptId.h"

//
// This represents a result of compiling, which may include a line number and
// script reference.
// It can also represent an index into a textual resource.
//
class CompileResult
{
public:
    enum CompileResultType
    {
        CRT_Error,
        CRT_Warning,
        CRT_Message,
    };

    CompileResult()
    {
        _nLine = 0;
        _nCol = 0;
        _type = CRT_Message;
        _resourceType = ResourceType::None;
    }

    CompileResult(const std::string& message, CompileResultType type = CRT_Message)
    {
        _message = message;
        _nLine = 0;
        _nCol = 0;
        _type = type;
        _resourceType = ResourceType::None;
    }

    // For other resources.
    CompileResult(const std::string& message, ResourceType type, int number, int index)
    {
        _message = message;
        _nLine = number;
        _nCol = index;
        _type = CRT_Message;
        _resourceType = type;
    }

    CompileResult(const std::string& message, ScriptId script, int nLineNumber)
    {
        _message = message;
        _script = script;
        _nLine = nLineNumber;
        _nCol = 0;
        _type = CRT_Message;
        _resourceType = ResourceType::None;
    }

    CompileResult(const std::string& message, ScriptId script, int nLineNumber, int nCol, CompileResultType type)
    {
        _message = message;
        _script = script;
        _nLine = nLineNumber;
        _nCol = nCol;
        _type = type;
        _resourceType = ResourceType::None;
    }

    bool IsError() const { return (_type == CRT_Error); }
    bool IsWarning() const { return (_type == CRT_Warning); }
    ScriptId GetScript() const { return _script; }
    const std::string& GetMessage() const { return _message; }
    int GetLineNumber() const { return _nLine; }
    int GetColumn() const { return _nCol; }
    bool CanGotoScript() const { return !_script.IsNone(); }
    ResourceType GetResourceType() const { return _resourceType; }

private:
    std::string _message;
    ScriptId _script;
    ResourceType _resourceType;
    int _nLine;
    int _nCol;
    CompileResultType _type;
};

class ICompileLog
{
public:
    virtual ~ICompileLog() = default;

    virtual void ReportResult(const CompileResult& result) = 0;
};