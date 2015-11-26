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

#include "StudioSyntaxParser.h"

namespace sci
{
    enum class ValueType;
}

template<typename _TParser>
void FunctionStatementA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->FunctionPtr->AddStatement(move(pContext->StatementPtrReturn));
    }
}

template<typename _TParser>
void AddUseA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->Script().AddUse(pContext->ScratchString());
    }
}

template<typename _TParser>
void AddIncludeA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->Script().AddInclude(pContext->ScratchString());
    }
}

template<typename _TParser>
void ScriptNumberA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        if (pContext->PropertyValue.GetType() == ValueType::Number)
        {
            pContext->Script().SetScriptNumber(pContext->PropertyValue.GetNumberValue());
        }
        else
        {
            pContext->Script().SetScriptNumberDefine(pContext->PropertyValue.GetStringValue());
        }
    }
}

template<typename _TParser>
void PropValueIntA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->PropertyValueWasSet = true;
        pContext->PropertyValue.SetValue(pContext->Integer, pContext->HexInt);
        if (pContext->NegInt)
        {
            pContext->PropertyValue.Negate();
        }
        pContext->PropertyValue.SetPosition(stream.GetPosition());
    }
}

template<sci::ValueType type, typename _TParser>
void PropValueStringA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->PropertyValueWasSet = true;
        pContext->PropertyValue.SetValue(pContext->ScratchString(), type);
        pContext->PropertyValue.SetPosition(stream.GetPosition());
    }
}


//
// Defines
//
template<typename _TParser>
void CreateDefineA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateDefine();
        pContext->DefinePtr->SetScript(&pContext->Script());
    }
}

template<typename _TParser>
void DefineLabelA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->DefinePtr->SetLabel(pContext->ScratchString());
        pContext->DefinePtr->SetPosition(stream.GetPosition());
    }
    else
    {
        pContext->ReportError("Expected define label.", stream);
    }
}

template<typename _TParser>
void DefineValueA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        IntegerFlags flags = IntegerFlags::None;
        if (pContext->HexInt)
        {
            flags |= IntegerFlags::Hex;
        }
        if (pContext->NegInt)
        {
            flags |= IntegerFlags::Negative;
        }
        pContext->DefinePtr->SetValue(pContext->Integer, flags);
    }
    else
    {
        pContext->ReportError("Expected integer value.", stream);
    }
}

template<typename _TParser>
void FinishDefineA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        if (pContext->ifDefDefineState != IfDefDefineState::False)
        {
            pContext->Script().AddDefine(move(pContext->DefinePtr));
        }
    }
}

// Script variable
template<typename _TParser>
void CreateScriptVarA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->TransferScriptVariable();
    }
    else
    {
        pContext->ReportError("Expected variable declaration.", stream);
    }
}

template<typename _TParser>
void ScriptVarInitA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        // Add a value to the script variable.
        pContext->VariableDeclPtr->AddSimpleInitializer(pContext->PropertyValue);
    }
    else
    {
        pContext->ReportError("Script variables cannot be initialized with this type of value.", stream);
    }
}

template<typename _TParser>
void ScriptVarInitAutoExpandA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        // Add a value to the script variable.
        pContext->VariableDeclPtr->AddSimpleInitializer(pContext->PropertyValue);
        if (pContext->VariableDeclPtr->IsUnspecifiedSize())
        {
            pContext->VariableDeclPtr->SetSize(pContext->VariableDeclPtr->GetInitializers().size());
            pContext->VariableDeclPtr->SetIsUnspecifiedSize(true); // Since the above reset it.
        }
    }
    else
    {
        pContext->ReportError("Script variables cannot be initialized with this type of value.", stream);
    }
}

template<typename _TParser>
void FinishScriptVarA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->VariableDeclPtr->SetScript(&pContext->Script());
        pContext->Script().AddVariable(move(pContext->VariableDeclPtr));
    }
}

// Variable Declaration (e.g. foo[5])
template<typename _TParser>
void CreateVarDeclA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->StartVariableDecl(pContext->ScratchString());
        pContext->VariableDecl->SetPosition(stream.GetPosition());
    }
}

