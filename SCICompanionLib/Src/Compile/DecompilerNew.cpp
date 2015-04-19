#include "stdafx.h"
#include "CFGNode.h"
#include "DecompilerCore.h"
#include "DecompilerNew.h"
#include "ScriptOMAll.h"
#include "AppState.h"

using namespace std;
using namespace sci;
// Can we make a CodeChunk that pretends like it's a tree?


std::string _indent2(int iIndent)
{
    std::string theFill;
    theFill.insert(theFill.begin(), iIndent, ' ');
    return theFill;
}


enum class ChunkType
{
    None,
    If,
    Then,
    Else,
    Condition,
    Do,
    While,
    LoopBody,
    And,
    Or,
    First,
    FirstNegated,
    Second,
    Invert,
    Switch,
    Case,
    DefaultCase,
    CaseCondition,
    CaseBody,
    SwitchValue,
    Break,
};

const char *chunkTypeNames[] =
{
    "None",
    "If",
    "Then",
    "Else",
    "Condition",
    "Do",
    "While",
    "LoopBody",
    "And",
    "Or",
    "First",
    "First_N",
    "Second",
    "Invert",
    "Switch",
    "Case",
    "DefaultCase",
    "CaseCondition",
    "CaseBody",
    "SwitchValue",
};

struct CodeChunk
{
    CodeChunk() : _hasPos(false), _chunkType(ChunkType::None) {}

    bool _hasPos;
    code_pos pos;
    ChunkType _chunkType;

    code_pos GetCode()
    {
        assert(_hasPos);
        return pos;
    }
    ChunkType GetType()
    {
        return _chunkType;
    }

    CodeChunk *GetChild(ChunkType type)
    {
        for (auto &child : children)
        {
            if (child->GetType() == type)
            {
                return child.get();
            }
        }
        return nullptr;
    }

    void SetPos(code_pos pos) { this->pos = pos; _hasPos = true; }
    void SetType(ChunkType type) { this->_chunkType = type; _hasPos = false; }

    CodeChunk *PrependChild()
    {
        children.insert(children.begin(), move(make_unique<CodeChunk>()));
        return (*children.begin()).get();
    }
    void PrependChild(std::unique_ptr<CodeChunk> chunk)
    {
        children.insert(children.begin(), move(chunk));
    }

    void Print(std::ostream &os, int iIndent) const
    {
        os << _indent2(iIndent);
        if (_hasPos)
        {
            os << OpcodeNames[static_cast<BYTE>(pos->get_opcode())];
        }
        else
        {
            os << "[" << chunkTypeNames[(int)_chunkType] << "]";
        }
        os << "\n";

        for (auto &child : children)
        {
            child->Print(os, iIndent + 2);
        }
    }

    const std::vector<std::unique_ptr<CodeChunk>> &Children() { return children; }

    std::unique_ptr<CodeChunk> StealChild(size_t index)
    {
        std::unique_ptr<CodeChunk> stolen = move(children[index]);
        children.erase(children.begin() + index);
        return stolen;
    }
    size_t GetChildCount() { return children.size(); }
    CodeChunk *Child(int i) { return children[i].get(); }

private:
    // Children might be stored backward for now, we'll see
    std::vector<std::unique_ptr<CodeChunk>> children;
};

struct ContextFrame
{
    const CFGNode *node;
    CodeChunk *Parent;
    int cStackConsume;
    int cAccConsume;
};

class CodeChunkEnumContext
{
public:
    stack<ContextFrame> frames;
    CodeChunk *Current;

    ContextFrame &Frame() {
        return frames.top();
    }

    bool CanPopBeyond()
    {
        // Not sure how I'll handle this. It's possible each frame might
        // need a stack of CFGNode's, and we check if we're in it. Or rather, if we're in
        // it, then we can pop. If we're not, it's someone else's frame.

        // REVIEW: What we have now isn't enough. Consider the case where we had to
        // bounce back out of RawCodeNode in order to get more instructions, while deep inside.
        // and now we're in an if statement say... how will we ever bounce back up? I think instead
        // we have the notion of a parent CFGNode (in the visiting thing), and we shouldn't bounce beyond that or something?
        return
            (frames.size() > 1) 
            && (Frame().node == nullptr);
    }

    size_t GetLevelCount() { return frames.size(); }

    void PopFrame()
    {
        frames.pop();
        Current = (frames.size() > 1) ? frames.top().Parent : nullptr;
    }

    void AddStructured(ChunkType type)
    {
        Current = Frame().Parent->PrependChild();
        Current->SetType(type);
    }

    void PushStructured(ChunkType type, const CFGNode *ownerNode)
    {
        Current = Frame().Parent->PrependChild();
        Current->SetType(type);
        assert(Frame().cStackConsume == 0);
        assert(Frame().cAccConsume == 0);
        // Generally structured nodes don't consume anything
        // (though we might change that to support re-using previous acc values?)
        PushFrame(0, 0, ownerNode);
    }

    void StartInstruction(code_pos code)
    {
        Current = Frame().Parent->PrependChild();
        Current->SetPos(code);
        assert(Frame().cStackConsume == 0);
        assert(Frame().cAccConsume == 0);
        Consumption consumption = _GetInstructionConsumption(*code);
        if (consumption.cAccConsume || consumption.cStackConsume)
        {
            // This instruction needs stuff... recurse
            PushFrame(consumption.cAccConsume, consumption.cStackConsume, nullptr);
        }
    }

    void AddInstructionToCurrent(code_pos code)
    {
        CodeChunk *newChild = Current->PrependChild();
        newChild->SetPos(code);

        Consumption consumption = _GetInstructionConsumption(*code);
        if (consumption.cAccConsume || consumption.cStackConsume)
        {
            // This instruction we just got consumes stuff. We need to recurse here and
            // have it eat up its stuff.
            Current = newChild;
            PushFrame(consumption.cAccConsume, consumption.cStackConsume, nullptr);
        }

    }

    void ConsumeAccForStructuredNode()
    {
        // A structured node like an if counts as generating one acc
        // REVIEW: Only if there is an else also... ?
        if (Frame().cAccConsume)
        {
            Frame().cAccConsume--;
        }
    }

    void PopBackUpMax()
    {
        // Pop all the nodes we can
        while ((Frame().cAccConsume == 0) && (Frame().cStackConsume == 0))
        {
            if (CanPopBeyond())
            {
                PopFrame();
            }
            else
            {
                break;
            }
        }
    }

    void PushMain(const MainNode *node)
    {
        PushFrame(0, 0, node);
    }

private:
    void PushFrame(int cAccConsume, int cStackConsume, const CFGNode *node = nullptr)
    {
        ContextFrame frame;
        frame.node = node;
        frame.cAccConsume = cAccConsume;
        frame.cStackConsume = cStackConsume;
        frame.Parent = Current;
        frames.push(frame);
    }
};

