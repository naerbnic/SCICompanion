// Syntax parser utilities for implementations of syntax parsers.

#pragma once

#include "CrystalScriptStream.h"
#include "ParserCommon.h"

inline bool ExtractToken(std::string& str, ScriptCharIterator& stream)
{
    bool fRet = false;
    str.clear();
    char ch = stream.GetChar();
    if (isalpha(ch) || (ch == '_'))     // First character must be a letter or _
    {
        fRet = true;
        str += ch;
        ch = stream.AdvanceAndGetChar();
        while (isalnum(ch) || (ch == '_'))  // Then any alphanumeric character is fine.
        {
            fRet = true;
            str += ch;
            ch = stream.AdvanceAndGetChar();
        }
    }
    return fRet;
}


//
// Common parsing primitives
//
template<typename _TContext, typename _CommentPolicy>
bool AlphanumP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    return ExtractToken(pContext->ScratchString(), stream);
}


//
// Quick way to specify filesnames without needing quotes. Accepts alphanumeric characters, _ and .
//
template<typename _TContext, typename _CommentPolicy>
bool FilenameP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    std::string& str = pContext->ScratchString();
    str.clear();
    char ch = stream.GetChar();
    if (isalnum(ch) || (ch == '_'))
    {
        fRet = true;
        str += ch;
        ch = stream.AdvanceAndGetChar();
        while (isalnum(ch) || (ch == '_') || (ch == '.'))
        {
            fRet = true;
            str += ch;
            ch = stream.AdvanceAndGetChar();
        }
    }
    return fRet;
}

// Asm instructions can include '?' in the name too
template<typename _TContext, typename _CommentPolicy>
bool AsmInstructionP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    std::string& str = pContext->ScratchString();
    str.clear();
    char ch = stream.GetChar();
    if (isalpha(ch) || (ch == '_') || (ch == '&') || (ch == '-') || (ch == '+'))     // First character must be a letter or _ or & or + or - (for the rest instruction, or inc/dec)
    {
        fRet = true;
        str += ch;
        ch = stream.AdvanceAndGetChar();
        while (isalnum(ch) || (ch == '_') || (ch == '?'))  // Then any alphanumeric character is fine, or ?
        {
            fRet = true;
            str += ch;
            ch = stream.AdvanceAndGetChar();
        }
    }

    if (fRet)
    {
        fRet = IsOpcode(str);
    }

    return fRet;
}

extern const char* g_keywords[4];

// TODO: Refactor with above
template<typename _TContext, typename _CommentPolicy>
bool AlphanumPNoKeyword(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = AlphanumP(pParser, pContext, stream);
    if (fRet)
    {
        std::string& str = pContext->ScratchString();
        for (int i = 0; fRet && (i < ARRAYSIZE(g_keywords)); i++)
        {
            fRet = (str != g_keywords[i]);
        }
        if (fRet && pContext->extraKeywords)
        {
            fRet = pContext->extraKeywords->find(str) == pContext->extraKeywords->end();
        }
    }
    return fRet;
}

template<typename _TContext, typename _CommentPolicy>
bool SequenceP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    for (auto& parser : pParser->_parsers)
    {
        if (!parser->Match(pContext, stream).Result())
        {
            return false;
        }
    }
    return true;
}

template<typename _TContext, typename _CommentPolicy>
bool AlternativeP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    for (auto& parser : pParser->_parsers)
    {
        if (parser->Match(pContext, stream).Result())
        {
            return true;
        }
    }
    return false;
    return false;
}

template<typename _TContext, typename _CommentPolicy>
bool KleeneP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    while (pParser->_pa->Match(pContext, stream).Result())
    {
        while (isspace(stream.GetChar()))
        {
            stream.Advance();
        }
    }
    return true; // Always matches
}

template<typename _TContext, typename _CommentPolicy>
bool OneOrMoreP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool atLeastOne = false;
    while (pParser->_pa->Match(pContext, stream).Result())
    {
        atLeastOne = true;
        while (isspace(stream.GetChar()))
        {
            stream.Advance();
        }
    }
    return atLeastOne;
}

template<typename _TContext, typename _CommentPolicy>
bool ZeroOrOnceP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    pParser->_pa->Match(pContext, stream);
    return true; // Always matches
}

template<typename _TContext, typename _CommentPolicy>
bool NotP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    return !pParser->_pa->Match(pContext, stream).Result();
}

template<typename _TContext, typename _CommentPolicy>
bool QuotedStringP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    return pParser->ReadStringStudio<'"', '"'>(pContext, stream, pContext->ScratchString());
}

