#include "TargetCodeTrace.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 这个文件是 GUI 后端展示模块。
//
// 核心运行真正使用的是 VirtualMachine.cpp 中的 VM 指令生成和执行。
// 这里额外生成“目标代码展示表”和 RDL/SEM 轨迹，是为了课程设计界面展示：
// - 哪个基本块生成了哪些目标代码；
// - 每条四元式对应哪些目标指令；
// - 变量在基本块内是否仍然活跃。

namespace
{

bool isNumberLiteral(const std::string& s)
{
    // 判断字符串是否为数字常量。
    if (s.empty())
    {
        return false;
    }

    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool isTempName(const std::string& s)
{
    // 判断名字是否像临时变量 t1、t2。
    if (s.size() < 2 || s[0] != 't')
    {
        return false;
    }

    for (std::size_t i = 1; i < s.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
        {
            return false;
        }
    }

    return true;
}

bool isVariableOperand(const std::string& s)
{
    // 过滤掉空占位符、数字常量、字符常量，剩下的才按变量处理。
    if (s.empty() || s == "_")
    {
        return false;
    }
    if (isNumberLiteral(s))
    {
        return false;
    }
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'')
    {
        return false;
    }

    return true;
}

std::string opToTarget(const std::string& op)
{
    // 把四元式运算符转换成展示用目标指令名。
    if (op == "+") return "ADD";
    if (op == "-") return "SUB";
    if (op == "*") return "MUL";
    if (op == "/") return "DIV";
    if (op == "<") return "LT";
    if (op == ">") return "GT";
    if (op == "=") return "EQ";
    if (op == "!=") return "NE";
    if (op == "<=") return "LE";
    if (op == ">=") return "GE";
    return "";
}

struct QuadLiveInfo
{
    // 某条四元式的三个操作数字段在该位置之后是否还会被使用。
    bool arg1Live = false;
    bool arg2Live = false;
    bool resultLive = false;
};

std::vector<QuadLiveInfo> buildLiveness(
    const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks,
    const std::vector<Symbol>& symbols)
{
    // 活跃信息分析。
    // 它从基本块末尾往前看：如果一个变量后面还要读，就是 live。
    // 这个信息只用于 GUI 展示，不影响 VirtualMachine 的真实执行。
    std::unordered_set<std::string> temporaryNames;
    for (const auto& s : symbols)
    {
        if (s.kind == "temporary")
        {
            temporaryNames.insert(s.name);
        }
    }

    auto isTemp = [&](const std::string& v)
    {
        return temporaryNames.find(v) != temporaryNames.end() || isTempName(v);
    };

    std::vector<QuadLiveInfo> live(quads.size());
    for (const auto& block : blocks)
    {
        if (block.start < 0 || block.end >= static_cast<int>(quads.size()) || block.end < block.start)
        {
            continue;
        }

        std::unordered_map<std::string, bool> symLive;
        for (int i = block.start; i <= block.end; ++i)
        {
            const Quadruple& q = quads[i];
            if (isVariableOperand(q.arg1) && symLive.find(q.arg1) == symLive.end()) symLive[q.arg1] = !isTemp(q.arg1);
            if (isVariableOperand(q.arg2) && symLive.find(q.arg2) == symLive.end()) symLive[q.arg2] = !isTemp(q.arg2);
            if (isVariableOperand(q.result) && symLive.find(q.result) == symLive.end()) symLive[q.result] = !isTemp(q.result);
        }

        for (int i = block.end; i >= block.start; --i)
        {
            const Quadruple& q = quads[i];
            if (isVariableOperand(q.result))
            {
                live[i].resultLive = symLive[q.result];
                symLive[q.result] = false;
            }
            if (isVariableOperand(q.arg1))
            {
                live[i].arg1Live = symLive[q.arg1];
                symLive[q.arg1] = true;
            }
            if (isVariableOperand(q.arg2))
            {
                live[i].arg2Live = symLive[q.arg2];
                symLive[q.arg2] = true;
            }
        }
    }

    return live;
}

struct GeneratedInstruction
{
    // 生成中的目标指令。
    // target 用来暂存跳转目标，最后再转成标签文本。
    std::string op;
    std::string a;
    std::string b;
    std::string c;
    int target = -1;
};

struct IfBranchContext
{
    int ifQuadIndex = -1;
    int elseQuadIndex = -1;
};

struct CodegenState
{
    std::vector<GeneratedInstruction> generatedInstructions;
    std::string registerDescriptor;
    std::vector<int> controlFlowStack;
    int pendingWhileEntryInstructionIndex = -1;
};

struct QuadProcessMeta
{
    int poppedInstructionIndex = -1;
    int poppedInstructionIndex2 = -1;
    int pushedInstructionIndex = -1;
    int firstInstructionIndex = 0;
};

std::map<int, int> buildQuadToBlock(const std::vector<Quadruple>& quads, const std::vector<BasicBlock>& blocks)
{
    // 建立“四元式下标 -> 基本块编号”的映射，方便目标代码表显示 B1/B2。
    std::map<int, int> quadToBlock;
    for (const auto& b : blocks)
    {
        for (int i = b.start; i <= b.end; ++i)
        {
            if (i >= 0 && i < static_cast<int>(quads.size()))
            {
                quadToBlock[i] = b.id;
            }
        }
    }
    return quadToBlock;
}

void buildIfMaps(
    const std::vector<Quadruple>& quads,
    std::map<int, int>& ifToEndIf,
    std::map<int, int>& ifToElse)
{
    // 配对 if/el/ie，得到 if 的 false 分支和结束位置。
    std::vector<IfBranchContext> ifContextStack;
    for (int i = 0; i < static_cast<int>(quads.size()); ++i)
    {
        if (quads[i].op == "if")
        {
            ifContextStack.push_back({i, -1});
        }
        else if (quads[i].op == "el")
        {
            if (!ifContextStack.empty())
            {
                ifContextStack.back().elseQuadIndex = i;
            }
        }
        else if (quads[i].op == "ie")
        {
            if (!ifContextStack.empty())
            {
                ifToEndIf[ifContextStack.back().ifQuadIndex] = i;
                ifToElse[ifContextStack.back().ifQuadIndex] = ifContextStack.back().elseQuadIndex;
                ifContextStack.pop_back();
            }
        }
    }
}

int appendInstruction(CodegenState& state, const std::string& op, const std::string& a, const std::string& b, const std::string& c, int target = -1)
{
    state.generatedInstructions.push_back({op, a, b, c, target});
    return static_cast<int>(state.generatedInstructions.size()) - 1;
}

void flushRegisterToVariable(CodegenState& state)
{
    if (!state.registerDescriptor.empty() && isVariableOperand(state.registerDescriptor))
    {
        appendInstruction(state, "ST", "R0", state.registerDescriptor, "_");
    }
    state.registerDescriptor.clear();
}

void loadVariableToRegister(CodegenState& state, const std::string& name)
{
    if (state.registerDescriptor == name)
    {
        return;
    }

    appendInstruction(state, "LD", "R0", name, "_");
    state.registerDescriptor = name;
}

void handleArithmeticOrAssign(const Quadruple& q, const std::string& targetOp, CodegenState& state)
{
    if (!targetOp.empty())
    {
        loadVariableToRegister(state, q.arg1);
        appendInstruction(state, targetOp, "R0", "R0", q.arg2);
        state.registerDescriptor = q.result;
        return;
    }

    if (q.op == ":=")
    {
        loadVariableToRegister(state, q.arg1);
        state.registerDescriptor = q.result;
    }
}

void handleControlFlow(
    const Quadruple& q,
    int instBegin,
    CodegenState& state,
    QuadProcessMeta& meta)
{
    if (q.op == "if" || q.op == "do")
    {
        loadVariableToRegister(state, q.arg1);
        int falseJumpInstructionIndex = appendInstruction(state, "FJ", "R0", "_", "_", -1);
        state.controlFlowStack.push_back(falseJumpInstructionIndex);
        meta.pushedInstructionIndex = falseJumpInstructionIndex;
        state.registerDescriptor.clear();
        return;
    }

    if (q.op == "el")
    {
        flushRegisterToVariable(state);
        int falseJumpInstructionIndex = -1;
        if (!state.controlFlowStack.empty())
        {
            falseJumpInstructionIndex = state.controlFlowStack.back();
            state.controlFlowStack.pop_back();
            meta.poppedInstructionIndex = falseJumpInstructionIndex;
        }

        int jumpToEndInstructionIndex = appendInstruction(state, "JMP", "_", "_", "_", -1);
        if (falseJumpInstructionIndex >= 0)
        {
            state.generatedInstructions[falseJumpInstructionIndex].target = jumpToEndInstructionIndex + 1;
        }

        state.controlFlowStack.push_back(jumpToEndInstructionIndex);
        meta.pushedInstructionIndex = jumpToEndInstructionIndex;
        return;
    }

    if (q.op == "ie")
    {
        int endIfEntryInstructionIndex = instBegin;
        flushRegisterToVariable(state);
        if (!state.controlFlowStack.empty())
        {
            int pendingJumpInstructionIndex = state.controlFlowStack.back();
            state.controlFlowStack.pop_back();
            meta.poppedInstructionIndex = pendingJumpInstructionIndex;
            state.generatedInstructions[pendingJumpInstructionIndex].target = endIfEntryInstructionIndex;
        }
        return;
    }

    if (q.op == "wh")
    {
        state.pendingWhileEntryInstructionIndex = instBegin;
        return;
    }

    if (q.op == "we")
    {
        flushRegisterToVariable(state);
        int doFalseJumpInstructionIndex = -1;
        int whileEntryInstructionIndex = -1;
        if (!state.controlFlowStack.empty())
        {
            doFalseJumpInstructionIndex = state.controlFlowStack.back();
            state.controlFlowStack.pop_back();
            meta.poppedInstructionIndex = doFalseJumpInstructionIndex;
        }
        if (!state.controlFlowStack.empty())
        {
            whileEntryInstructionIndex = state.controlFlowStack.back();
            state.controlFlowStack.pop_back();
            meta.poppedInstructionIndex2 = whileEntryInstructionIndex;
        }

        int jumpBackInstructionIndex = appendInstruction(state, "JMP", "_", "_", "_", whileEntryInstructionIndex);
        if (doFalseJumpInstructionIndex >= 0)
        {
            state.generatedInstructions[doFalseJumpInstructionIndex].target = jumpBackInstructionIndex + 1;
        }
    }
}

void handleCallAndReturn(const Quadruple& q, CodegenState& state)
{
    if (q.op == "param")
    {
        appendInstruction(state, "ARG", q.arg1, "_", "_");
        return;
    }

    if (q.op == "call")
    {
        appendInstruction(state, "CALL", q.arg1, q.arg2, "_");
        if (q.result != "_" && !q.result.empty())
        {
            appendInstruction(state, "MOVRET", "R0", "_", "_");
            appendInstruction(state, "ST", "R0", q.result, "_");
            state.registerDescriptor.clear();
        }
        return;
    }

    if (q.op == "ret")
    {
        appendInstruction(state, "RET", q.arg1, "_", "_");
        state.registerDescriptor.clear();
    }
}

std::string quadText(int index, const Quadruple& q)
{
    return std::to_string(index) + ": (" + q.op + ", " + q.arg1 + ", " + q.arg2 + ", " + q.result + ")";
}

std::string instTextRange(const std::vector<GeneratedInstruction>& generatedInstructions, int begin, int end)
{
    if (begin >= static_cast<int>(generatedInstructions.size()))
    {
        return "-";
    }

    int realEnd = std::min(end, static_cast<int>(generatedInstructions.size()) - 1);
    std::string text;
    for (int i = begin; i <= realEnd; ++i)
    {
        const GeneratedInstruction& in = generatedInstructions[i];
        if (!text.empty())
        {
            text += " ; ";
        }

        std::string showB = in.b;
        if ((in.op == "FJ" || in.op == "TJ" || in.op == "JMP") && in.target >= 0)
        {
            showB = "L" + std::to_string(in.target);
        }
        text += in.op + " " + in.a + ", " + showB + ", " + in.c;
    }

    return text;
}

TargetTraceItem makeTraceItem(
    int quadIndex,
    const Quadruple& q,
    int instBegin,
    int instEnd,
    const std::map<int, int>& quadToBlock,
    const CodegenState& state,
    const QuadProcessMeta& meta)
{
    TargetTraceItem item;

    auto blockIterator = quadToBlock.find(quadIndex);
    item.basicBlock = (blockIterator == quadToBlock.end()) ? "_" : ("B" + std::to_string(blockIterator->second + 1));
    item.quadruple = quadText(quadIndex, q);
    item.targetCode = std::to_string(instBegin) + ":" + std::to_string(instEnd);
    item.rdl = state.registerDescriptor.empty() ? "0" : state.registerDescriptor;

    if (meta.poppedInstructionIndex >= 0 && meta.poppedInstructionIndex2 >= 0)
    {
        item.sem = std::to_string(meta.poppedInstructionIndex + 1) + "✓ " + std::to_string(meta.poppedInstructionIndex2 + 1) + "✓";
    }
    else if (meta.poppedInstructionIndex >= 0 && meta.pushedInstructionIndex >= 0)
    {
        item.sem = std::to_string(meta.poppedInstructionIndex + 1) + "✓ " + std::to_string(meta.pushedInstructionIndex + 1);
    }
    else if (meta.poppedInstructionIndex >= 0)
    {
        item.sem = std::to_string(meta.poppedInstructionIndex + 1) + "✓";
    }
    else if (meta.poppedInstructionIndex2 >= 0)
    {
        item.sem = std::to_string(meta.poppedInstructionIndex2 + 1) + "✓";
    }
    else if (meta.pushedInstructionIndex >= 0)
    {
        item.sem = std::to_string(meta.pushedInstructionIndex + 1);
    }

    return item;
}

void finalizeTraceText(BackendGenerationResult& out, const std::vector<GeneratedInstruction>& generatedInstructions)
{
    for (auto& item : out.trace)
    {
        std::size_t sep = item.targetCode.find(':');
        if (sep == std::string::npos)
        {
            continue;
        }

        int begin = std::stoi(item.targetCode.substr(0, sep));
        int end = std::stoi(item.targetCode.substr(sep + 1));
        item.targetCode = instTextRange(generatedInstructions, begin, end);
    }
}

void finalizeOutputCodes(BackendGenerationResult& out, const std::vector<GeneratedInstruction>& generatedInstructions)
{
    out.codes.reserve(generatedInstructions.size() + 1);
    out.instructions.reserve(generatedInstructions.size() + 1);

    for (const auto& in : generatedInstructions)
    {
        std::string showB = in.b;
        if ((in.op == "FJ" || in.op == "TJ" || in.op == "JMP") && in.target >= 0)
        {
            showB = "L" + std::to_string(in.target);
        }

        out.codes.push_back(in.op + " " + in.a + ", " + showB + ", " + in.c);
        out.instructions.push_back({in.op, in.a, showB, in.c});
    }

    out.codes.push_back("HALT _, _, _");
    out.instructions.push_back({"HALT", "_", "_", "_"});
}

} // namespace