CFGNode *GetFirstPredecessorOrNull(CFGNode *node)
{
    assert(node->Predecessors().size() <= 1);
    return node->Predecessors().empty() ? nullptr : *node->Predecessors().begin();
}
CFGNode *GetFirstSuccOrNull(CFGNode *node)
{
    assert(node->Successors().size() <= 1);
    return node->Successors().empty() ? nullptr : *node->Successors().begin();
}

// This assembles our CFGNode tree into a tree of nodes that consume the accumulator and stack,
// with children that produce the acc/stack values that they consume.
// We can do this consume tree just inside the RawCodeNodes, because sometimes control
// structures take part in the tree. For instance, an if statement or switch statement
// can be embedded in and expression, since they "return" values in the accumulator.
class EnumerateCodeChunks : public ICFGNodeVisitor
{
private:
    // Scope guard for pushing/popping frames for structured nodes.
    class StructuredFrame
    {
    public:
        StructuredFrame(CodeChunkEnumContext &context, ChunkType type, const CFGNode &node) : _context(context)
        {
            _context.PushStructured(type, &node);
        }
        ~StructuredFrame()
        {
            _context.PopFrame();
        }
    private:
        CodeChunkEnumContext &_context;
    };

public:
    EnumerateCodeChunks(CodeChunkEnumContext &context) : _context(context) {}

    void Visit(const RawCodeNode &rawCodeNode) override
    {
        code_pos cur = rawCodeNode.end;
        --cur;
        code_pos start = rawCodeNode.start;
        if (start->get_opcode() != Opcode::INDETERMINATE)
        {
            --start;
        }
        while (cur != start)
        {
            if ((_context.Frame().cAccConsume == 0) && (_context.Frame().cStackConsume == 0))
            {
                if (_context.CanPopBeyond())
                {
                    _context.PopFrame();
                }
                else
                {
                    // We can't pop up any more. Time for a new instruction in the current frame.
                    _context.StartInstruction(cur);
                    --cur;
                }
            }
            else
            {
                Consumption consTemp = _GetInstructionConsumption(*cur);
                if (consTemp.cAccGenerate)
                {
                    if (_context.Frame().cAccConsume)
                    {
                        _context.Frame().cAccConsume -= consTemp.cAccGenerate;
                    }
                }
                if (consTemp.cStackGenerate)
                {
                    if (_context.Frame().cStackConsume)
                    {
                        _context.Frame().cStackConsume -= consTemp.cStackGenerate;
                    }
                    else
                    {
                        // Something put something on the stack and we didn't need it.
                        // We can't do anything useful here.
                        assert(false); // INVESTIGATE
                    }
                }

                _context.AddInstructionToCurrent(cur);
                --cur;
            }
        }

        _context.PopBackUpMax();
    }

    void Visit(const LoopNode &loopNode) override
    {
        size_t levelCount = _context.GetLevelCount();
        _context.ConsumeAccForStructuredNode();

        // Let's detect the loop type
        CFGNode *latch = loopNode[SemId::Latch];
        CFGNode *head = loopNode[SemId::Head];
        if ((latch->Successors().size() == 2) && (head->Successors().size() == 1))
        {
            StructuredFrame structuredFrame(_context, ChunkType::Do, loopNode);
            // It's a do loop. The condition is the latch.

            // Condition
            {
                StructuredFrame structuredFrame(_context, ChunkType::Condition, loopNode);
                latch->Accept(*this);
            }

            // then... this is a bit more complicated since we follow a chain
            {
                StructuredFrame structuredFrame(_context, ChunkType::LoopBody, loopNode);
                assert(latch->Predecessors().size() == 1);
                _FollowForwardChain(head, *latch->Predecessors().begin());
            }
        }
        else if ((latch->Successors().size() == 1) && (head->Successors().size() == 2))
        {
            StructuredFrame structuredFrame(_context, ChunkType::While, loopNode);
            // It's a while. The condition is the head.

            // Condition
            {
                StructuredFrame structuredFrame(_context, ChunkType::Condition, loopNode);
                head->Accept(*this);
            }

            CFGNode *thenNode, *elseNode;
            GetThenAndElseBranches(head, &thenNode, &elseNode);
            {
                StructuredFrame structuredFrame(_context, ChunkType::LoopBody, loopNode);
                _FollowForwardChain(thenNode, latch);
            }
        }

        assert(levelCount == _context.GetLevelCount());
        _context.PopBackUpMax();
    }

    void Visit(const SwitchNode &switchNode) override
    {
        // TODO: Need to sort out pre-amble and tail and such, because right now the switch is just one big blob.
        // This presents an issue with accumulator consumption.
        // Or perhaps we can just process the tail without entering a new context.
        CFGNode *switchTail = switchNode[SemId::Tail];
        switchTail->Accept(*this);
        // After processing the tail, we MUST have a toss that we just processed.
        assert(_context.Frame().cStackConsume == 1);
        if (_context.Frame().cStackConsume)
        {
            _context.Frame().cStackConsume--;
        }
        // Pop back up 
        _context.PopBackUpMax();

        // Now we can do the actual cases, and consume any necessary acc
        size_t levelCount = _context.GetLevelCount();
        _context.ConsumeAccForStructuredNode();

        _context.PushStructured(ChunkType::Switch, &switchNode);

        // We'll add the cases here. They are immediate successors of the head.
        vector<CFGNode*> sortedCases;
        CFGNode *switchHead = switchNode[SemId::Head];
        copy(switchHead->Successors().begin(), switchHead->Successors().end(), back_inserter(sortedCases));
        sort(sortedCases.begin(), sortedCases.end(), CompareCFGNodesByAddress());
        for (CFGNode *caseNode : sortedCases)
        {
            caseNode->Accept(*this);
        }

        // And now the switch head.
        // Now we need to process the switch value. But it's special.... because we may have a pre-amble.
        // We basically want to process just enough to get back to net zero. And then beyond that,
        // we should switch "out" of the switch node.
        // OR... we could just move out of the switch node right now:
        _context.PopFrame();
        assert(levelCount == _context.GetLevelCount());

        // And then process the switch value? Then during code gen, we'll know to look prior to the switch statement for the value?
        switchHead->Accept(*this);

        _context.PopBackUpMax();
    }

    void Visit(const CaseNode &caseNode) override
    {
        size_t levelCount = _context.GetLevelCount();
        // Cases don't need to do acc consumption.

        _context.PushStructured(ChunkType::Case, &caseNode);

        // There could be a case condition.
        CFGNode *caseHead = caseNode[SemId::Head];
        CFGNode *caseBody = caseHead;
        if (caseHead->ContainsTag(SemanticTags::CaseCondition))
        {
            assert(!caseNode.ContainsTag(SemanticTags::DefaultCase));
            assert(caseHead->Type == CFGNodeType::RawCode);
            
            _context.PushStructured(ChunkType::CaseCondition, &caseNode);
            caseHead->Accept(*this);
            _context.PopFrame();
            caseBody = GetFirstSuccOrNull(caseHead);
        }

        _context.PushStructured(ChunkType::CaseBody, &caseNode);
        _FollowForwardChain(caseBody);
        _context.PopFrame();

        _context.PopFrame();
        assert(levelCount == _context.GetLevelCount());
        _context.PopBackUpMax();
    }

