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
#include "ScriptOMAll.h"

using namespace sci;
using namespace std;

template<typename T>
void ForwardTraverse(const T &container, IExploreNode &en)
{
    for (auto const& node : container)
    {
        node->Traverse(en);
    }
}


void FunctionBase::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_signatures, en);
    ForwardTraverse(_tempVars, en);
	ForwardTraverse(_segments, en);
}

void FunctionSignature::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_params, en);
}

void FunctionParameter::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}

void CodeBlock::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	ForwardTraverse(_segments, en);
}
void NaryOp::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_segments, en);
}
void Cast::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_statement1->Traverse(en);
}
void SendCall::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    // One of three forms of send params.
    ForwardTraverse(_params, en);
	if (GetTargetName().empty())
    {
        if (!_object3 || _object3->GetName().empty())
        {
			_statement1->Traverse(en);
        }
        else
        {
			_object3->Traverse(en);
        }
    }
    if (_rest)
    {
        _rest->Traverse(en);
    }
}
void ProcedureCall::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	ForwardTraverse(_segments, en);
}
void ConditionalExpression::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	ForwardTraverse(_segments, en);
}
void SwitchStatement::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_statement1->Traverse(en);
    ForwardTraverse(_cases, en);
}
void CondStatement::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    if (_statement1) { _statement1->Traverse(en); }
    ForwardTraverse(_clausesTemp, en);
}
void ForLoop::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	GetInitializer()->Traverse(en);
	_innerCondition->Traverse(en);
    _looper->Traverse(en);
	ForwardTraverse(_segments, en);
}
void WhileLoop::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_innerCondition->Traverse(en);
	ForwardTraverse(_segments, en);
}
void Script::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);

    //
    ForwardTraverse(Globals, en);
    ForwardTraverse(Externs, en);
    ForwardTraverse(ClassDefs, en);
    ForwardTraverse(Selectors, en);

    ForwardTraverse(_procedures, en);
    ForwardTraverse(_classes, en);
    ForwardTraverse(_scriptVariables, en);
    ForwardTraverse(_scriptStringDeclarations, en);
    ForwardTraverse(_synonyms, en);
}

void VariableDecl::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}

void DoLoop::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_innerCondition->Traverse(en);
	ForwardTraverse(_segments, en);
}
void Assignment::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_variable->Traverse(en);
	_statement1->Traverse(en);
}
void ClassProperty::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
}
void ClassDefinition::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_methods, en);
    ForwardTraverse(_properties, en);
}
void SendParam::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	ForwardTraverse(_segments, en);
}
void LValue::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    if (HasIndexer())
    {
		_indexer->Traverse(en);
    }
}
void ReturnStatement::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	if (_statement1) _statement1->Traverse(en);
}
void CaseStatementBase::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    if (!IsDefault() && _statement1) _statement1->Traverse(en);
	ForwardTraverse(_segments, en);
}
void UnaryOp::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_statement1->Traverse(en);
}
void BinaryOp::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_statement1->Traverse(en);
	_statement2->Traverse(en);
}
void IfStatement::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
	_innerCondition->Traverse(en);
	_statement1->Traverse(en);
    if (_statement2)
    {
        _statement2->Traverse(en);
    }
}
void ComplexPropertyValue::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
    if (_pArrayInternal)
    {
        _pArrayInternal->Traverse(en);
    }
}
void PropertyValueNode::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
}
void Synonym::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}
void BreakStatement::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
}
void ContinueStatement::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}
void RestStatement::Traverse(IExploreNode &en)
{
	ExploreNodeBlock enb(en, *this);
}
void Asm::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_segments, en);
}
void AsmBlock::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    ForwardTraverse(_segments, en);
}
void WeakSyntaxNode::Traverse(IExploreNode &en)
{
    assert(false);
}
void ClassDefDeclaration::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}
void SelectorDeclaration::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
}
void GlobalDeclaration::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    if (InitialValue)
    {
        InitialValue->Traverse(en);
    }
}
void ExternDeclaration::Traverse(IExploreNode &en)
{
    ExploreNodeBlock enb(en, *this);
    ScriptNumber.Traverse(en);
}