template<typename _TParser>
void VarDeclSizeA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->VariableDecl->SetSize(pContext->Integer);
    }
}

template<typename _TParser>
void VarDeclSizeConstantA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->VariableDecl->SetSize(pContext->ScratchString());
    }
}

template<typename _TParser>
void VarDeclSizeErrorA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (!match.Result())
    {
        pContext->ReportError("Expected array size.", stream);
    }
}

// Function temp vars
template<typename _TParser>
void StartFunctionTempVarA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        // Just make sure we clear out any value (in case its used for initialization).
        // Actually this is no longer true. Variables aren't zero-initialized.
        // We actually need to track whether or not the variable has an initial value. Temp variables (i.e. in functions)
        // can only have initial values if they are not arrays (and the initial values are set explicitly by assignments in byte code)
        // I guess this is an SCIStudio limitation, but we'll keep the same syntax.
        pContext->PropertyValueWasSet = false;	// So we'll actually say it was not set.
        pContext->PropertyValue.Zero();
    }
    else
    {
        pContext->ReportError("Expected variable name.", stream);
    }
}

template<typename _TParser>
void FinishFunctionTempVarA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->VariableDecl->SetPosition(stream.GetPosition());
        // REVIEW
        // Use the current PropertyValue for initialization (it not used, it was zero'd out in StartFunctionTempVarA)
        if (pContext->PropertyValueWasSet)
        {
            pContext->FunctionPtr->AddVariable(move(pContext->VariableDecl), pContext->PropertyValue);
        }
        else
        {
            pContext->FunctionPtr->AddVariable(move(pContext->VariableDecl));
        }
    }
}

// Procedures

// Function
template<typename _TParser>
void CreateProcedureA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateProcedure();
    }
}

template<typename _TParser>
void CreateMethodA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateMethod();
        pContext->FunctionPtr->SetOwnerClass(pContext->ClassPtr.get());
    }
}

template<typename _TParser>
void ProcedurePublicA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        static_cast<ProcedurePtr>(pContext->FunctionPtr.get())->SetPublic(true);
    }
}

template<typename _TParser>
void FunctionNameA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->FunctionPtr->SetName(pContext->ScratchString());
        pContext->FunctionPtr->SetScript(&pContext->Script());
        pContext->FunctionPtr->SetPosition(stream.GetPosition());
    }
    else
    {
        pContext->ReportError("Expected function name.", stream);
    }
}

template<typename _TParser>
void FunctionParameterA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        std::unique_ptr<FunctionParameter> param = std::make_unique<FunctionParameter>(pContext->ScratchString());
        param->SetPosition(stream.GetPosition());
        pContext->FunctionPtr->GetSignaturesNC()[0]->AddParam(std::move(param), false);
    }
}

template<typename _TParser>
void FunctionCloseA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    pContext->FunctionPtr->SetEndPosition(stream.GetPosition());
    if (!match.Result() && pParser->_psz) // Check for _psz here... might have ')', or nothing.
    {
        GeneralE(match, pParser, pContext, stream);
    }
}

template<typename _TParser>
void FinishProcedureA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->Script().AddProcedure(move(pContext->GetFunctionAsProcedure()));
    }
}

template<typename _TParser>
void ProcedureClassA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        static_cast<ProcedurePtr>(pContext->FunctionPtr.get())->SetClass(pContext->ScratchString());
    }
    else
    {
        pContext->ReportError("Expected class name.", stream);
    }
}


// Add a statement to a SyntaxNode
template<typename _T, typename _TParser>
void AddStatementA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<_T>()->AddStatement(move(pContext->StatementPtrReturn));
    }
}

template<typename _TParser>
void StartStatementA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->PushSyntaxNode();
    }
}

template<typename _TParser>
void FinishStatementA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->FinishStatement(true);
        assert(pContext->StatementPtrReturn); // This is our "return value"
        pContext->StatementPtrReturn->SetPosition(stream.GetPosition()); // Not the most accurate position, but good enough.
    }
    else
    {
        pContext->FinishStatement(false);
    }
}

template<typename _T, typename _TParser>
void SetStatementA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateSyntaxNode<_T>(stream);
        pContext->GetSyntaxNode<_T>()->SetPosition(stream.GetPosition());
    }
}