    void Visit(const CompoundConditionNode &conditionNode) override
    {
        size_t levelCount = _context.GetLevelCount();
        _context.ConsumeAccForStructuredNode();

        ConditionType type = conditionNode.condition;
        bool isFirstTermNegated = conditionNode.isFirstTermNegated;

        _context.PushStructured((type == ConditionType::And) ? ChunkType::And : ChunkType::Or, &conditionNode);

        CFGNode *first = conditionNode[SemId::First];
        // X
        _context.PushStructured(isFirstTermNegated ? ChunkType::FirstNegated : ChunkType::First, &conditionNode);
        first->Accept(*this);
        _context.PopFrame();

        CFGNode *second = conditionNode[SemId::Second];
        // Y
        _context.PushStructured(ChunkType::Second, &conditionNode);
        second->Accept(*this);
        _context.PopFrame();

        _context.PopFrame();
        assert(levelCount == _context.GetLevelCount());
        _context.PopBackUpMax();
    }

    void Visit(const InvertNode &invert) override
    {
        size_t levelCount = _context.GetLevelCount();
        _context.ConsumeAccForStructuredNode();

        _context.PushStructured(ChunkType::Invert, &invert);
        invert[SemId::Head]->Accept(*this);
        _context.PopFrame();
        assert(levelCount == _context.GetLevelCount());
        _context.PopBackUpMax();
    }

    void _FollowForwardChain(CFGNode *node, CFGNode *inclusiveEnd = nullptr)
    {
        // We want to go backwards
        vector<CFGNode*> nodes;
        while (node != nullptr)
        {
            nodes.push_back(node);
            if (inclusiveEnd == node)
            {
                break;
            }
            node = GetFirstSuccOrNull(node);
        }

        std::reverse(nodes.begin(), nodes.end());
        for (CFGNode *current : nodes)
        {
            if (current->ContainsTag(SemanticTags::LoopBreak))
            {
                // We've already identified break statements during control flow, so we can
                // be confident this is one without looking at its destination.
                //AddStatement(move(make_unique<BreakStatement>()));
                _context.AddStructured(ChunkType::Break);
            }
            else
            {
                current->Accept(*this);
            }
        }
    }

    void Visit(const IfNode &ifNode) override
    {
        size_t levelCount = _context.GetLevelCount();
        _context.ConsumeAccForStructuredNode();

        // Adds a child with an if
        _context.PushStructured(ChunkType::If, &ifNode);

        // Now we need to add three children:
        //  - condition
        //  - then
        //  - else
        CFGNode *ifCondition = ifNode[SemId::Head];
        CFGNode *thenNode, *elseNode;
        GetThenAndElseBranches(ifCondition, &thenNode, &elseNode);

        // Condition
        _context.PushStructured(ChunkType::Condition, &ifNode);
        ifCondition->Accept(*this);
        _context.PopFrame();

        // then... this is a bit more complicated since we follow a chain
        _context.PushStructured(ChunkType::Then, &ifNode);
        _FollowForwardChain(thenNode);
        _context.PopFrame();

        if (elseNode && (elseNode->Type != CFGNodeType::Exit))
        {
            _context.PushStructured(ChunkType::Else, &ifNode);
            _FollowForwardChain(elseNode);
            _context.PopFrame();
        }

        _context.PopFrame();
        assert(levelCount == _context.GetLevelCount());
        _context.PopBackUpMax();
    }

    void Visit(const MainNode &main) override
    {
        _context.PushMain(&main);
        CFGNode *node = main[SemId::Tail];
        while (node)
        {
            node->Accept(*this);
            node = GetFirstPredecessorOrNull(node);
        }
        _context.PopFrame();

        // For now, print out mainChunk
    }
    void Visit(const CFGNode &node)override {}

private:
    CodeChunkEnumContext &_context;
};

// fwd decl
std::unique_ptr<SyntaxNode> _CodeNodeToSyntaxNode(CodeChunk &node, DecompileLookups &lookups);

void _ApplySyntaxNodeToCodeNode(CodeChunk &node, StatementsNode &statementsNode, DecompileLookups &lookups)
{
    unique_ptr<SingleStatement> pStatement = std::make_unique<SingleStatement>();
    pStatement->SetSyntaxNode(std::move(_CodeNodeToSyntaxNode(node, lookups)));
    statementsNode.AddStatement(std::move(pStatement));
}

void _ApplySyntaxNodeToCodeNode1(CodeChunk &node, OneStatementNode &statementsNode, DecompileLookups &lookups)
{
    unique_ptr<SingleStatement> pStatement = std::make_unique<SingleStatement>();
    pStatement->SetSyntaxNode(std::move(_CodeNodeToSyntaxNode(node, lookups)));
    statementsNode.SetStatement1(move(pStatement));
}

void _ApplySyntaxNodeToCodeNode2(CodeChunk &node, TwoStatementNode &statementsNode, DecompileLookups &lookups)
{
    unique_ptr<SingleStatement> pStatement = std::make_unique<SingleStatement>();
    pStatement->SetSyntaxNode(std::move(_CodeNodeToSyntaxNode(node, lookups)));
    statementsNode.SetStatement2(move(pStatement));
}

void _ApplySyntaxNodeToCodeNodeConditionNode(CodeChunk &node, ConditionNode &conditionNode, DecompileLookups &lookups)
{
    unique_ptr<ConditionalExpression> condExp = make_unique<ConditionalExpression>();
    _ApplySyntaxNodeToCodeNode(node, *condExp, lookups);
    conditionNode.SetCondition(move(condExp));
}

void _ApplyChildren(CodeChunk &node, StatementsNode &statementsNode, DecompileLookups &lookups)
{
    for (auto &child : node.Children())
    {
        _ApplySyntaxNodeToCodeNode(*child, statementsNode, lookups);
    }
}


