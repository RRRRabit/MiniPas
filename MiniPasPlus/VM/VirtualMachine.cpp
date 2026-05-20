#include "VirtualMachine.h"

#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <unordered_map>

void VirtualMachine::execute(const std::vector<VMInstruction>& program)
{
    // 每次执行新程序前，都必须清空上一次运行留下来的状态。
    globals_.clear();
    frames_.clear();
    argBuffer_.clear();
    retVal_ = 0.0;

    // 主程序也放在一个 Frame 中，这样读写逻辑可以统一处理。
    frames_.push_back(Frame{});

    // programCounter 是“当前执行到第几条 VM 指令”。
    // 普通指令执行完会 ++programCounter；跳转指令会直接改成目标下标。
    int programCounter = 0;

    // 防死循环保护。
    // 如果用户写了 while true 这种无限循环，不能让软件卡死。
    int guard = 0;
    const int maxSteps = 2000000;

    while (programCounter >= 0 && programCounter < static_cast<int>(program.size()))
    {
        if (++guard > maxSteps)
        {
            throw std::runtime_error("VM 执行步数超限，可能存在死循环");
        }

        const VMInstruction& instruction = program[programCounter];
        // HALT 是程序结束指令。
        if (instruction.op == "HALT")
        {
            break;
        }

        // 分组尝试执行指令。
        // 例如 MOV 属于数据传送，ADD 属于二元运算，JMP 属于跳转。
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
    // 这里只返回 globals_，因为答辩/界面展示的是程序最终可见变量。
    // 函数内部局部变量在函数返回后已经没有意义。
    return globals_;
}

bool VirtualMachine::executeDataTransferInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "LD")
    {
        // LD a b：把 b 的值读出来，写到 a。
        writeValue(instruction.a, readValue(instruction.b));
        ++programCounter;
        return true;
    }

    if (instruction.op == "ST")
    {
        // ST a b：把 a 的值写到 b。
        // 当前项目里 MOV 更常用，ST 作为目标代码指令保留。
        writeValue(instruction.b, readValue(instruction.a));
        ++programCounter;
        return true;
    }

    if (instruction.op == "MOV")
    {
        // MOV a b：赋值指令，对应四元式里的 :=。
        writeValue(instruction.a, readValue(instruction.b));
        ++programCounter;
        return true;
    }

    if (instruction.op == "MOVRET")
    {
        // MOVRET a：函数调用返回后，把 retVal_ 写入 a。
        writeValue(instruction.a, retVal_);
        ++programCounter;
        return true;
    }

    return false;
}

bool VirtualMachine::executeBinaryInstruction(const VMInstruction& instruction, int& programCounter)
{
    // 二元指令都有三个核心操作数：
    // a = b op c
    // 例如 ADD t1 x y 表示 t1 = x + y。
    if (instruction.op != "ADD" && instruction.op != "SUB" && instruction.op != "MUL" && instruction.op != "DIV"
        && instruction.op != "LT" && instruction.op != "GT" && instruction.op != "EQ" && instruction.op != "LE"
        && instruction.op != "GE" && instruction.op != "NE" && instruction.op != "AND" && instruction.op != "OR")
    {
        return false;
    }

    double leftValue = readValue(instruction.b);
    double rightValue = readValue(instruction.c);
    double resultValue = 0.0;

    // 算术运算：结果仍然是数字。
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
    // 关系运算：true 用 1.0 表示，false 用 0.0 表示。
    // 这样后面的 FJ/TJ 跳转只需要判断是不是 0。
    else if (instruction.op == "LT") resultValue = (leftValue < rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "GT") resultValue = (leftValue > rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "EQ") resultValue = (leftValue == rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "LE") resultValue = (leftValue <= rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "GE") resultValue = (leftValue >= rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "NE") resultValue = (leftValue != rightValue) ? 1.0 : 0.0;
    else if (instruction.op == "AND") resultValue = (isTrue(leftValue) && isTrue(rightValue)) ? 1.0 : 0.0;
    else if (instruction.op == "OR") resultValue = (isTrue(leftValue) || isTrue(rightValue)) ? 1.0 : 0.0;

    // 把运算结果写回目标变量，然后顺序执行下一条。
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

    // NO 是 not 的意思。
    // 如果 b 为真，则 a = 0；如果 b 为假，则 a = 1。
    writeValue(instruction.a, isTrue(readValue(instruction.b)) ? 0.0 : 1.0);
    ++programCounter;
    return true;
}

