
#include "TargetCodeDemo.h"

// 把四元式格式化成表格里可读的文本。
static std::string quadToText(const Quadruple& q)
{
    return "(" + q.op + ", " + q.arg1 + ", " + q.arg2 + ", " + q.result + ")";
}

// 把单条四元式转换成“目标代码风格”字符串。
static std::string toTargetCode(const Quadruple& q)
{
    if (q.op == ":=")
    {
        return "MOV " + q.result + ", " + q.arg1;
    }
    if (q.op == "+")
    {
        return "ADD " + q.result + ", " + q.arg1 + ", " + q.arg2;
    }
    if (q.op == "-")
    {
        return "SUB " + q.result + ", " + q.arg1 + ", " + q.arg2;
    }
    if (q.op == "*")
    {
        return "MUL " + q.result + ", " + q.arg1 + ", " + q.arg2;
    }
    if (q.op == "/")
    {
        return "DIV " + q.result + ", " + q.arg1 + ", " + q.arg2;
    }
    if (q.op == "if" || q.op == "do")
    {
        return "FJ " + q.arg1 + ", Lfalse";
    }
    if (q.op == "el" || q.op == "we")
    {
        return "JMP Lnext";
    }
    if (q.op == "param")
    {
        return "ARG " + q.arg1;
    }
    if (q.op == "call")
    {
        return "CALL " + q.arg1 + ", " + q.arg2;
    }
    if (q.op == "ret")
    {
        return "RET " + q.arg1;
    }
    return "MARK " + q.op;
}

// 按基本块遍历优化后四元式，生成目标代码表数据。
std::vector<TargetCodeItem> TargetCodeDemo::generate(const std::vector<Quadruple>& quads,
                                                     const std::vector<BasicBlock>& blocks)
{
    std::vector<TargetCodeItem> result;

    for (const auto& block : blocks)
    {
        // range-for：逐个读取 basic block，更贴近“按块遍历”的语义。
        for (int i = block.start; i <= block.end && i < static_cast<int>(quads.size()); ++i)
        {
            const Quadruple& q = quads[i];
            result.push_back({
                "B" + std::to_string(block.id),
                quadToText(q),
                toTargetCode(q),
                "RDL: 记录当前四元式需要的变量",
                "SEM: 根据 op 选择 MOV/ADD/JMP/CALL 等目标指令"
            });
        }
    }

    return result;
}