std::unique_ptr<SyntaxNode> _CodeNodeToSyntaxNode2(CodeChunk &node, DecompileLookups &lookups)
{
    switch (node._chunkType)
    {
        case ChunkType::Break:
        {
            return unique_ptr<SyntaxNode>(new BreakStatement());
        }

        case ChunkType::If:
        {
            unique_ptr<CppIfStatement> ifStatement = make_unique<CppIfStatement>();
            _ApplySyntaxNodeToCodeNodeConditionNode(*node.GetChild(ChunkType::Condition), *ifStatement, lookups);
            _ApplySyntaxNodeToCodeNode1(*node.GetChild(ChunkType::Then), *ifStatement, lookups);
            if (node.GetChild(ChunkType::Else))
            {
                _ApplySyntaxNodeToCodeNode2(*node.GetChild(ChunkType::Else), *ifStatement, lookups);
            }
            return unique_ptr<SyntaxNode>(move(ifStatement));
        }

        case ChunkType::Condition:
            assert(node.GetChildCount() == 1);
            return _CodeNodeToSyntaxNode(*node.Child(0), lookups);

        case ChunkType::And:
        case ChunkType::Or:
        {
            unique_ptr<BinaryOp> binaryOp = make_unique<BinaryOp>();
            if (node.GetChild(ChunkType::FirstNegated))
            {
                unique_ptr<SingleStatement> negated = make_unique<SingleStatement>();
                unique_ptr<UnaryOp> unaryOp = make_unique<UnaryOp>();
                unaryOp->SetName("!");
                _ApplySyntaxNodeToCodeNode1(*node.GetChild(ChunkType::FirstNegated), *unaryOp, lookups);
                negated->SetSyntaxNode(move(unaryOp));
                binaryOp->SetStatement1(move(negated));
            }
            else
            {
                _ApplySyntaxNodeToCodeNode1(*node.GetChild(ChunkType::First), *binaryOp, lookups);
            }
            _ApplySyntaxNodeToCodeNode2(*node.GetChild(ChunkType::Second), *binaryOp, lookups);
            return unique_ptr<SyntaxNode>(move(binaryOp));
        }
        
        case ChunkType::Then:
        case ChunkType::Else:
        case ChunkType::CaseBody:
        {
            unique_ptr<CodeBlock> codeBlock = std::make_unique<CodeBlock>();
            _ApplyChildren(node, *codeBlock, lookups);
            return unique_ptr<SyntaxNode>(move(codeBlock));
        }
            
        case ChunkType::While:
        {
            unique_ptr<WhileLoop> whileLoop = make_unique<WhileLoop>();
            _ApplySyntaxNodeToCodeNodeConditionNode(*node.GetChild(ChunkType::Condition), *whileLoop, lookups);
            _ApplyChildren(*node.GetChild(ChunkType::LoopBody), *whileLoop, lookups);
            return unique_ptr<SyntaxNode>(move(whileLoop));
        }

        case ChunkType::Do:
        {
            unique_ptr<DoLoop> doLoop = make_unique<DoLoop>();
            _ApplySyntaxNodeToCodeNodeConditionNode(*node.GetChild(ChunkType::Condition), *doLoop, lookups);
            _ApplyChildren(*node.GetChild(ChunkType::LoopBody), *doLoop, lookups);
            return unique_ptr<SyntaxNode>(move(doLoop));
        }

        case ChunkType::Invert:
        {
            unique_ptr<UnaryOp> unaryOp = make_unique<UnaryOp>();
            unaryOp->SetName("!");
            assert(node.GetChildCount() == 1);
            _ApplySyntaxNodeToCodeNode1(*node.Child(0), *unaryOp, lookups);
            return unique_ptr<SyntaxNode>(move(unaryOp));
        }

        case ChunkType::Switch:
        {
            unique_ptr<SwitchStatement> switchStatement = make_unique<SwitchStatement>();
            
            for (auto &child : node.Children())
            {
                if (child->GetType() == ChunkType::Case)
                {
                    // A case should have two children. The case body and (optionally) a condition
                    unique_ptr<CaseStatement> caseStatement = make_unique<CaseStatement>();
                    CodeChunk *cond = child->GetChild(ChunkType::CaseCondition);
                    if (cond)
                    {
                        assert(cond->GetChildCount() == 1);
                        _ApplySyntaxNodeToCodeNode1(*cond->Child(0), *caseStatement, lookups);
                    }
                    else
                    {
                        caseStatement->SetDefault(true);
                    }
                    CodeChunk *body = child->GetChild(ChunkType::CaseBody);
                    _ApplySyntaxNodeToCodeNode(*body, *caseStatement, lookups);
                    switchStatement->AddCase(move(caseStatement));
                }
                else if (child->GetType() == ChunkType::SwitchValue)
                {
                    assert(child->GetChildCount() == 1);
                    _ApplySyntaxNodeToCodeNode1(*child->Child(0), *switchStatement, lookups);
                }
            }

            return unique_ptr<SyntaxNode>(move(switchStatement));
        }
    }


    std::unique_ptr<Comment> comment = std::make_unique<Comment>();
    comment->SetName("CONTROL STRUCTURE");
    return unique_ptr<SyntaxNode>(comment.release());
}

WORD _GetImmediateFromCodeNode(CodeChunk &node, CodeChunk *pNodePrevious = nullptr, bool assertIfNone = false, bool *foundOut = nullptr)
{
    bool found = true;
    WORD w = 0;
    if (node._hasPos)
    {
        code_pos pos = node.pos;
        switch (pos->get_opcode())
        {
            case Opcode::LDI:
                w = pos->get_first_operand();
                break;

            case Opcode::PUSHI:
                w = pos->get_first_operand();
                break;

            case Opcode::PUSH0:
                w = 0;
                break;

            case Opcode::PUSH1:
                w = 1;
                break;

            case Opcode::PUSH2:
                w = 2;
                break;

            case Opcode::PUSH:
                // REVIEW, hits too often..... ldi jmp ldi push
                //ASSERT(node.GetChildCount());
                break;

            case Opcode::DUP:
                if (pNodePrevious)
                {
                    w = _GetImmediateFromCodeNode(*pNodePrevious);
                }
                else
                {
                    code_pos posFlatPrevious = pos;
                    --posFlatPrevious;
                    // REVIEW: could crash...
                    CodeChunk nodeTemp;
                    nodeTemp.SetPos(posFlatPrevious);
                    w = _GetImmediateFromCodeNode(nodeTemp);
                }
                break;

            default:
                found = false;
                assert(!assertIfNone);
                break;
        }
    }
    if (foundOut)
    {
        *foundOut = found;
    }
    return w;
}

const char SelfToken[] = "self";
const char InvalidLookupError[] = "LOOKUP_ERROR";
const char RestParamName[] = "params";


// Looks for a rest instruction at index (and tracks it and adds to the SendParam). Returns true if it found one.
bool _MaybeConsumeRestInstruction(SendParam *pSendParam, int index, CodeChunk &node, DecompileLookups &lookups)
{
    if (index >= (int)node.GetChildCount())
    {
        return false;
    }

    code_pos restInstruction = node.Child(index)->GetCode();
    bool foundRest = (restInstruction->get_opcode() == Opcode::REST);
    if (foundRest)
    {
        std::unique_ptr<RestStatement> rest = std::make_unique<RestStatement>();
        // rest->SetName(RestParamName);
        // We'll set the name at a later time, because it might already have a name.

        // We need to tell the function that
        // someone used a rest instruction with this index:
        uint16_t parameterIndex = restInstruction->get_first_operand();
        assert(parameterIndex > 0); // otherwise, I don't know what I'm doing.
        // The rest statement will take on its name if so. Otherwise we'll use "params"
        lookups.TrackRestStatement(rest.get(), parameterIndex);

        unique_ptr<SingleStatement> pStatement = std::make_unique<SingleStatement>();
        pStatement->SetSyntaxNode(std::move(rest));
        pSendParam->AddStatement(std::move(pStatement));
    }
    return foundRest;
}