namespace backend
{

BackendGenerationResult generateTargetArtifacts(
    const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks,
    const std::vector<Symbol>& symbols)
{
    BackendGenerationResult out;
    (void)buildLiveness(quads, blocks, symbols);

    std::set<int> blockEnds;
    for (const auto& b : blocks)
    {
        blockEnds.insert(b.end);
    }

    std::map<int, int> quadToBlock = buildQuadToBlock(quads, blocks);
    std::map<int, int> ifToEndIf;
    std::map<int, int> ifToElse;
    buildIfMaps(quads, ifToEndIf, ifToElse);

    CodegenState state;

    for (int i = 0; i < static_cast<int>(quads.size()); ++i)
    {
        const Quadruple& q = quads[i];
        const std::string targetOp = opToTarget(q.op);
        QuadProcessMeta meta;
        meta.firstInstructionIndex = static_cast<int>(state.generatedInstructions.size());

        if (state.pendingWhileEntryInstructionIndex >= 0 && q.op != "wh" && meta.firstInstructionIndex >= state.pendingWhileEntryInstructionIndex)
        {
            state.controlFlowStack.push_back(meta.firstInstructionIndex);
            meta.pushedInstructionIndex = meta.firstInstructionIndex;
            state.pendingWhileEntryInstructionIndex = -1;
        }

        handleArithmeticOrAssign(q, targetOp, state);
        handleControlFlow(q, meta.firstInstructionIndex, state, meta);
        handleCallAndReturn(q, state);

        if (blockEnds.find(i) != blockEnds.end())
        {
            flushRegisterToVariable(state);
        }

        int instEnd = static_cast<int>(state.generatedInstructions.size()) - 1;
        out.trace.push_back(makeTraceItem(i, q, meta.firstInstructionIndex, instEnd, quadToBlock, state, meta));
    }

    finalizeTraceText(out, state.generatedInstructions);
    finalizeOutputCodes(out, state.generatedInstructions);
    return out;
}

} // namespace backend
