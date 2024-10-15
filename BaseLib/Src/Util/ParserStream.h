#pragma once
#include <string>

#include "LineCol.h"

// This is a pure abstract interface to be implemented by the text buffer.
//
// This is the equivalent of the old CCrystalScriptStream class.
class ParserStream
{
public:
    virtual ~ParserStream() = default;

    virtual char GetChar() const = 0;
    virtual void Advance() = 0;
    virtual std::string ToString() const = 0;
    virtual LineCol GetPosition() const = 0;

    virtual void Restore();
    void ResetLine();
};