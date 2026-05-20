#include "SimplePrecedence.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

// 简单优先分析是 GUI 教学展示模块。
// 它只分析表达式的移进/规约过程，不参与真正的 Parser 编译流程。
// 真正生成四元式的是 Parser.cpp；这里生成的是给表格看的步骤。

namespace
{

struct ParserCore
{
    // 表达式文法使用的符号：
    // E 表达式，T 项，F 因子，i 代表标识符/常量，# 是输入结束符。
    std::vector<std::string> symbols = {"E", "T", "F", "i", "+", "*", "(", ")", "#"};
    std::unordered_map<std::string, int> index;
    std::vector<std::vector<std::string>> table;
};

ParserCore buildCore()
{
    // 构造简单优先关系表。
    // 表格单元格含义：
    // "<" 表示移进，">" 表示规约，"=" 表示同一产生式内部的相邻符号。
    ParserCore core;
    for (int i = 0; i < static_cast<int>(core.symbols.size()); ++i)
    {
        core.index[core.symbols[i]] = i;
    }
    core.table.assign(core.symbols.size(), std::vector<std::string>(core.symbols.size(), ""));

    auto setRel = [&](const std::string& left, const std::string& right, const std::string& rel)
    {
        core.table[core.index[left]][core.index[right]] = rel;
    };

    // 依据简单优先关系（Wirth-Weber）给表达式文法填关系：
    // E -> E+T | T, T -> T*F | F, F -> (E) | i
    setRel("E", "+", "=");
    setRel("+", "T", "=");
    setRel("T", "*", "=");
    setRel("*", "F", "=");
    setRel("(", "E", "=");
    setRel("E", ")", "=");

    setRel("+", "i", "<");
    setRel("+", "(", "<");
    setRel("*", "i", "<");
    setRel("*", "(", "<");
    setRel("(", "i", "<");
    setRel("(", "(", "<");
    setRel("#", "i", "<");
    setRel("#", "(", "<");
    setRel("#", "+", "<");
    setRel("#", "*", "<");

    setRel("i", "+", ">");
    setRel("i", "*", ">");
    setRel("i", ")", ">");
    setRel("i", "#", ">");
    setRel(")", "+", ">");
    setRel(")", "*", ">");
    setRel(")", ")", ">");
    setRel(")", "#", ">");

    // 终结符之间的常见关系，便于步骤分析稳定执行
    setRel("+", "+", ">");
    setRel("+", "*", "<");
    setRel("+", ")", ">");
    setRel("+", "#", ">");
    setRel("*", "+", ">");
    setRel("*", "*", ">");
    setRel("*", ")", ">");
    setRel("*", "#", ">");
    setRel("(", ")", "=");
    setRel("#", "#", "=");

    return core;
}

bool isExprToken(const Token& token)
{
    // 判断一个 Token 是否可能属于表达式。
    // 为了展示简单，标识符和常量统一看成 i。
    if (token.type == TokenType::IDENTIFIER || token.type == TokenType::CONSTANT)
    {
        return true;
    }
    if (token.type == TokenType::OPERATOR && (token.lexeme == "+" || token.lexeme == "*" || token.lexeme == "-" || token.lexeme == "/"))
    {
        return true;
    }
    return token.type == TokenType::DELIMITER &&
           (token.lexeme == "(" || token.lexeme == ")" || token.lexeme == "[" || token.lexeme == "]" || token.lexeme == ".");
}

bool isRelOp(const Token& token);

std::vector<Token> extractFirstExpression(const std::vector<Token>& tokens)
{
    // 从完整 Token 序列里提取第一个赋值语句右侧表达式。
    std::vector<Token> expression;
    bool afterAssign = false;
    int parenthesisDepth = 0;
    for (const auto& token : tokens)
    {
        if (!afterAssign)
        {
            if (token.type == TokenType::OPERATOR && token.lexeme == ":=")
            {
                afterAssign = true;
            }
            continue;
        }

        if (token.type == TokenType::DELIMITER && token.lexeme == "(") ++parenthesisDepth;
        if (token.type == TokenType::DELIMITER && token.lexeme == ")") --parenthesisDepth;

        if (parenthesisDepth <= 0 && token.type == TokenType::DELIMITER &&
            (token.lexeme == ";" || token.lexeme == "end" || token.lexeme == "]"))
        {
            break;
        }

        if (isExprToken(token))
        {
            expression.push_back(token);
        }
    }
    return expression;
}

std::vector<std::vector<Token>> extractAllExpressions(const std::vector<Token>& tokens)
{
    // 从所有赋值语句中提取右侧表达式。
    std::vector<std::vector<Token>> expressions;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type == TokenType::OPERATOR && tokens[i].lexeme == ":=")
        {
            std::vector<Token> expression;
            int parenthesisDepth = 0;
            for (std::size_t j = i + 1; j < tokens.size(); ++j)
            {
                const Token& token = tokens[j];
                if (token.type == TokenType::END_OF_FILE)
                {
                    break;
                }
                if (token.type == TokenType::DELIMITER && token.lexeme == "(") ++parenthesisDepth;
                if (token.type == TokenType::DELIMITER && token.lexeme == ")") --parenthesisDepth;
                if (parenthesisDepth <= 0 && token.type == TokenType::DELIMITER && token.lexeme == ";")
                {
                    break;
                }
                if (parenthesisDepth <= 0
                    && token.type == TokenType::KEYWORD
                    && (token.lexeme == "else" || token.lexeme == "end" || token.lexeme == "then" || token.lexeme == "do"))
                {
                    break;
                }
                if (isExprToken(token))
                {
                    expression.push_back(token);
                }
            }
            if (!expression.empty())
            {
                expressions.push_back(expression);
            }
        }
    }
    return expressions;
}