bool VirtualMachine::executeJumpInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "FJ")
    {
        // FJ = False Jump。
        // 条件为假时跳转，常用于 if 条件不成立、while 条件不成立。
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
        // TJ = True Jump。
        // 当前项目主要使用 FJ 和 JMP，TJ 保留给后续扩展。
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
        // 无条件跳转。比如 if 的 true 分支执行完，要跳过 else 分支。
        programCounter = parseLabel(instruction.a);
        return true;
    }

    return false;
}

bool VirtualMachine::executeCallInstruction(const VMInstruction& instruction, int& programCounter)
{
    if (instruction.op == "ARG")
    {
        // ARG x：把实参 x 的值压入参数缓冲区。
        // 多个参数会连续执行多条 ARG。
        argBuffer_.push_back(readValue(instruction.a));
        ++programCounter;
        return true;
    }

    if (instruction.op == "CALL")
    {
        // CALL L10 2：调用 L10 位置的函数，参数数量是 2。
        int argc = static_cast<int>(readValue(instruction.b));
        if (argc < 0 || argc > static_cast<int>(argBuffer_.size()))
        {
            throw std::runtime_error("VM 运行时错误：CALL 参数数量非法");
        }

        Frame frame;
        // 记住返回地址：函数 RET 之后要回到 CALL 的下一条。
        frame.returnPc = programCounter + 1;

        // 把最近压入的 argc 个参数放入新 Frame 的 ARG0、ARG1...
        // 后面函数入口处会执行 MOV，把 ARG0 复制给真正的形参名。
        for (int i = 0; i < argc; ++i)
        {
            frame.locals["ARG" + std::to_string(i)] = argBuffer_[argBuffer_.size() - argc + i];
        }

        // 参数已经交给新 Frame，缓冲区里删掉这批参数。
        argBuffer_.resize(argBuffer_.size() - argc);
        frames_.push_back(std::move(frame));

        // 跳到函数入口。
        programCounter = parseLabel(instruction.a);
        return true;
    }

    if (instruction.op == "RET")
    {
        // RET x：函数返回，返回值是 x 的当前值。
        retVal_ = readValue(instruction.a);

        // 如果只剩主程序 Frame，说明主程序也结束了。
        if (frames_.size() <= 1)
        {
            programCounter = -1;
            return true;
        }

        // 弹出当前函数 Frame，回到调用点后面继续执行。
        int retPc = frames_.back().returnPc;
        frames_.pop_back();
        programCounter = retPc;
        return true;
    }

    return false;
}

double VirtualMachine::readValue(const std::string& operand) const
{
    // "_" 是四元式和 VM 指令里的占位符，表示这里没有实际操作数。
    if (operand.empty() || operand == "_")
    {
        return 0.0;
    }

    // L12 这种形式通常是跳转标签。
    // 某些地方会把标签当作数字读取，这里兼容地返回 12。
    if (operand.size() > 1 && operand[0] == 'L' && std::isdigit(static_cast<unsigned char>(operand[1])))
    {
        return std::strtod(operand.substr(1).c_str(), nullptr);
    }

    // 数字常量直接转换成 double。
    if (isNumber(operand))
    {
        return std::strtod(operand.c_str(), nullptr);
    }

    // 先查当前函数的局部变量。
    // 如果正在执行函数，形参和局部临时变量一般都在这里。
    if (!frames_.empty())
    {
        auto it = frames_.back().locals.find(operand);
        if (it != frames_.back().locals.end())
        {
            return it->second;
        }
    }

    // 再查全局变量。找不到就返回 0，等价于默认初值 0。
    auto g = globals_.find(operand);
    return g == globals_.end() ? 0.0 : g->second;
}

