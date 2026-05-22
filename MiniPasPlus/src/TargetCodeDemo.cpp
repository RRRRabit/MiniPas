
#include "TargetCodeDemo.h"

using std::string;
using std::to_string;
using std::vector;

namespace
{

// 把四元式格式化成表格里可读的文本。
static string quadToText(const Quadruple &q)
{
    return "(" + q.op + ", " + q.arg1 + ", " + q.arg2 + ", " + q.result + ")";
}

static string opToTarget(const string &op)
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

static string pseudoTargetCode(const Quadruple &q)
{
    const string targetOp = opToTarget(q.op);
    if (!targetOp.empty())
    {
        return "LD R, " + q.arg1 + " ; " + targetOp + " R, " + q.arg2 + " ; ST " + q.result + ", R";
    }

    if (q.op == ":=")
    {
        return "LD R, " + q.arg1 + " ; ST " + q.result + ", R";
    }
    if (q.op == "if" || q.op == "do")
    {
        return "LD R, " + q.arg1 + " ; FJ R, Lfalse";
    }
    if (q.op == "el" || q.op == "we")
    {
        return "JMP Lnext";
    }
    if (q.op == "wh" || q.op == "ie" || q.op == "program" || q.op == "end")
    {
        return "MARK " + q.op;
    }
    if (q.op == "param")
    {
        return "ARG " + q.arg1;
    }
    if (q.op == "call")
    {
        return q.result == "_" ? "CALL " + q.arg1 + ", " + q.arg2
                               : "CALL " + q.arg1 + ", " + q.arg2 + " ; MOVRET R ; ST " + q.result + ", R";
    }
    if (q.op == "ret")
    {
        return "RET " + q.arg1;
    }
    return "MARK " + q.op;
}

static string rdlText(const Quadruple &q)
{
    if (!opToTarget(q.op).empty() || q.op == ":=")
    {
        return "R=" + q.result;
    }
    if (q.op == "if" || q.op == "do")
    {
        return "R=" + q.arg1;
    }
    return "0";
}


} // namespace

// 按基本块遍历优化后四元式，生成目标代码表数据。
vector<TargetCodeItem> TargetCodeDemo::generate(const vector<Quadruple> &quads,
                                                const vector<BasicBlock> &blocks)
{
    vector<TargetCodeItem> result;

    for (const auto &block : blocks)
    {
        for (int i = block.start; i <= block.end && i < static_cast<int>(quads.size()); ++i)
        {
            const Quadruple &q = quads[i];
            result.push_back({"B" + to_string(block.id),
                              quadToText(q),
                              pseudoTargetCode(q),
                              rdlText(q),
                              semText(q)});
        }
    }

    return result;
}
