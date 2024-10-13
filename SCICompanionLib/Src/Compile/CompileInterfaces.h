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

#include <string_view>

#include "Types.h"
#include "CompileLog.h"

enum class ResolvedToken
{
    ImmediateValue,
    GlobalVariable,
    ScriptVariable,
    ScriptString,
    TempVariable,
    Parameter,
    Class,
    Instance, // Might need more...
    ExportInstance, // Instance in another script.
    ClassProperty,
    Self,
    Unknown,
};

enum ProcedureType
{
    ProcedureUnknown,
    ProcedureMain, // Something in the main script (wIndex)
    ProcedureExternal, // Something in another script  (wScript, wIndex)
    ProcedureLocal, // Something in the current script (wIndex)
    ProcedureKernel, // A kernel function (wIndex)
};

//
// Allows a source code component to declare its filename/position.
// Also supports an "end" position, optionally (by default == to start position)
//
class ISourceCodePosition
{
public:
    ISourceCodePosition()
    {
    }

    ISourceCodePosition(const ISourceCodePosition& src) = default;
    int GetLineNumber() const { return _start.Line(); }
    int GetColumnNumber() const { return _start.Column(); }
    int GetEndLineNumber() const { return _end.Line(); }

    void SetPosition(LineCol pos)
    {
        _start = pos;
        _end = pos;
    }

    void SetEndPosition(LineCol pos) { _end = pos; }
    LineCol GetPosition() const { return _start; }
    LineCol GetEndPosition() const { return _end; }

protected:
    ISourceCodePosition& operator=(const ISourceCodePosition& src) = default;

private:
    LineCol _start;
    LineCol _end;
};


class CompileContext;

class IVariableLookupContext
{
public:
    virtual ResolvedToken LookupVariableName(CompileContext& context, const std::string& str, WORD& wIndex,
                                             SpeciesIndex& dataType) const = 0;
};

// Selector/value pair.
struct species_property
{
    uint16_t wSelector;
    uint16_t wValue;
    SpeciesIndex wType; // DataTypeNone means no type was specified.
    bool fTrackRelocation;
};


enum class IntegerFlags : uint16_t
{
    None = 0x00000000,
    Hex = 0x00000001, // Originally represented as a hex number.
    Negative = 0x00000002, // Originally represented as a -ve number. (value is already correct)
};

DEFINE_ENUM_FLAGS(IntegerFlags, uint16_t)

class ILookupSaids
{
public:
    virtual ~ILookupSaids() = default;
    virtual bool LookupWord(const std::string& word, uint16_t& wordGroup) = 0;
};

class ILookupDefine
{
public:
    virtual ~ILookupDefine() = default;
    virtual bool LookupDefine(const std::string& str, uint16_t& wValue) = 0;
};