void _ProcessLEA(PropertyValueBase &value, code_pos pos, DecompileLookups &lookups)
{
    VarScope varScope;
    WORD wVarIndex;
    value.SetValue(_GetVariableNameFromCodePos(pos, lookups, &varScope, &wVarIndex), ValueType::Pointer);
    lookups.TrackVariableUsage(varScope, wVarIndex, true);  // Let's consider this indexed.
}

Consumption _GetInstructionConsumption(CodeChunk &node)
{
    if (node._hasPos)
    {
        return _GetInstructionConsumption(*node.GetCode());
    }
    else
    {
        // A control structure
        Consumption consumption = {};
        consumption.cAccGenerate = 1;
        return consumption;
    }
}

std::unique_ptr<SyntaxNode> _CodeNodeToSyntaxNode(CodeChunk &node, DecompileLookups &lookups)
{
    bool preferLValue = lookups.PreferLValue;
    PreferLValue resetPreferLValue(lookups, false); // Reset it, because if it's true it's not normal.

    if (!node._hasPos)
    {
        return _CodeNodeToSyntaxNode2(node, lookups);
    }
    code_pos pos = node.pos;
    Opcode bOpcode = pos->get_opcode();
    switch (bOpcode)
    {
        case Opcode::LDI:
        case Opcode::PUSHI:
        case Opcode::PUSH0:
        case Opcode::PUSH1:
        case Opcode::PUSH2:
        {
            unique_ptr<PropertyValue> value = std::make_unique<PropertyValue>();
            switch (bOpcode)
            {
                case Opcode::PUSH0:
                    value->SetValue(0);
                    break;
                case Opcode::PUSH1:
                    value->SetValue(1);
                    break;
                case Opcode::PUSH2:
                    value->SetValue(2);
                    break;
                default: // LDI and PUSHI
                    uint16_t theValue = pos->get_first_operand();
                    value->SetValue(theValue);
                    if (theValue == 65535)
                    {
                        // This was probably intended to be -1
                        value->Negate();
                    }
                    break;
            }
            return unique_ptr<SyntaxNode>(move(value));
        }
        break;

        case Opcode::PUSH:
            assert(node.Children().size() == 1);
            {
                // Fwd to the first child (should be what gets in to the accumulator)
                return _CodeNodeToSyntaxNode(*node.Children()[0], lookups);
            }
            break;

        case Opcode::CALLK:   // kernel, # of params
        case Opcode::CALLB:   // mainproc index, # of params
        case Opcode::CALLE:   // script, index, # of params
        case Opcode::CALL:    // offset, # of params
        {
            WORD cParams = (bOpcode == Opcode::CALLE) ? pos->get_third_operand() : pos->get_second_operand();
            cParams /= 2; // bytes -> params
            cParams += 1; // +1 because there is a parameter count first.
            std::stringstream ss;
            switch (bOpcode)
            {
                case Opcode::CALLK:
                    ss << lookups.LookupKernelName(pos->get_first_operand());
                    break;
                case Opcode::CALLB:
                    ss << "proc000_" << pos->get_first_operand();
                    break;
                case Opcode::CALLE:
                    ss << _GetPublicProcedureName(pos->get_first_operand(), pos->get_second_operand());
                    break;
                case Opcode::CALL:
                    ss << _GetProcNameFromScriptOffset(pos->get_final_postop_offset() + pos->get_first_operand());
                    break;
            }
            unique_ptr<ProcedureCall> pProcCall = std::make_unique<ProcedureCall>();
            pProcCall->SetName(ss.str());
            for (WORD i = 0; i < node.GetChildCount(); ++i)
            {
                if (i < cParams)
                {
                    if (i == 0)
                    {
                        // The first one should just be the number of parameters.
                        WORD cParamsCheck = _GetImmediateFromCodeNode(*node.Child(i));
                        ASSERT((cParamsCheck + 1) == cParams);
                    }
                    else
                    {
                        _ApplySyntaxNodeToCodeNode(*node.Child(i), *pProcCall, lookups);
                    }
                }
                else
                {
                    if (i == cParams)
                    {
                        // This might be a rest instruction.
                        _ApplySyntaxNodeToCodeNode(*node.Child(i), *pProcCall, lookups);
                    }
                    else
                    {
                        assert(false);  // How would we get here?
                    }
                }
            }

            _MassageProcedureCall(*pProcCall, lookups, pos);
            return unique_ptr<SyntaxNode>(move(pProcCall));
        }
        break;

        case Opcode::SELF:
        case Opcode::SEND:
        case Opcode::SUPER:
        {
            unique_ptr<SendCall> sendCall = std::make_unique<SendCall>();
            if (bOpcode == Opcode::SELF)
            {
                sendCall->SetName(SelfToken);
            }
            else if (bOpcode == Opcode::SUPER)
            {
                // TODO - actually you can send to any super class... the first operand says which
                sendCall->SetName("super");
            }
            Consumption cons = _GetInstructionConsumption(*pos);
            WORD cStackPushesLeft = cons.cStackConsume;
            WORD cAccLeft = (bOpcode == Opcode::SEND) ? 1 : 0;
            size_t i = 0;
            WORD cParamsLeft = 0;
            bool fLookingForSelector = true;
            unique_ptr<SendParam> sendParam;

            while ((cAccLeft || cStackPushesLeft) && (i < node.GetChildCount()))
            {
                CodeChunk *pPreviousChild = i ? node.Child(i - 1) : nullptr;
                if (cAccLeft)
                {
                    Consumption consAcc = _GetInstructionConsumption(*node.Child(i));
                    if (consAcc.cAccGenerate)
                    {
                        // We want an LValue instead of a property value. Tell _CodeNodeToSyntaxNode this.
                        PreferLValue preferLValue(lookups, true);
                        unique_ptr<SyntaxNode> pSendObject = _CodeNodeToSyntaxNode(*node.Child(i), lookups);
                        const LValue *pValue = SafeSyntaxNode<LValue>(pSendObject.get());
                        if (pValue)
                        {
                            unique_ptr<LValue> lValue(static_cast<LValue*>(pSendObject.release()));
                            sendCall->SetLValue(move(lValue));
                        }
                        else
                        {
                            unique_ptr<SingleStatement> temp = make_unique<SingleStatement>();
                            temp->SetSyntaxNode(move(pSendObject));
                            sendCall->SetStatement1(move(temp));
                        }
                        --cAccLeft;
                    }
                }

                if (cStackPushesLeft)
                {
                    Consumption consAcc = _GetInstructionConsumption(*node.Child(i));
                    if (consAcc.cStackGenerate)
                    {
                        if (fLookingForSelector)
                        {
                            sendParam.reset(new SendParam);
                            fLookingForSelector = false;
                            bool found;
                            WORD wValue = _GetImmediateFromCodeNode(*node.Child(i), pPreviousChild, false, &found);
                            if (!found)
                            {
                                // Occasionally selectors can be variables.
                                std::unique_ptr<SyntaxNode> syntaxNode = _CodeNodeToSyntaxNode(*node.Child(i), lookups);
                                const LValue *pLValue = SafeSyntaxNode<LValue>(syntaxNode.get());
                                if (pLValue)
                                {
                                    sendParam->SetName(pLValue->GetName());
                                }
                                else
                                {
                                    assert(false);
                                }
                            }
                            else
                            {
                                sendParam->SetName(lookups.LookupSelectorName(wValue));
                                sendParam->SetIsMethod(!_MaybeAProperty(sendParam->GetName()));
                            }
                        }
                        else if (cParamsLeft)
                        {
                            --cParamsLeft;
                            _ApplySyntaxNodeToCodeNode(*node.Child(i), *sendParam, lookups); // Hmm, we were passing the wrong previous node here before... will that matter?
                            if (cParamsLeft == 0)
                            {
                                if (_MaybeConsumeRestInstruction(sendParam.get(), i + 1, node, lookups))
                                {
                                    i++;
                                }
                                sendCall->AddSendParam(std::move(sendParam));
                                fLookingForSelector = true;
                            }
                        }
                        else
                        {
                            // Must be a param count
                            cParamsLeft = _GetImmediateFromCodeNode(*node.Child(i), pPreviousChild);
                            if (cParamsLeft == 0)
                            {
                                if (_MaybeConsumeRestInstruction(sendParam.get(), i + 1, node, lookups))
                                {
                                    i++;
                                }
                                sendCall->AddSendParam(std::move(sendParam));
                                fLookingForSelector = true;
                            }
                        }
                        --cStackPushesLeft;
                    }
                }

                ++i; // Always increment i
                // TODO: warn if we didn't do anything in this loop?  Unused instruction???
            }

            sendCall->SimplifySendObject();
            return unique_ptr<SyntaxNode>(move(sendCall));
        }
        break;

        case Opcode::BNT:
        case Opcode::BT:
            assert(node.GetChildCount() == 1);
            if (node.GetChildCount() >= 1)
            {
                // This should have one child, which is the condition...
                return _CodeNodeToSyntaxNode(*node.Child(0), lookups);
            }
            break;

        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::MOD:
        case Opcode::SHR:
        case Opcode::SHL:
        case Opcode::XOR:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::ADD:
        case Opcode::EQ:
        case Opcode::GT:
        case Opcode::LT:
        case Opcode::LE:
        case Opcode::NE:
        case Opcode::GE:
        case Opcode::UGT:
        case Opcode::UGE:
        case Opcode::ULT:
        case Opcode::ULE:
            if (node.GetChildCount() >= 2)
            {
                unique_ptr<BinaryOp> binaryOp = std::make_unique<BinaryOp>();
                // The child that generates the stack push should be the first statement.
                CodeChunk *pCodeNodeStack = node.Child(0);
                CodeChunk *pCodeNodeAcc = node.Child(1);
                Consumption cons = _GetInstructionConsumption(*pCodeNodeStack);
                if (!cons.cStackGenerate)
                {
                    // Ooops... the other one must have been the stack-generating one.
                    swap(pCodeNodeStack, pCodeNodeAcc);
                }
                _ApplySyntaxNodeToCodeNode1(*pCodeNodeStack, *binaryOp, lookups);
                _ApplySyntaxNodeToCodeNode2(*pCodeNodeAcc, *binaryOp, lookups);
                binaryOp->SetName(GetBinaryOperatorForInstruction(bOpcode, LangSyntaxSCIStudio));
                return unique_ptr<SyntaxNode>(move(binaryOp));
            }
            break;

        case Opcode::BNOT:
        case Opcode::NOT:
        case Opcode::NEG:
            if (node.GetChildCount() >= 1)
            {
                unique_ptr<UnaryOp> unaryOp = std::make_unique<UnaryOp>();
                CodeChunk *pCodeNodeAcc = node.Child(0);
                Consumption cons = _GetInstructionConsumption(*pCodeNodeAcc);
                assert(cons.cAccGenerate);
                _ApplySyntaxNodeToCodeNode1(*pCodeNodeAcc, *unaryOp, lookups);
                unaryOp->SetName(GetUnaryOperatorForInstruction(bOpcode, LangSyntaxSCIStudio));
                return unique_ptr<SyntaxNode>(move(unaryOp));
            }
            break;

        case Opcode::REST:
        {
            unique_ptr<PropertyValue> pValue = std::make_unique<PropertyValue>();
            pValue->SetValue("rest", ValueType::Token);
            return unique_ptr<SyntaxNode>(move(pValue));
        }
        break;

        case Opcode::LEA:
        {
            WORD wType = pos->get_first_operand();
            bool hasIndexer = ((wType >> 1) & LEA_ACC_AS_INDEX_MOD) == LEA_ACC_AS_INDEX_MOD;
            if (hasIndexer)
            {
                unique_ptr<ComplexPropertyValue> pValue = std::make_unique<ComplexPropertyValue>();

                assert(node.GetChildCount() == 1);
                unique_ptr<SyntaxNode> pIndexerNode = _CodeNodeToSyntaxNode(*node.Child(0), lookups);
                unique_ptr<SingleStatement> temp = make_unique<SingleStatement>();
                temp->SetSyntaxNode(move(pIndexerNode));
                pValue->SetIndexer(move(temp));

                _ProcessLEA(*pValue, pos, lookups);
                return unique_ptr<SyntaxNode>(move(pValue));
            }
            else
            {
                unique_ptr<PropertyValue> pValue = std::make_unique<PropertyValue>();
                _ProcessLEA(*pValue, pos, lookups);
                return unique_ptr<SyntaxNode>(move(pValue));
            }
        }
        break;

        case Opcode::CLASS:
        case Opcode::PUSHSELF:
        {
            std::string className = (bOpcode == Opcode::CLASS) ? lookups.LookupClassName(pos->get_first_operand()) : SelfToken;
            if (!className.empty())
            {
                unique_ptr<PropertyValue> value = std::make_unique<PropertyValue>();
                value->SetValue(className, ValueType::Token);
                return unique_ptr<SyntaxNode>(move(value));
            }
        }
        break;

        case Opcode::LOFSA:
        case Opcode::LOFSS:
        {
            // The first operand specifies an offset from the start of the next instruction.
            // In SCI0 is this a relative offset from the post operation program counter.
            // In SCI1 it appears to be an absolute offset.
            ICompiledScriptSpecificLookups::ObjectType type;
            SCIVersion version = appState->GetVersion();
            uint16_t wName = version.lofsaOpcodeIsAbsolute ?
                pos->get_first_operand() :
                (pos->get_first_operand() + pos->get_final_postop_offset());
            std::string name = lookups.LookupScriptThing(wName, type);
            if (name.empty())
            {
                name = InvalidLookupError;
                appState->LogInfo("Unable to find symbol for %d.", wName);
            }
            unique_ptr<PropertyValue> value = std::make_unique<PropertyValue>();
            value->SetValue(name, _ScriptObjectTypeToPropertyValueType(type));
            return unique_ptr<SyntaxNode>(move(value));
        }
        break;

        case Opcode::ATOP:    // acc to property index
        case Opcode::STOP:    // Stack to property index
        {
            unique_ptr<Assignment> pAssignment = std::make_unique<Assignment>();
            if (node.GetChildCount())
            {
                _ApplySyntaxNodeToCodeNode1(*node.Child(0), *pAssignment, lookups);
            }
            // Now this is a property... find out which.
            WORD wPropertyIndex = node.GetCode()->get_first_operand();
            unique_ptr<LValue> lValue = make_unique<LValue>();
            lValue->SetName(lookups.LookupPropertyName(wPropertyIndex));
            pAssignment->SetVariable(move(lValue));
            pAssignment->SetName("=");
            return unique_ptr<SyntaxNode>(move(pAssignment));
        }
        break;

        case Opcode::PTOS:    // property index to stack
        case Opcode::PTOA:    // property index to acc
        case Opcode::IPTOA:   // Inc prop to acc
        case Opcode::DPTOA:   // Dec prop to acc
        case Opcode::IPTOS:   // Inc prop to stack
        case Opcode::DPTOS:   // Dec prop to stack
        {
            unique_ptr<PropertyValue> pValue = std::make_unique<PropertyValue>();
            ASSERT(node.GetChildCount() == 0);
            WORD wPropertyIndex = node.GetCode()->get_first_operand();
            pValue->SetValue(lookups.LookupPropertyName(wPropertyIndex), ValueType::Token);
            bool fIncrement = (bOpcode == Opcode::IPTOA) || (bOpcode == Opcode::IPTOS);
            bool fDecrement = (bOpcode == Opcode::DPTOA) || (bOpcode == Opcode::DPTOS);
            if (fIncrement || fDecrement)
            {
                // Phil - TODO - copied code with below..
                unique_ptr<UnaryOp> pUnary = std::make_unique<UnaryOp>();
                pUnary->SetName(fIncrement ? "++" : "--");
                unique_ptr<SingleStatement> temp = make_unique<SingleStatement>();
                temp->SetSyntaxNode(std::move(pValue));
                pUnary->SetStatement1(std::move(temp));
                return unique_ptr<SyntaxNode>(move(pUnary));
            }
            else
            {
                if (preferLValue && (pValue->GetType() == ValueType::Token))
                {
                    // Come callers want an LValue instead
                    unique_ptr<LValue> lValue = make_unique<LValue>();
                    lValue->SetName(pValue->GetStringValue());
                    return unique_ptr<SyntaxNode>(move(lValue));
                }
                else
                {
                    return unique_ptr<SyntaxNode>(move(pValue));
                }
            }
        }
        break;

        case Opcode::DUP:
            assert(false); // In what conditions do we hit this? Should already be handled in switch statements.
            /*
            if (node.GetPrevious())
            {
                return _CodeNodeToSyntaxNode(*node.GetPrevious(), lookups);
            }
            else
            {
                // TODO: Walk backwards until we have an instruction that puts something on the stack.
                // This may not work in all cases (if we pass a branch, etc...)
                appState->LogInfo("WARNING: Possible incorrect logic.");
                // How do we handle this one?
                // assert(false);
            }*/
            break;

        case Opcode::JMP:
            // Return empty comment.
            return unique_ptr<SyntaxNode>(new Comment());
            // Already handled by branching code
            // OH BUT IS IT
            // I don't think this is needed anymore
            /*
            if (lookups.BreakExit._Ptr)
            {
                if (pos->get_branch_target() == (*lookups.BreakExit)->GetCode())
                {
                    // This jump points to the break exit.
                    return unique_ptr<SyntaxNode>(new BreakStatement());
                }
            }

            return unique_ptr<SyntaxNode>(new Comment());*/

        case Opcode::LINK:
            return unique_ptr<SyntaxNode>(new Comment());
            break;

        case Opcode::RET:
        {
            unique_ptr<ReturnStatement> pReturn = std::make_unique<ReturnStatement>();
            if (node.GetChildCount())
            {
                _ApplySyntaxNodeToCodeNode1(*node.Child(0), *pReturn, lookups);
            }
            return unique_ptr<SyntaxNode>(move(pReturn));
            // TODO: Difficult to determine if the code actually meant to return anything...
            // we'll need to check if the previous thing was "intentional" or not...
            // We could have trouble with branching...
        }
        break;

        case Opcode::SELFID:
        {
            unique_ptr<PropertyValue> pv = make_unique<PropertyValue>();
            // REVIEW: Given that different syntaxes might use different keywords
            // for "self", it is questionable to have it as a string token here.
            pv->SetValue(SelfToken, ValueType::Token);
            return unique_ptr<SyntaxNode>(move(pv));
        }
        break;

        default:
            if ((bOpcode >= Opcode::LAG) && (bOpcode <= Opcode::LastOne))
            {
                // This could be a load or store operation.
                if (_IsVOStoreOperation(bOpcode))
                {
                    // Store operation: Assignment (LValue/PropertyValue)
                    unique_ptr<Assignment> pAssignment = std::make_unique<Assignment>();
                    pAssignment->SetName("=");

                    // The variable name
                    unique_ptr<LValue> lValue = make_unique<LValue>();
                    WORD wVarIndex;
                    VarScope varScope;
                    lValue->SetName(_GetVariableNameFromCodePos(pos, lookups, &varScope, &wVarIndex));
                    bool isIndexed = _IsVOIndexed(bOpcode);
                    lookups.TrackVariableUsage(varScope, wVarIndex, isIndexed);
                    if (isIndexed) // The accumulator is used as an indexer.
                    {
                        for (size_t i = 0; i < node.GetChildCount(); i++)
                        {
                            Consumption cons = _GetInstructionConsumption(*node.Child(i));
                            if (cons.cAccGenerate)
                            {
                                unique_ptr<SyntaxNode> pIndexerNode = _CodeNodeToSyntaxNode(*node.Child(i), lookups);
                                unique_ptr<SingleStatement> temp = make_unique<SingleStatement>();
                                temp->SetSyntaxNode(move(pIndexerNode));
                                lValue->SetIndexer(move(temp));
                                break;
                            }
                        }
                    }
                    pAssignment->SetVariable(move(lValue));

                    // Is assigned our child:
                    ASSERT(node.GetChildCount() >= 1);
                    for (size_t i = 0; i < node.GetChildCount(); i++)
                    {
                        Consumption cons = _GetInstructionConsumption(*node.Child(i));
                        if ((_IsVOStack(bOpcode) && cons.cStackGenerate) || (!_IsVOStack(bOpcode) && cons.cAccGenerate))
                        {
                            _ApplySyntaxNodeToCodeNode1(*node.Child(i), *pAssignment, lookups);
                        }
                    }
                    return unique_ptr<SyntaxNode>(move(pAssignment));
                }
                else
                {
                    // Load operation - make PropertyValue
                    std::unique_ptr<PropertyValueBase> pValue;

                    // Does it have an indexer?
                    bool isIndexed = _IsVOIndexed(bOpcode);
                    if (isIndexed)
                    {
                        pValue.reset(new ComplexPropertyValue);
                        // Then it should have a child
                        if (node.GetChildCount() >= 1)
                        {
                            unique_ptr<SingleStatement> pStatement = std::make_unique<SingleStatement>();
                            pStatement->SetSyntaxNode(std::move(_CodeNodeToSyntaxNode(*node.Child(0), lookups)));
                            static_cast<ComplexPropertyValue*>(pValue.get())->SetIndexer(std::move(pStatement));
                        }
                        else
                        {
                            ASSERT(FALSE); // REVIEW, TODO 
                        }
                    }
                    else
                    {
                        pValue.reset(new PropertyValue);
                    }

                    VarScope varScope;
                    WORD wIndex;
                    pValue->SetValue(_GetVariableNameFromCodePos(pos, lookups, &varScope, &wIndex), ValueType::Token);
                    lookups.TrackVariableUsage(varScope, wIndex, isIndexed);

                    // If it has an incrementer or decrementer, wrap it in a unary operator first.
                    bool fIncrement = _IsVOIncremented(bOpcode);
                    bool fDecrement = _IsVODecremented(bOpcode);
                    if (fIncrement || fDecrement)
                    {
                        unique_ptr<UnaryOp> pUnary = std::make_unique<UnaryOp>();
                        pUnary->SetName(fIncrement ? "++" : "--");
                        unique_ptr<SingleStatement> temp = make_unique<SingleStatement>();
                        temp->SetSyntaxNode(move(pValue));
                        pUnary->SetStatement1(std::move(temp));
                        return unique_ptr<SyntaxNode>(move(pUnary));
                    }
                    else
                    {
                        if (preferLValue && (pValue->GetType() == ValueType::Token))
                        {
                            // Come callers want an LValue instead
                            unique_ptr<LValue> lValue = make_unique<LValue>();
                            lValue->SetName(pValue->GetStringValue());
                            if (isIndexed)
                            {
                                lValue->SetIndexer(move(static_cast<ComplexPropertyValue*>(pValue.get())->StealIndexer()));
                            }
                            return unique_ptr<SyntaxNode>(move(lValue));
                        }
                        else
                        {
                            return unique_ptr<SyntaxNode>(move(pValue));
                        }
                    }
                }
            }
            break;
    }

    // Up next: Opcode::TOSS
    unique_ptr<Comment> pComment = std::make_unique<Comment>();
    pComment->SetName("ERROR_UNIMPLEMENTED_OPCODE");
    return unique_ptr<SyntaxNode>(move(pComment));
}