struct CollectedSpan
{
    // 用于记录从源程序中截取出来的一段表达式或条件。
    std::size_t startIndex = 0;
    std::string kind; // assign / condition
    std::vector<Token> tokens;
};

std::vector<CollectedSpan> extractAllSpans(const std::vector<Token>& tokens)
{
    std::vector<CollectedSpan> spans;

    auto collectAssignRight = [&](std::size_t start)
    {
        std::vector<Token> expression;
        int parenthesisDepth = 0;
        for (std::size_t j = start; j < tokens.size(); ++j)
        {
            const Token& token = tokens[j];
            if (token.type == TokenType::END_OF_FILE)
            {
                break;
            }
            if (token.type == TokenType::DELIMITER && token.lexeme == "(") ++parenthesisDepth;
            if (token.type == TokenType::DELIMITER && token.lexeme == ")") --parenthesisDepth;
            if (parenthesisDepth <= 0 && token.type == TokenType::DELIMITER && token.lexeme == ";")
            {
                break;
            }
            if (parenthesisDepth <= 0
                && token.type == TokenType::KEYWORD
                && (token.lexeme == "else" || token.lexeme == "end" || token.lexeme == "then" || token.lexeme == "do"))
            {
                break;
            }
            if (isExprToken(token))
            {
                expression.push_back(token);
            }
        }
        return expression;
    };

    auto collectCondition = [&](std::size_t start)
    {
        std::vector<Token> condition;
        int parenthesisDepth = 0;
        int bracketDepth = 0;
        for (std::size_t j = start; j < tokens.size(); ++j)
        {
            const Token& token = tokens[j];
            if (token.type == TokenType::END_OF_FILE)
            {
                break;
            }
            if (token.type == TokenType::DELIMITER && token.lexeme == "(") ++parenthesisDepth;
            if (token.type == TokenType::DELIMITER && token.lexeme == ")")
            {
                if (parenthesisDepth == 0 && bracketDepth == 0) break;
                --parenthesisDepth;
            }
            if (token.type == TokenType::DELIMITER && token.lexeme == "[") ++bracketDepth;
            if (token.type == TokenType::DELIMITER && token.lexeme == "]") --bracketDepth;

            if (parenthesisDepth == 0 && bracketDepth == 0
                && token.type == TokenType::KEYWORD
                && (token.lexeme == "then" || token.lexeme == "do"))
            {
                break;
            }
            if (isExprToken(token) || isRelOp(token))
            {
                condition.push_back(token);
            }
        }
        return condition;
    };

    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type == TokenType::OPERATOR && tokens[i].lexeme == ":=")
        {
            std::vector<Token> expr = collectAssignRight(i + 1);
            if (!expr.empty())
            {
                spans.push_back({i, "assign", expr});
            }
        }
        else if (tokens[i].type == TokenType::KEYWORD && (tokens[i].lexeme == "if" || tokens[i].lexeme == "while"))
        {
            std::vector<Token> cond = collectCondition(i + 1);
            if (!cond.empty())
            {
                spans.push_back({i, "condition", cond});
            }
        }
    }

    std::sort(spans.begin(), spans.end(), [](const CollectedSpan& a, const CollectedSpan& b)
    {
        return a.startIndex < b.startIndex;
    });
    return spans;
}

std::string toTerminal(const Token& token)
{
    if (token.type == TokenType::IDENTIFIER || token.type == TokenType::CONSTANT)
    {
        return "i";
    }
    if (token.lexeme == "/" || token.lexeme == "-")
    {
        return token.lexeme == "/" ? "*" : "+";
    }
    return token.lexeme;
}