void VirtualMachine::writeValue(const std::string& operand, double value)
{
    // 空操作数或占位符不写。
    if (operand.empty() || operand == "_")
    {
        return;
    }

    // ARG0、ARG1... 是函数调用参数，只属于当前函数 Frame。
    if (!frames_.empty() && operand.rfind("ARG", 0) == 0)
    {
        frames_.back().locals[operand] = value;
        return;
    }

    // R 开头的名字通常是函数返回相关的临时变量，放在当前 Frame 更合理。
    if (!frames_.empty() && !operand.empty() && operand[0] == 'R')
    {
        frames_.back().locals[operand] = value;
        return;
    }

    // 如果当前在函数内部，并且变量名不是数组元素/记录字段这种复合名字，
    // 就优先认为它是局部变量或临时变量。
    if (frames_.size() > 1 && operand.find('.') == std::string::npos && operand.find('[') == std::string::npos)
    {
        frames_.back().locals[operand] = value;
        return;
    }

    // 其他情况写入全局变量。
    // 数组元素会被展开成 a[0]、a[1]；记录字段会被展开成 r.x、r.y。
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
    // MiniPasPlus 里没有单独的 bool 存储类型，运行时用数字表示真假：
    // 0 是假，非 0 是真。
    return v != 0.0;
}