void _FixupSwitches(CodeChunk *chunk)
{
    for (size_t i = 0; i < chunk->GetChildCount(); )
    {
        CodeChunk *child = chunk->Child(i);
        if (child->GetType() == ChunkType::Switch)
        {
            assert(i > 0);
            if (i < (chunk->GetChildCount() - 1))
            {
                // Look for a toss instruction and get rid of it.
                CodeChunk *next = chunk->Child(i + 1);
                assert((next->_hasPos) && (next->GetCode()->get_opcode() == Opcode::TOSS));
                unique_ptr<CodeChunk> deleteMe = chunk->StealChild(i + 1);
            }

            if (i > 0)
            {
                // Steal the previous guy and add it to switch's children
                // as the switch value.
                unique_ptr<CodeChunk> prev = chunk->StealChild(i - 1);
                Consumption testConsumption = _GetInstructionConsumption(*prev);
                assert((testConsumption.cAccGenerate == 0) && (testConsumption.cStackGenerate == 1));
                CodeChunk *switchValue = child->PrependChild(); // Add a new node that will be the switch value.
                switchValue->SetType(ChunkType::SwitchValue);
                switchValue->PrependChild(move(prev));
                continue;
            }
        }
        i++;
    }
    for (auto &child : chunk->Children())
    {
        _FixupSwitches(child.get());
    }
}

