// 文件说明：实现基本块局部优化流程，包括常量折叠、传播、代数化简等。

#include "BackendOptimization.h"
#include <map>
#include <set>

using std::map;
using std::string;
using std::vector;

// 以基本块为单位执行局部优化，并返回新的四元式序列。
// 当前实现：复制传播、代数化简、公共子表达式复用。
vector<Quadruple> BackendOptimization::optimizeByBasicBlocks(
    const vector<Quadruple> &quads,
    const vector<BasicBlock> &blocks)
{
    vector<Quadruple> output;

    for (const auto &block : blocks)
    {
        // alias 表示“谁可以替换成谁”。
        map<string, string> alias;

        //// 解析复合语句块 begin ... end。
void Parser::parseCompoundStmt()
{
    consume(TokenType::Keyword, "begin", "复合语句缺少 begin");
    parseStmtList();
    consume(TokenType::Keyword, "end", "复合语句缺少 end");
}

// 解析语句序列（分号分隔）。
void Parser::parseStmtList()
{
    if (check(TokenType::Keyword, "end"))
    {
        return;
    }
    parseStmt();
    while (match(TokenType::Delimiter, ";"))
    {
        if (check(TokenType::Keyword, "end") || check(TokenType::Keyword, "else"))
        {
            break;
        }
        parseStmt();
    }
}

// 语句分发器：按首 Token 决定进入哪类语句解析。
void Parser::parseStmt()
{
    if (check(TokenType::Keyword, "begin"))
    {
        parseCompoundStmt();
        return;
    }
    if (check(TokenType::Keyword, "if"))
    {
        parseIfStmt();
        return;
    }
    if (check(TokenType::Keyword, "while"))
    {
        parseWhileStmt();
        return;
    }
    if (check(TokenType::Identifier) && tokens_[current_ + 1].lexeme == "(")
    {
        parseCallStmt();
        return;
    }
    parseAssignStmt();
} 记录已经算过的表达式。
        map<string, string> exprValue;

        for (int i = block.start; i <= block.end && i < static_cast<int>(quads.size()); ++i)
        {
            Quadruple q = quads[i];

            if (q.op == ":=")
            {
                if (alias.count(q.arg1))
                {
                    q.arg1 = alias[q.arg1];
                }
                alias[q.result] = q.arg1;
                output.push_back(q);
                continue;
            }

            if (q.op == "+" || q.op == "-" || q.op == "*" || q.op == "/")
            {
                if (alias.count(q.arg1))
                {
                    q.arg1 = alias[q.arg1];
                }
                if (alias.count(q.arg2))
                {
                    q.arg2 = alias[q.arg2];
                }

                // 代数化简：x + 0 = x，x * 1 = x。
                if ((q.op == "+" && q.arg2 == "0") || (q.op == "*" && q.arg2 == "1"))
                {
                    output.push_back({":=", q.arg1, "_", q.result});
                    continue;
                }

                // 公共子表达式：相同 op/arg1/arg2 只算一次。
                string key = q.op + "|" + q.arg1 + "|" + q.arg2;
                if (exprValue.count(key))
                {
                    output.push_back({":=", exprValue[key], "_", q.result});
                    continue;
                }

                exprValue[key] = q.result;
                output.push_back(q);
                continue;
            }

            output.push_back(q);
        }
    }

    return output;
}
