#include "stdafx.h"
#include "CFGNode.h"
#include "format.h"
#include "StlUtil.h"

using namespace std;

// Safe to call on anything. If it doesn't apply, it returns null.
CFGNode *GetOtherBranch(CFGNode *branchNode, CFGNode *branch1)
{
    CFGNode *other = nullptr;
    if (branchNode->Successors().size() == 2)
    {
        CFGNode *thenNode, *elseNode;
        GetThenAndElseBranches(branchNode, &thenNode, &elseNode);
        if (thenNode == branch1)
        {
            other = elseNode;
        }
        else if (elseNode == branch1)
        {
            other = thenNode;
        }
        else
        {
            assert(false && "branch1 is not a successor of branchNode");
        }
    }
    return other;
}

uint16_t CFGNode::GetStartingAddress() const
{
    uint16_t address = 0xffff;
    if (Type == CFGNodeType::RawCode)
    {
        address = (static_cast<const RawCodeNode*>(this))->start->get_final_offset_dontcare();
    }
    else if (Type == CFGNodeType::Exit)
    {
        address = (static_cast<const ExitNode*>(this))->startingAddressForExit;
    }
    else
    {
        assert((*this)[SemId::Head]);
        address = (*this)[SemId::Head]->GetStartingAddress();
    }
    return address;
}

// Given a node that represents a raw code branch or a compound condition, returns
// which of its two successor nodes is the "then"  branch, and which is the "else" 
void GetThenAndElseBranches(CFGNode *node, CFGNode **thenNode, CFGNode **elseNode)
{
    *thenNode = nullptr;
    *elseNode = nullptr;
    assert((node->Successors().size() == 2) && "Asking for Then/Else branches on a node without two Successors()");
    auto it = node->Successors().begin();
    CFGNode *one = *it;
    CFGNode *two = *(++it);

    if (node->Type == CFGNodeType::CompoundCondition)
    {
        CompoundConditionNode *ccNode = static_cast<CompoundConditionNode *>(node);
        uint16_t trueAddress = ccNode->thenBranch;
        if (trueAddress == one->GetStartingAddress())
        {
            *elseNode = two;
            *thenNode = one;
        }
        else
        {
            assert(trueAddress == two->GetStartingAddress());
            *elseNode = one;
            *thenNode = two;
        }
    }
    else if (node->Type == CFGNodeType::RawCode)
    {
        scii lastInstruction = node->getLastInstruction();
        if (lastInstruction.get_opcode() == Opcode::BNT)
        {
            uint16_t target = lastInstruction.get_branch_target()->get_final_offset();
            if (target == one->GetStartingAddress())
            {
                *elseNode = one;
                *thenNode = two;
            }
            else
            {
                assert(target == two->GetStartingAddress());
                *elseNode = two;
                *thenNode = one;
            }
        }
        else if (lastInstruction.get_opcode() == Opcode::BT)
        {
            uint16_t target = lastInstruction.get_branch_target()->get_final_offset();
            if (target == one->GetStartingAddress())
            {
                *elseNode = two;
                *thenNode = one;
            }
            else
            {
                assert(target == two->GetStartingAddress());
                *elseNode = one;
                *thenNode = two;
            }
        }
    }
}

RawCodeNode::RawCodeNode(code_pos start) : CFGNode(CFGNodeType::RawCode, {}), start(start)
{
    DebugId = fmt::format("{:04x}:{}", start->get_final_offset_dontcare(), OpcodeNames[static_cast<BYTE>(start->get_opcode())]);
}

bool CompareCFGNodesByAddress::operator() (const CFGNode* lhs, const CFGNode* rhs) const
{
    uint16_t lAddress = lhs->GetStartingAddress();
    uint16_t rAddress = rhs->GetStartingAddress();
    if (lAddress == rAddress)
    {
        return lhs < rhs;
    }
    else
    {
        return lAddress < rAddress;
    }
}