template<typename _TContext, typename _CommentPolicy>
bool SQuotedStringP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    // Note: We could expand autocomplete coverage to include said tokens. In that case we'd need to
    // have ReadStringStudio *not* block all autocomplete channels.
    return pParser->ReadStringStudio<'\'', '\''>(pContext, stream, pContext->ScratchString());
}

template<typename _TContext, typename _CommentPolicy>
bool BraceStringP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    return pParser->ReadStringStudio<'{', '}'>(pContext, stream, pContext->ScratchString());
}

//
// Handles negation, hex, etc...
//
template<typename _TContext, typename _CommentPolicy>
bool IntegerP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    int i = 0;
    bool fNeg = false;
    bool fRet = false;
    bool fHex = false;
    if (stream.GetChar() == '$')
    {
        fHex = true;
        stream.Advance();
        while (isxdigit(stream.GetChar()) && (i <= (std::numeric_limits<uint16_t>::max)()))
        {
            i *= 16;
            i += charToI(stream.GetChar());
            fRet = true;
            stream.Advance();
        }
    }
    else
    {
        if (stream.GetChar() == '-')
        {
            fNeg = true;
            stream.Advance();
        }
        while (isdigit(stream.GetChar()) && (i <= (std::numeric_limits<uint16_t>::max)()))
        {
            i *= 10;
            i += charToI(stream.GetChar());
            fRet = true;
            stream.Advance();
        }
    }
    if (fRet)
    {
        // Make sure that the number isn't followed by an alphanumeric char
        fRet = !isalnum(stream.GetChar());
    }
    if (fNeg)
    {
        i = -i;
    }
    if (fRet)
    {
        if (fRet)
        {
            fRet = (i <= (std::numeric_limits<uint16_t>::max)());
            if (!fRet)
            {
                pContext->ReportError(errIntegerTooLarge, stream);
            }
            else
            {
                fRet = (i >= (std::numeric_limits<int16_t>::min)());
                if (!fRet)
                {
                    pContext->ReportError(errIntegerTooSmall, stream);
                }
            }
        }

        // Let the context know so people can use it.
        pContext->SetInteger(i, fNeg, fHex, stream);
    }
    return fRet;
}


template<typename _TContext, typename _CommentPolicy>
bool AlphanumOpenP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    std::string& str = pContext->ScratchString();
    str.clear();
    char ch = stream.GetChar();
    if (isalpha(ch) || (ch == '_'))
    {
        fRet = true;
        str += ch;
        ch = stream.AdvanceAndGetChar();
        while (isalnum(ch) || (ch == '_'))
        {
            fRet = true;
            str += ch;
            ch = stream.AdvanceAndGetChar();
        }
    }
    if (fRet && (stream.GetChar() == '('))
    {
        stream.Advance();
        return true;
    }
    else
    {
        return false;
    }
}

template<typename _TContext, typename _CommentPolicy>
bool AlwaysMatchP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    return true; // Always matches
}


//
// Matches a single character
//
template<typename _TContext, typename _CommentPolicy>
bool CharP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    const char* psz = pParser->_psz;
    while (*psz && (stream.GetChar() == *psz))
    {
        stream.Advance();
        ++psz;
    }
    return (*psz == 0);
}

//
// Matches a keyword (e.g. a piece of text NOT followed by an alphanumeric character).
//
template<typename _TContext, typename _CommentPolicy>
bool KeywordP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    const char* psz = pParser->_psz;
    while (*psz && (stream.GetChar() == *psz))
    {
        stream.Advance();
        ++psz;
    }
    if (*psz == 0)
    {
        // We used up the whole keyword.  Make sure it isn't followed by another alpha numeric char.
        return !isalnum(stream.GetChar());
    }
    return false;
}

