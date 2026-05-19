#ifndef MINIPASPLUS_SIMPLEPRECEDENCE_H
#define MINIPASPLUS_SIMPLEPRECEDENCE_H

#include "Token.h"
#include <map>
#include <string>
#include <vector>

struct SimplePrecedenceStep
{
    std::string stackText;
    std::string inputText;
    std::string relationText;
    std::string actionText;
};

struct SimplePrecedenceResult
{
    bool enabled = false;
    bool success = false;
    std::string expressionText;
    std::string errorMessage;
    std::vector<std::string> symbols;
    std::vector<std::vector<std::string>> table;
    std::vector<SimplePrecedenceStep> steps;
};

namespace simple_precedence
{

SimplePrecedenceResult analyzeFromTokens(const std::vector<Token>& tokens);
SimplePrecedenceResult analyzeExpressionTokens(const std::vector<Token>& expressionTokens);
std::vector<SimplePrecedenceResult> analyzeAllExpressionsFromTokens(const std::vector<Token>& tokens);
SimplePrecedenceResult analyzeRelationConditionTokens(const std::vector<Token>& conditionTokens);

} // namespace simple_precedence

#endif
