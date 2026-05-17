#include "VirtualMachine.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <unordered_map>

void VirtualMachine::execute(const std::vector<VMInstruction>& program)
{
    globals_.clear();
    frames_.clear();
    argBuffer_.clear();
    retVal_ = 0.0;

    frames_.push_back(Frame{});
    int programCounter = 0;
    int guard = 0;
    const int maxSteps = 2000000;

    while (programCounter >= 0 && programCounter < static_cast<int>(program.size()))
    {
        if (++guard > maxSteps)
        {
            throw std::runtime_error("VM 执行步数超限，可能存在死循环");
        }

        const VMInstruction& instruction = program[programCounter];
        if (instruction.op == "HALT")
        {
            break;
        }

        if (executeDataTransferInstruction(instruction, programCounter))
        {
            continue;
        }
        if (executeBinaryInstruction(instruction, programCounter))
        {
            continue;
        }
        if (executeUnaryInstruction(instruction, programCounter))
        {
            continue;
        }
        if (executeJumpInstruction(instruction, programCounter))
        {
            continue;
        }
        if (executeCallInstruction(instruction, programCounter))
        {
            continue;
        }

        throw std::runtime_error("VM 运行时错误：未知指令 " + instruction.op);
    }
}

const std::map<std::string, double>& VirtualMachine::values() const
{
    return globals_;
}

bool VirtualMachine::executeDataTransferInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "LD")
    {
        writeValue(instruction.a, readValue(instruction.b));
        ++programCounter;
        return true;
    }

    if (instruction.op == "ST")
    {
        writeValue(instruction.b, readValue(instruction.a));
        ++programCounter;
        return true;
    }

    if (instruction.op == "MOV")
    {
        writeValue(instruction.a, readValue(instruction.b));
        ++programCounter;
        return true;
    }

    if (instruction.op == "MOVRET")
    {
        writeValue(instruction.a, retVal_);
        ++programCounter;
        return true;
    }

    return false;
}

bool VirtualMachine::executeBinaryInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op != "ADD" && instruction.op != "SUB" && instruction.op != "MUL" && instruction.op != "DIV"
        && instruction.op != "LT" && instruction.op != "GT" && instruction.op != "EQ" && instruction.op != "LE"
        && instruction.op != "GE" && instruction.op != "NE" && instruction.op != "AND" && instruction.op != "OR")
    {
        return false;
    }

    double leftValue = readValue(instruction.b);
    double rightValue = readValue(instruction.c);
    double resultValue = 0.0;

    if (instruction.op == "ADD") resultValue = leftValue + rightValue;
    else if (instruction.op == "SUB") resultValue = leftValue - rightValue;
    else if (instruction.op == "MUL") resultValue = leftValue * rightValue;
    else if (instruction.op == "DIV")
    {
        if (rightValue == 0.0)
        {
            throw std::runtime_error("VM 运行时错误：除数为 0");
        }
        resultValue = leftValue / rightValue;
    }
    else if (instruction.op == "LT") resultValue = (leftValue < rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "GT") resultValue = (leftValue > rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "EQ") resultValue = (leftValue == rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "LE") resultValue = (leftValue <= rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "GE") resultValue = (leftValue >= rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "NE") resultValue = (leftValue != rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "AND") resultValue = (isTrue(leftValue) && isTrue(rightValue)) ? 1.0 : 0.0;
    else if (instruction.op == "OR") resultValue = (isTrue(leftValue) || isTrue(rightValue)) ? 1.0 : 0.0;

    writeValue(instruction.a, resultValue);
    ++programCounter;
    return true;
}

bool VirtualMachine::executeUnaryInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op != "NO")
    {
        return false;
    }

    writeValue(instruction.a, isTrue(readValue(instruction.b)) ? 0.0 : 1.0);
    ++programCounter;
    return true;
}