template<typename _TContext, typename _CommentPolicy>
bool OperatorP(const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, ScriptCharIterator& stream)
{
    bool fRet = false;
    const char* psz = pParser->_psz;
    while (*psz && (stream.GetChar() == *psz))
    {
        stream.Advance();
        ++psz;
    }
    if (*psz == 0)
    {
        pContext->ScratchString() = pParser->_psz;

        char chNext = stream.GetChar();
        // REVIEW: we're relying on careful ordering of rules to have this parser work.
        // The "operator" parser needs to make sure there isn't another "operator char" following
        // this:
        if ((chNext != '|') && (chNext != '&') && (chNext != '=') && (chNext != '<') && (chNext != '>'))
        {
            // Probably more...
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}



//
// Parser operators
//

//
// *a
// 
// Matches n instances of a  (where n can be 0)
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator*(const ParserBase<_TContext, _CommentPolicy>& a)
{
    return ParserBase<_TContext, _CommentPolicy>(KleeneP<_TContext, _CommentPolicy>, a);
}

//
// a >> b
// 
// Matches a followed by b
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator>>(const ParserBase<_TContext, _CommentPolicy>& a, const ParserBase<_TContext, _CommentPolicy>& b)
{
    // We must create a new "sequence" thing if a is not a sequence, or if a has an action.
    // If a has an action, then we'll mis-place it if we don't create a new sequence.
    // e.g.
    // c = a >> b;
    // e = c[act1] >> d[act2]
    // would result in
    // Seq[act1]
    //      a
    //      b
    //      d[act2]
    // The result would be act2 gets called prior to act1.
    // It looks like this optimization won't be useful, since most things have actions.
    if ((a.GetMatchingFunction() != SequenceP<_TContext, _CommentPolicy>) || (a._pfnA))
    {
        ParserBase<_TContext, _CommentPolicy> alternative(SequenceP<_TContext, _CommentPolicy>);
        alternative.AddParser(a);
        alternative.AddParser(b);
        return alternative;
    }
    else
    {
        // A is already an alternative.  Add b to it.
        ParserBase<_TContext, _CommentPolicy> parserA(a);
        if (parserA.GetMatchingFunction() == SequenceP<_TContext, _CommentPolicy>)
        {
            parserA.AddParser(b);
            return parserA;
        }
        else
        {
            // The matching function changed (reference forwarding...) we can't just add it...
            ParserBase<_TContext, _CommentPolicy> alternative(SequenceP<_TContext, _CommentPolicy>);
            alternative.AddParser(a);
            alternative.AddParser(b);
            return alternative;
        }
    }
}

//
// a | b
// 
// Matches a or b
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator|(const ParserBase<_TContext, _CommentPolicy>& a, const ParserBase<_TContext, _CommentPolicy>& b)
{
    // See operator>>
    if ((a.GetMatchingFunction() != AlternativeP<_TContext, _CommentPolicy>) || (a._pfnA))
    {
        ParserBase<_TContext, _CommentPolicy> alternative(AlternativeP<_TContext, _CommentPolicy>);
        alternative.AddParser(a);
        alternative.AddParser(b);
        return alternative;
    }
    else
    {
        // A is already an alternative.  Add b to it.
        ParserBase<_TContext, _CommentPolicy> parserA(a);
        if (parserA.GetMatchingFunction() == AlternativeP<_TContext, _CommentPolicy>)
        {
            parserA.AddParser(b);
            return parserA;
        }
        else
        {
            // The matching function of a changed... we can no longer just add.
            ParserBase<_TContext, _CommentPolicy> alternative(AlternativeP<_TContext, _CommentPolicy>);
            alternative.AddParser(a);
            alternative.AddParser(b);
            return alternative;
        }
    }
}

//
// (a % b)     
//
// equivalent to    (a >> *(b >> a))
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator%(const ParserBase<_TContext, _CommentPolicy>& a, const ParserBase<_TContext, _CommentPolicy>& b)
{
    return a >> *(b >> a);
}

//
// -a
// 
// 0 or 1 instances of a
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator-(const ParserBase<_TContext, _CommentPolicy>& a)
{
    return ParserBase<_TContext, _CommentPolicy>(ZeroOrOnceP<_TContext, _CommentPolicy>, a);
}

//
// ++a
// 
// Matches one or more instances of a
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator++(const ParserBase<_TContext, _CommentPolicy>& a)
{
    return ParserBase<_TContext, _CommentPolicy>(OneOrMoreP<_TContext, _CommentPolicy>, a);
}

//
// !a
// 
// true if a fails to match
//
template<typename _TContext, typename _CommentPolicy>
inline ParserBase<_TContext, _CommentPolicy> operator!(const ParserBase<_TContext, _CommentPolicy>& a)
{
    return ParserBase<_TContext, _CommentPolicy>(NotP<_TContext, _CommentPolicy>, a);
}


//
// Common error primitives
//
template<typename _TContext, typename _CommentPolicy>
void GeneralE(MatchResult& match, const ParserBase<_TContext, _CommentPolicy>* pParser, _TContext* pContext, const ScriptCharIterator& stream)
{
    assert(pParser->_psz); // Not valid to call this if there isn't something we can say was expected.
    if (!match.Result())
    {
        std::string error = "Expected \"";
        error += pParser->_psz;
        error += "\" ";
        pContext->ReportError(error, stream);
    }
}