bool isRelOp(const Token& token)
{
    return token.type == TokenType::OPERATOR &&
           (token.lexeme == "<" || token.lexeme == ">" || token.lexeme == "=" ||
            token.lexeme == "!=" || token.lexeme == "<=" || token.lexeme == ">=");
}

std::vector<Token> normalizeExpressionTokens(const std::vector<Token>& tokens)
{
    std::vector<Token> normalized;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        const Token& token = tokens[i];
        if (token.type == TokenType::IDENTIFIER)
        {
            normalized.push_back(token);
            std::size_t j = i + 1;
            while (j < tokens.size())
            {
                const Token& next = tokens[j];
                if (next.type == TokenType::DELIMITER && next.lexeme == ".")
                {
                    if (j + 1 < tokens.size() && tokens[j + 1].type == TokenType::IDENTIFIER)
                    {
                        j += 2;
                        continue;
                    }
                    break;
                }
                if (next.type == TokenType::DELIMITER && next.lexeme == "[")
                {
                    int depth = 1;
                    ++j;
                    while (j < tokens.size() && depth > 0)
                    {
                        if (tokens[j].type == TokenType::DELIMITER && tokens[j].lexeme == "[") ++depth;
                        else if (tokens[j].type == TokenType::DELIMITER && tokens[j].lexeme == "]") --depth;
                        ++j;
                    }
                    continue;
                }
                break;
            }
            i = j - 1;
            continue;
        }

        if (token.type == TokenType::CONSTANT
            || (token.type == TokenType::OPERATOR && (token.lexeme == "+" || token.lexeme == "-" || token.lexeme == "*" || token.lexeme == "/"))
            || (token.type == TokenType::DELIMITER && (token.lexeme == "(" || token.lexeme == ")")))
        {
            normalized.push_back(token);
        }
    }
    return normalized;
}

std::string join(const std::vector<std::string>& values)
{
    std::ostringstream oss;
    for (const auto& value : values)
    {
        oss << value;
    }
    return oss.str();
}

int rightMostTerminalIndex(const std::vector<std::string>& stack, const ParserCore& core)
{
    for (int i = static_cast<int>(stack.size()) - 1; i >= 0; --i)
    {
        if (core.index.find(stack[i]) != core.index.end())
        {
            return i;
        }
    }
    return -1;
}

bool reduceOnce(std::vector<std::string>& stack)
{
    if (!stack.empty() && stack.back() == "i")
    {
        stack.back() = "N";
        return true;
    }
    if (stack.size() >= 3)
    {
        int n = static_cast<int>(stack.size());
        if (stack[n - 3] == "N" && (stack[n - 2] == "+" || stack[n - 2] == "*") && stack[n - 1] == "N")
        {
            stack.resize(n - 3);
            stack.push_back("N");
            return true;
        }
        if (stack[n - 3] == "(" && stack[n - 2] == "N" && stack[n - 1] == ")")
        {
            stack.resize(n - 3);
            stack.push_back("N");
            return true;
        }
    }
    return false;
}

} // namespace

