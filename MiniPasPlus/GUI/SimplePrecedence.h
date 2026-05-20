#ifndef MINIPASPLUS_SIMPLEPRECEDENCE_H
#define MINIPASPLUS_SIMPLEPRECEDENCE_H

#include "Token.h"
#include <map>
#include <string>
#include <vector>

struct SimplePrecedenceStep
{
    // 分析栈当前内容。
    std::string stackText;

    // 剩余输入串。
    std::string inputText;

    // 栈顶终结符和当前输入符号之间的优先关系：<、=、>。
    std::string relationText;

    // 本步动作：移进、规约、接受、报错。
    std::string actionText;
};

struct SimplePrecedenceResult
{
    // enabled=false 表示当前源程序里没有可展示的表达式。
    bool enabled = false;
    bool success = false;

    // 被分析的表达式文本。
    std::string expressionText;
    std::string errorMessage;

    // 优先关系表的表头符号和表格内容。
    std::vector<std::string> symbols;
    std::vector<std::vector<std::string>> table;

    // 分析过程每一步，用于 GUI 表格展示。
    std::vector<SimplePrecedenceStep> steps;
};

namespace simple_precedence
{

// 从完整 Token 序列里找一个表达式，并做简单优先分析。
SimplePrecedenceResult analyzeFromTokens(const std::vector<Token>& tokens);

// 对已经截取好的表达式 Token 做简单优先分析。
SimplePrecedenceResult analyzeExpressionTokens(const std::vector<Token>& expressionTokens);

// 从完整 Token 序列中提取所有赋值/条件表达式并分析。
std::vector<SimplePrecedenceResult> analyzeAllExpressionsFromTokens(const std::vector<Token>& tokens);

// 对关系条件中的左右表达式做简单优先分析。
SimplePrecedenceResult analyzeRelationConditionTokens(const std::vector<Token>& conditionTokens);

} // namespace simple_precedence

#endif