int VirtualMachine::parseLabel(const std::string& text) const
{
    // 标签格式固定为 L + 数字，例如 L0、L15。
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
    // 四元式里的运算符是 "+", "<=" 这种源语言风格；
    // VM 指令使用 ADD、LE 这种更像汇编的名字。
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
    const std::vector<FunctionInfo>& functions,
    const std::vector<ParameterInfo>& parameters)
{
    struct PendingInstruction
    {
        // instruction：已经生成出来但标签可能还没填好的 VM 指令。
        VMInstruction instruction;

        // jumpTargetQuadIndex：这条跳转指令最终要跳到哪条四元式。
        // 第二遍才能把“四元式下标”换成“VM 指令标签”。
        int jumpTargetQuadIndex = -1;

        // callTargetFunctionName：CALL 指令先保存函数名，最后统一换成函数入口标签。
        std::string callTargetFunctionName;
    };

    // 下面这些表用来配对结构化控制流。
    // 四元式没有直接写 L1/L2，而是用 if/el/ie、wh/do/we 标记结构。
    // 生成 VM 时必须知道：if 条件失败跳哪里，while 条件失败跳哪里，循环结束跳回哪里。
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
        // 第一遍扫描：配对 if/el/ie 和 wh/do/we。
        // 用栈是为了支持嵌套控制流。
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

    // quadToInstructionIndex[i] 表示第 i 条四元式翻译后，对应 VM 指令的起始下标。
    // 这个映射是后面回填跳转标签的关键。
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
            // 普通运算四元式：(+ a b t1) -> ADD t1 a b。
            appendPendingInstruction({vmOp, q.result, q.arg1, q.arg2});
        }
        else if (q.op == ":=")
        {
            // 赋值四元式：(:= x _ y) -> MOV y x。
            appendPendingInstruction({"MOV", q.result, q.arg1, "_"});
        }
        else if (q.op == "if")
        {
            // if 条件为假时，跳到 else 分支或 if 结束位置。
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
            // 执行完 true 分支后，需要无条件跳过 else 分支。
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
            // while 条件为假时，跳到循环结束之后。
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
            // 循环体执行完后，跳回 while 条件位置重新判断。
            auto whileQuadIterator = whileEndToWhileQuad.find(i);
            int whileConditionQuad = (whileQuadIterator == whileEndToWhileQuad.end()) ? -1 : (whileQuadIterator->second + 1);
            appendPendingInstruction({"JMP", "_", "_", "_"}, whileConditionQuad);
        }
        else if (q.op == "param")
        {
            // param 四元式翻译成 ARG，把实参值暂存。
            appendPendingInstruction({"ARG", q.arg1, "_", "_"});
        }
        else if (q.op == "call")
        {
            // call 四元式先生成 CALL。函数入口标签现在还不知道，最后统一回填。
            appendPendingInstruction({"CALL", "_", q.arg2, "_"}, -1, q.arg1);
            if (q.result != "_" && !q.result.empty())
            {
                // 如果函数有返回值，把 retVal_ 移到接收变量。
                appendPendingInstruction({"MOVRET", q.result, "_", "_"});
            }
        }
        else if (q.op == "ret")
        {
            // 函数返回。
            appendPendingInstruction({"RET", q.arg1, "_", "_"});
        }
    }

    // 处理函数入口和形参绑定。
    // Parser 只知道“函数从哪条四元式开始”和“参数有哪些”；
    // VM 需要在函数入口插入 MOV，把 ARG0/ARG1 复制到真正的形参名。
    std::unordered_map<std::string, int> functionEntryInstruction;
    std::unordered_map<std::string, std::vector<ParameterInfo>> functionParams;
    for (const auto& p : parameters)
    {
        functionParams[p.functionName].push_back(p);
    }
    for (auto& kv : functionParams)
    {
        std::sort(kv.second.begin(), kv.second.end(), [](const ParameterInfo& a, const ParameterInfo& b)
        {
            return a.offset < b.offset;
        });
    }

    for (const auto& functionInfo : functions)
    {
        if (functionInfo.entryQuad >= 0 && functionInfo.entryQuad < static_cast<int>(quadToInstructionIndex.size()))
        {
            int entryInstructionIndex = quadToInstructionIndex[functionInfo.entryQuad];
            if (entryInstructionIndex >= 0)
            {
                functionEntryInstruction[functionInfo.name] = entryInstructionIndex;
                auto it = functionParams.find(functionInfo.name);
                if (it != functionParams.end() && !it->second.empty())
                {
                    // 在函数入口前插入：
                    // MOV 参数名 ARG0
                    // MOV 参数名 ARG1
                    // 这样函数体里直接读参数名即可。
                    for (std::size_t i = 0; i < it->second.size(); ++i)
                    {
                        pendingInstructions.insert(
                            pendingInstructions.begin() + entryInstructionIndex + static_cast<int>(i),
                            PendingInstruction{{"MOV", it->second[i].name, "ARG" + std::to_string(i), "_"}, -1, ""});
                    }
                    // 插入了新指令后，后面所有四元式对应的 VM 下标都要整体后移。
                    for (std::size_t q = 0; q < quadToInstructionIndex.size(); ++q)
                    {
                        if (quadToInstructionIndex[q] >= entryInstructionIndex)
                        {
                            quadToInstructionIndex[q] += static_cast<int>(it->second.size());
                        }
                    }
                }
            }
        }
    }

    // 函数定义通常位于主程序之前。
    // 如果 VM 从第 0 条顺序执行，会误入函数体；所以最终 VM 程序开头会插入一条 JMP，
    // 直接跳到主程序入口。
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

    // +1 是因为 vmProgram 前面多插入了这条 JMP，后面所有真实指令标签都整体后移一位。
    vmProgram.push_back({"JMP", "L" + std::to_string(mainEntryInstructionIndex + 1), "_", "_"});

    for (const auto& pendingInstruction : pendingInstructions)
    {
        VMInstruction instruction = pendingInstruction.instruction;

        // 回填跳转标签。
        // 之前保存的是“四元式下标”，现在转换成“VM 指令标签 Lx”。
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
                    // JMP 的目标标签放在 a 字段。
                    instruction.a = "L" + std::to_string(targetLabel);
                }
                else
                {
                    // FJ/TJ 的目标标签放在 b 字段，a 字段放条件。
                    instruction.b = "L" + std::to_string(targetLabel);
                }
            }
        }

        if (instruction.op == "CALL")
        {
            // CALL 的目标函数名在这里换成真正的入口标签。
            auto functionEntryIterator = functionEntryInstruction.find(pendingInstruction.callTargetFunctionName);
            instruction.a = (functionEntryIterator != functionEntryInstruction.end()) ? ("L" + std::to_string(functionEntryIterator->second + 1)) : "L0";
        }

        vmProgram.push_back(instruction);
    }

    // 程序正常结束。
    vmProgram.push_back({"HALT", "_", "_", "_"});
    return vmProgram;
}

} // namespace vm