bool VirtualMachine::executeJumpInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "FJ")
    {
        if (!isTrue(readValue(instruction.a)))
        {
            programCounter = parseLabel(instruction.b);
        }
        else
        {
            ++programCounter;
        }
        return true;
    }

    if (instruction.op == "TJ")
    {
        if (isTrue(readValue(instruction.a)))
        {
            programCounter = parseLabel(instruction.b);
        }
        else
        {
            ++programCounter;
        }
        return true;
    }

    if (instruction.op == "JMP")
    {
        programCounter = parseLabel(instruction.a);
        return true;
    }

    return false;
}

bool VirtualMachine::executeCallInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "ARG")
    {
        argBuffer_.push_back(readValue(instruction.a));
        ++programCounter;
        return true;
    }

    if (instruction.op == "CALL")
    {
        int argc = static_cast<int>(readValue(instruction.b));
        if (argc < 0 || argc > static_cast<int>(argBuffer_.size()))
        {
            throw std::runtime_error("VM 运行时错误：CALL 参数数量非法");
        }

        Frame frame;
        frame.returnPc = programCounter + 1;
        for (int i = 0; i < argc; ++i)
        {
            frame.locals["ARG" + std::to_string(i)] = argBuffer_[argBuffer_.size() - argc + i];
        }

        argBuffer_.resize(argBuffer_.size() - argc);
        frames_.push_back(std::move(frame));
        programCounter = parseLabel(instruction.a);
        return true;
    }

    if (instruction.op == "RET")
    {
        retVal_ = readValue(instruction.a);
        if (frames_.size() <= 1)
        {
            programCounter = -1;
            return true;
        }

        int retPc = frames_.back().returnPc;
        frames_.pop_back();
        programCounter = retPc;
        return true;
    }

    return false;
}

double VirtualMachine::readValue(const std::string& operand) const
{
    if (operand.empty() || operand == "_")
    {
        return 0.0;
    }

    if (operand.size() > 1 && operand[0] == 'L' && std::isdigit(static_cast<unsigned char>(operand[1])))
    {
        return std::strtod(operand.substr(1).c_str(), nullptr);
    }

    if (isNumber(operand))
    {
        return std::strtod(operand.c_str(), nullptr);
    }

    if (!frames_.empty())
    {
        auto it = frames_.back().locals.find(operand);
        if (it != frames_.back().locals.end())
        {
            return it->second;
        }
    }

    auto g = globals_.find(operand);
    return g == globals_.end() ? 0.0 : g->second;
}

void VirtualMachine::writeValue(const std::string& operand, double value)
{
    if (operand.empty() || operand == "_")
    {
        return;
    }

    if (!frames_.empty() && operand.rfind("ARG", 0) == 0)
    {
        frames_.back().locals[operand] = value;
        return;
    }

    if (!frames_.empty() && !operand.empty() && operand[0] == 'R')
    {
        frames_.back().locals[operand] = value;
        return;
    }

    globals_[operand] = value;
}

