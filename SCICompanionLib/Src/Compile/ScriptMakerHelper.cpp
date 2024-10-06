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
#include "ScriptOM.h"
#include "ScriptOMAll.h"
#include "ScriptMakerHelper.h"
#include "Operators.h"

using namespace sci;
using namespace std;

void _SetSendVariableTarget(SendCall &send, const std::string &target)
{
    auto lValue = make_unique<LValue>();
    lValue->SetName(target);
    send.SetLValue(move(lValue));
}

unique_ptr<SyntaxNode> _MakeNumberStatement(int16_t w)
{
    // TODO: Does this correctly handle all negative numbers?
    auto pValue = std::make_unique<ComplexPropertyValue>();
    pValue->SetValue((uint16_t)w);
    if (w < 0)
    {
        pValue->Negate();
    }
    return pValue;
}

unique_ptr<SyntaxNode> _MakeTokenStatement(const string &token)
{
    auto pValue = std::make_unique<ComplexPropertyValue>();
    pValue->SetValue(token, ValueType::Token);
    return pValue;
}

void _AddAssignment(MethodDefinition &method, const string &lvalueName, const string &assigned)
{
    auto pEquals = std::make_unique<Assignment>();
    pEquals->Operator = AssignmentOperator::Assign;
    auto lvalue = make_unique<LValue>();
    lvalue->SetName(lvalueName);
    pEquals->SetVariable(std::move(lvalue));
    pEquals->SetStatement1(_MakeTokenStatement(assigned));
    method.AddStatement(std::move(pEquals));
}

void _AddBasicSwitch(MethodDefinition &method, const string &switchValue, const string &case0Comments)
{
    auto pSwitch = std::make_unique<SwitchStatement>();

    // Make the switch value and add it to the swtich
    pSwitch->SetStatement1(_MakeTokenStatement(switchValue));

    // Make the case statement and add it to the switch
    auto pCase = std::make_unique<CaseStatement>();
    pCase->SetStatement1(_MakeNumberStatement(0));
    pCase->AddNewStatement<Comment>(case0Comments, CommentType::Indented);
    pSwitch->AddCase(std::move(pCase));

    // Add the switch to the method
    method.AddStatement(std::move(pSwitch));
}

// parameter may be empty.
void _AddSendCall(MethodDefinition &method, const string &objectName, const string &methodName, const string &parameter, bool isVariable)
{
    auto pSend = std::make_unique<SendCall>();
    if (isVariable)
    {
        _SetSendVariableTarget(*pSend, objectName);
    }
    else
    {
        pSend->SetName(objectName);
    }

    // Create the send param to add to the send call
    auto pParam = std::make_unique<SendParam>();
    pParam->SetName(methodName);
    pParam->SetIsMethod(true);

    if (!parameter.empty())
    {
        // Add the parameter to the sendparam.
        auto pValue = std::make_unique<ComplexPropertyValue>();
        pValue->SetValue(parameter, ValueType::Token);
        method.AddStatement(std::move(pValue));
    }

    pSend->AddSendParam(std::move(pParam));
    method.AddStatement(std::move(pSend));
}