namespace simple_precedence
{

SimplePrecedenceResult analyzeExpressionTokens(const std::vector<Token>& expressionTokens)
{
    SimplePrecedenceResult result;
    result.enabled = true;
    ParserCore core = buildCore();
    result.symbols = core.symbols;
    result.table = core.table;

    std::vector<Token> normalizedTokens = normalizeExpressionTokens(expressionTokens);
    if (normalizedTokens.empty())
    {
        result.success = false;
        result.errorMessage = "简单优先分析输入为空";
        return result;
    }

    std::vector<std::string> input;
    std::ostringstream exprText;
    for (const auto& token : expressionTokens)
    {
        exprText << token.lexeme << " ";
    }
    for (const auto& token : normalizedTokens)
    {
        input.push_back(toTerminal(token));
    }
    input.push_back("#");
    result.expressionText = exprText.str();

    std::vector<std::string> stack = {"#"};
    int ip = 0;

    while (ip < static_cast<int>(input.size()))
    {
        int topTermIndex = rightMostTerminalIndex(stack, core);
        std::string topTerm = (topTermIndex >= 0) ? stack[topTermIndex] : "";
        std::string cur = input[ip];

        if (core.index.find(topTerm) == core.index.end() || core.index.find(cur) == core.index.end())
        {
            result.success = false;
            result.errorMessage = "分析失败：出现不在优先表中的终结符";
            return result;
        }

        std::string rel = core.table[core.index[topTerm]][core.index[cur]];
        SimplePrecedenceStep step;
        step.stackText = join(stack);
        step.inputText = join(std::vector<std::string>(input.begin() + ip, input.end()));
        step.relationText = topTerm + " " + rel + " " + cur;

        if (rel == "<" || rel == "=")
        {
            stack.push_back(cur);
            ++ip;
            step.actionText = "移进";
            result.steps.push_back(step);
        }
        else if (rel == ">")
        {
            bool ok = reduceOnce(stack);
            step.actionText = ok ? "归约" : "归约失败";
            result.steps.push_back(step);
            if (!ok)
            {
                result.success = false;
                result.errorMessage = "分析失败：无法归约当前句柄";
                return result;
            }
        }
        else
        {
            result.success = false;
            step.actionText = "错误";
            result.steps.push_back(step);
            result.errorMessage = "分析失败：优先关系未定义";
            return result;
        }

        if (stack.size() == 2 && stack[0] == "#" && stack[1] == "N" && ip < static_cast<int>(input.size()) && input[ip] == "#")
        {
            SimplePrecedenceStep acceptStep;
            acceptStep.stackText = join(stack);
            acceptStep.inputText = join(std::vector<std::string>(input.begin() + ip, input.end()));
            acceptStep.relationText = "# = #";
            acceptStep.actionText = "接受";
            result.steps.push_back(acceptStep);
            result.success = true;
            return result;
        }
    }

    result.success = false;
    result.errorMessage = "分析失败：输入提前结束";
    return result;
}

SimplePrecedenceResult analyzeFromTokens(const std::vector<Token>& tokens)
{
    std::vector<Token> exprTokens = extractFirstExpression(tokens);
    if (exprTokens.empty())
    {
        SimplePrecedenceResult result;
        result.enabled = true;
        ParserCore core = buildCore();
        result.symbols = core.symbols;
        result.table = core.table;
        result.success = false;
        result.errorMessage = "未找到可用于简单优先分析的赋值表达式";
        return result;
    }
    return analyzeExpressionTokens(exprTokens);
}

std::vector<SimplePrecedenceResult> analyzeAllExpressionsFromTokens(const std::vector<Token>& tokens)
{
    std::vector<SimplePrecedenceResult> all;
    std::vector<CollectedSpan> spans = extractAllSpans(tokens);
    if (spans.empty())
    {
        all.push_back(analyzeFromTokens(tokens));
        return all;
    }
    for (const auto& span : spans)
    {
        if (span.kind == "assign")
        {
            all.push_back(analyzeExpressionTokens(span.tokens));
        }
        else
        {
            all.push_back(analyzeRelationConditionTokens(span.tokens));
        }
    }
    return all;
}

SimplePrecedenceResult analyzeRelationConditionTokens(const std::vector<Token>& conditionTokens)
{
    SimplePrecedenceResult result;
    result.enabled = true;
    ParserCore core = buildCore();
    result.symbols = core.symbols;
    result.table = core.table;

    int relPos = -1;
    int depthParen = 0;
    int depthBracket = 0;
    for (int i = 0; i < static_cast<int>(conditionTokens.size()); ++i)
    {
        const Token& token = conditionTokens[i];
        if (token.type == TokenType::DELIMITER && token.lexeme == "(") ++depthParen;
        if (token.type == TokenType::DELIMITER && token.lexeme == ")") --depthParen;
        if (token.type == TokenType::DELIMITER && token.lexeme == "[") ++depthBracket;
        if (token.type == TokenType::DELIMITER && token.lexeme == "]") --depthBracket;
        if (depthParen == 0 && depthBracket == 0 && isRelOp(token))
        {
            relPos = i;
            break;
        }
    }

    if (relPos <= 0 || relPos >= static_cast<int>(conditionTokens.size()) - 1)
    {
        result.success = false;
        result.errorMessage = "条件简单优先分析失败：未找到合法关系表达式";
        return result;
    }

    std::vector<Token> left(conditionTokens.begin(), conditionTokens.begin() + relPos);
    std::vector<Token> right(conditionTokens.begin() + relPos + 1, conditionTokens.end());
    const std::string relop = conditionTokens[relPos].lexeme;

    SimplePrecedenceResult leftResult = analyzeExpressionTokens(left);
    if (!leftResult.success)
    {
        result.success = false;
        result.errorMessage = "条件左侧失败: " + leftResult.errorMessage;
        return result;
    }
    SimplePrecedenceResult rightResult = analyzeExpressionTokens(right);
    if (!rightResult.success)
    {
        result.success = false;
        result.errorMessage = "条件右侧失败: " + rightResult.errorMessage;
        return result;
    }

    std::ostringstream condText;
    for (const auto& token : conditionTokens)
    {
        condText << token.lexeme << " ";
    }
    result.success = true;
    result.expressionText = condText.str();
    result.steps = leftResult.steps;
    result.steps.push_back({"#", "#", relop, "关系规约"});
    result.steps.insert(result.steps.end(), rightResult.steps.begin(), rightResult.steps.end());
    return result;
}

} // namespace simple_precedence