bool VirtualMachine::isNumber(const std::string& text) const
{
    if (text.empty())
    {
        return false;
    }

    char* end = nullptr;
    std::strtod(text.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool VirtualMachine::isTrue(double v) const
{
    return v != 0.0;
}

int VirtualMachine::parseLabel(const std::string& text) const
{
    if (text.size() > 1 && text[0] == 'L')
    {
        return std::atoi(text.substr(1).c_str());
    }

    throw std::runtime_error("VM 运行时错误：非法跳转标签 " + text);
}

namespace
{

std::string opToVm(const std::string& op)
{
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

} // namespace

namespace vm
{

std::vector<VMInstruction> generateVmInstructions(
    const std::vector<Quadruple>& quads,
    const std::vector<FunctionInfo>& functions)
{
    struct PendingInstruction
    {
        VMInstruction instruction;
        int jumpTargetQuadIndex = -1;
        std::string callTargetFunctionName;
    };

    std::map<int, int> ifToEndIfQuad;
    std::map<int, int> ifToElseQuad;
    std::vector<std::pair<int, int>> ifContextStack;
    std::vector<int> whileQuadStack;
    std::vector<std::pair<int, int>> doContextStack;
    std::map<int, int> doToWhileEndQuad;
    std::map<int, int> whileEndToWhileQuad;

    for (int i = 0; i < static_cast<int>(quads.size()); ++i)
    {
        const Quadruple& q = quads[i];
        if (q.op == "if")
        {
            ifContextStack.push_back({i, -1});
        }
        else if (q.op == "el")
        {
            if (!ifContextStack.empty())
            {
                ifContextStack.back().second = i;
            }
        }
        else if (q.op == "ie")
        {
            if (!ifContextStack.empty())
            {
                ifToEndIfQuad[ifContextStack.back().first] = i;
                ifToElseQuad[ifContextStack.back().first] = ifContextStack.back().second;
                ifContextStack.pop_back();
            }
        }
        else if (q.op == "wh")
        {
            whileQuadStack.push_back(i);
        }
        else if (q.op == "do")
        {
            int whileQuadIndex = whileQuadStack.empty() ? -1 : whileQuadStack.back();
            doContextStack.push_back({i, whileQuadIndex});
        }
        else if (q.op == "we")
        {
            if (!doContextStack.empty())
            {
                auto [doQuadIndex, whileQuadIndex] = doContextStack.back();
                doContextStack.pop_back();
                doToWhileEndQuad[doQuadIndex] = i;
                if (whileQuadIndex >= 0)
                {
                    whileEndToWhileQuad[i] = whileQuadIndex;
                }
            }
            if (!whileQuadStack.empty())
            {
                whileQuadStack.pop_back();
            }
        }
    }

    std::vector<PendingInstruction> pendingInstructions;
    std::vector<int> quadToInstructionIndex(quads.size(), -1);

    auto appendPendingInstruction = [&](const VMInstruction& instruction, int jumpTargetQuadIndex = -1, const std::string& callTargetFunctionName = "")
    {
        pendingInstructions.push_back({instruction, jumpTargetQuadIndex, callTargetFunctionName});
    };

    for (int i = 0; i < static_cast<int>(quads.size()); ++i)
    {
        quadToInstructionIndex[i] = static_cast<int>(pendingInstructions.size());
        const Quadruple& q = quads[i];
        std::string vmOp = opToVm(q.op);

        if (!vmOp.empty())
        {
            appendPendingInstruction({vmOp, q.result, q.arg1, q.arg2});
        }
        else if (q.op == ":=")
        {
            appendPendingInstruction({"MOV", q.result, q.arg1, "_"});
        }
        else if (q.op == "if")
        {
            int falseTarget = -1;
            auto elseQuadIterator = ifToElseQuad.find(i);
            if (elseQuadIterator != ifToElseQuad.end() && elseQuadIterator->second >= 0)
            {
                falseTarget = elseQuadIterator->second + 1;
            }
            else
            {
                auto endIfQuadIterator = ifToEndIfQuad.find(i);
                if (endIfQuadIterator != ifToEndIfQuad.end())
                {
                    falseTarget = endIfQuadIterator->second;
                }
            }
            appendPendingInstruction({"FJ", q.arg1, "_", "_"}, falseTarget);
        }
        else if (q.op == "el")
        {
            int ieTarget = -1;
            for (const auto& [ifQuadIndex, elseQuadIndex] : ifToElseQuad)
            {
                if (elseQuadIndex == i)
                {
                    auto endIfQuadIterator = ifToEndIfQuad.find(ifQuadIndex);
                    if (endIfQuadIterator != ifToEndIfQuad.end())
                    {
                        ieTarget = endIfQuadIterator->second;
                    }
                    break;
                }
            }
            appendPendingInstruction({"JMP", "_", "_", "_"}, ieTarget);
        }
        else if (q.op == "do")
        {
            int falseTarget = -1;
            auto whileEndQuadIterator = doToWhileEndQuad.find(i);
            if (whileEndQuadIterator != doToWhileEndQuad.end())
            {
                falseTarget = whileEndQuadIterator->second + 1;
            }
            appendPendingInstruction({"FJ", q.arg1, "_", "_"}, falseTarget);
        }
        else if (q.op == "we")
        {
            auto whileQuadIterator = whileEndToWhileQuad.find(i);
            int whileConditionQuad = (whileQuadIterator == whileEndToWhileQuad.end()) ? -1 : (whileQuadIterator->second + 1);
            appendPendingInstruction({"JMP", "_", "_", "_"}, whileConditionQuad);
        }
        else if (q.op == "param")
        {
            appendPendingInstruction({"ARG", q.arg1, "_", "_"});
        }
        else if (q.op == "call")
        {
            appendPendingInstruction({"CALL", "_", q.arg2, "_"}, -1, q.arg1);
            if (q.result != "_" && !q.result.empty())
            {
                appendPendingInstruction({"MOVRET", q.result, "_", "_"});
            }
        }
        else if (q.op == "ret")
        {
            appendPendingInstruction({"RET", q.arg1, "_", "_"});
        }
    }

    std::unordered_map<std::string, int> functionEntryInstruction;
    for (const auto& functionInfo : functions)
    {
        if (functionInfo.entryQuad >= 0 && functionInfo.entryQuad < static_cast<int>(quadToInstructionIndex.size()))
        {
            int entryInstructionIndex = quadToInstructionIndex[functionInfo.entryQuad];
            if (entryInstructionIndex >= 0)
            {
                functionEntryInstruction[functionInfo.name] = entryInstructionIndex;
            }
        }
    }

    int funcCount = static_cast<int>(functions.size());
    int seenRet = 0;
    int mainEntryQuad = 0;
    for (int i = 0; i < static_cast<int>(quads.size()); ++i)
    {
        if (quads[i].op == "ret")
        {
            ++seenRet;
            if (seenRet == funcCount)
            {
                mainEntryQuad = i + 1;
                break;
            }
        }
    }

    int mainEntryInstructionIndex = (mainEntryQuad >= 0 && mainEntryQuad < static_cast<int>(quadToInstructionIndex.size()))
        ? quadToInstructionIndex[mainEntryQuad]
        : 0;

    std::vector<VMInstruction> vmProgram;
    vmProgram.reserve(pendingInstructions.size() + 2);
    vmProgram.push_back({"JMP", "L" + std::to_string(mainEntryInstructionIndex + 1), "_", "_"});

    for (const auto& pendingInstruction : pendingInstructions)
    {
        VMInstruction instruction = pendingInstruction.instruction;

        if (pendingInstruction.jumpTargetQuadIndex >= 0)
        {
            int targetLabel = -1;
            if (pendingInstruction.jumpTargetQuadIndex < static_cast<int>(quadToInstructionIndex.size()))
            {
                int targetInstructionIndex = quadToInstructionIndex[pendingInstruction.jumpTargetQuadIndex];
                if (targetInstructionIndex >= 0)
                {
                    targetLabel = targetInstructionIndex + 1;
                }
            }
            else if (pendingInstruction.jumpTargetQuadIndex == static_cast<int>(quadToInstructionIndex.size()))
            {
                targetLabel = static_cast<int>(pendingInstructions.size()) + 1;
            }

            if (targetLabel >= 0)
            {
                if (instruction.op == "JMP")
                {
                    instruction.a = "L" + std::to_string(targetLabel);
                }
                else
                {
                    instruction.b = "L" + std::to_string(targetLabel);
                }
            }
        }

        if (instruction.op == "CALL")
        {
            auto functionEntryIterator = functionEntryInstruction.find(pendingInstruction.callTargetFunctionName);
            instruction.a = (functionEntryIterator != functionEntryInstruction.end()) ? ("L" + std::to_string(functionEntryIterator->second + 1)) : "L0";
        }

        vmProgram.push_back(instruction);
    }

    vmProgram.push_back({"HALT", "_", "_", "_"});
    return vmProgram;
}

} // namespace vm