void _RestructureCaseHeaders(CodeChunk *chunk)
{
    if (chunk->GetType() == ChunkType::CaseCondition)
    {
        // This should look like this:
        // [CaseCondition]
        //   bnt
        //      eq ?
        //        dup
        //         ldi   // Something that puts stuff in the accumulator
        //
        // We want to restructure it so it looks like:
        // [CaseCondition]
        //   ldi   // Something that puts stuff in the accumulator
        //
        CodeChunk *bnt = chunk->Child(0);
        CodeChunk *eq = bnt->Child(0);
        CodeChunk *dup = eq->Child(0);
        unique_ptr<CodeChunk> putInAcc = eq->StealChild(1);
        assert(_GetInstructionConsumption(*putInAcc).cAccGenerate == 1);
        unique_ptr<CodeChunk> deleteMe = chunk->StealChild(0);
        assert(chunk->GetChildCount() == 0);
        chunk->PrependChild(move(putInAcc));
    }

    for (auto &child : chunk->Children())
    {
        _RestructureCaseHeaders(child.get());
    }
}


void OutputNewStructure(sci::FunctionBase &func, MainNode &main, DecompileLookups &lookups)
{
    CodeChunkEnumContext context;
    unique_ptr<CodeChunk> mainChunk = make_unique<CodeChunk>();
    context.Current = mainChunk.get();
    EnumerateCodeChunks enumCodeChunks(context);
    enumCodeChunks.Visit(main);

    _FixupSwitches(mainChunk.get());
    _RestructureCaseHeaders(mainChunk.get());
    //_FixupOtherThing(mainChunk.get());

    std::stringstream ss;
    mainChunk->Print(ss, 0);
    ShowTextFile(ss.str().c_str(), func.GetName() + "_chunks.txt");

    // Now fill it in
    for (auto &child : mainChunk->Children())
    {
        _ApplySyntaxNodeToCodeNode(*child, func, lookups);
    }
}