template<typename _T, char const* error, typename _TParser>
void StatementBindTo1stA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<_T>()->SetStatement1(move(pContext->StatementPtrReturn));
    }
    else if (error)
    {
        pContext->ReportError(error, stream);
    }
}

template<typename _T, char const* error, typename _TParser>
void StatementBindTo2ndA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<_T>()->SetStatement2(move(pContext->StatementPtrReturn));
    }
    else
    {
        std::string statementName = pContext->GetSyntaxNode<_T>()->ToString();
        statementName += ":";
        statementName += error;
        pContext->ReportError(statementName.c_str(), stream);
    }
}

// Property values

// Complex properties
template<typename _TParser>
void ComplexValueIntA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        ComplexPropertyValue *pValue = pContext->GetSyntaxNode<ComplexPropertyValue>();
        pValue->SetValue(pContext->Integer, pContext->HexInt);
        if (pContext->NegInt)
        {
            pValue->Negate();
        }
    }
}

template<typename _TParser>
void ComplexValuePointerA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<ComplexPropertyValue>()->SetType(ValueType::Pointer);
    }
}

template<sci::ValueType type, typename _TParser>
void ComplexValueStringA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        ValueType typeToUse = type;
        ComplexPropertyValue *pValue = pContext->GetSyntaxNode<ComplexPropertyValue>();
        if (pValue->GetType() == ValueType::Pointer)
        {
            assert(typeToUse == ValueType::Token);
            typeToUse = ValueType::Pointer; // It was already decided it was a pointer.
        }
        pValue->SetValue(pContext->ScratchString(), typeToUse);
    }
}

template<typename _TParser>
void ComplexValueIndexerA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<ComplexPropertyValue>()->SetIndexer(move(pContext->StatementPtrReturn));
    }
    else
    {
        pContext->ReportError("Expected array index.", stream);
    }
}

//
// Syntax node
//
template<typename _TParser>
bool SyntaxNodeD(const _TParser *pParser, SyntaxContext *pContext, streamIt &stream)
{
    // Make room on the stack for a statement
    pContext->PushSyntaxNode();
    // Then call our sub parser
    return pParser->_pa->Match(pContext, stream).Result();
}
template<typename _TParser>
void EndSyntaxNodeA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    // No matter what the state, pop a node off the stack.
    pContext->DeleteAndPopSyntaxNode();
}

// l-value
template<typename _TParser>
void LValueIndexerA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<LValue>()->SetIndexer(move(pContext->StatementPtrReturn));
    }
    else
    {
        pContext->ReportError("Expected array index.", stream);
    }
}

// Assignments
template<typename _TParser>
void AssignmentVariableA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        // This is the variable description we need to add to the assignment thing:
        pContext->GetPrevSyntaxNode<Assignment>()->SetVariable(move(pContext->StealSyntaxNode<LValue>()));
    }
    else
    {
        pContext->ReportError("Expected variable.", stream);
    }
}


// Set the name of a SyntaxNode
template<typename _T, typename _TParser>
void SetStatementNameA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateSyntaxNode<_T>(stream);
        pContext->GetSyntaxNode<_T>()->SetName(pContext->ScratchString());
    }
}
template<typename _T, typename _TParser>
void SetSelectorNameA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->CreateSyntaxNode<_T>(stream);
        pContext->GetSyntaxNode<_T>()->SetName(pContext->ScratchString());
    }
}

template<typename _T, typename _TParser>
void SetNameA(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const streamIt &stream)
{
    if (match.Result())
    {
        pContext->GetSyntaxNode<_T>()->SetName(pContext->ScratchString());
    }
}

// Errors
template<typename _It, typename _TParser>
void IdentifierE(MatchResult &match, const _TParser *pParser, SyntaxContext *pContext, const _It &stream)
{
    if (!match.Result())
    {
        pContext->ReportError("Expected keyword.", stream);
    }
}

// Denoted as extern, so they can be used as function template parameters.
extern char const errBinaryOp[];
extern char const errCaseArg[];
extern char const errSwitchArg[];
extern char const errSendObject[];
extern char const errArgument[];
extern char const errInteger[];

