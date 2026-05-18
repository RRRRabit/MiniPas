#include "Parser.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType type, const std::string& lexeme) const {
    if (isAtEnd() && type != TokenType::END_OF_FILE)
    {
        return false;
    }
    if (peek().type != type)
    {
        return false;
    }
    return lexeme.empty() || peek().lexeme == lexeme;
}

bool Parser::match(TokenType type, const std::string& lexeme)
{
    if (!check(type, lexeme))
    {
        return false;
    }
    std::string expected = tokenTypeToString(type) + (lexeme.empty() ? "" : (":" + lexeme));
    traceStep("判定(y): 期望 " + expected + "，读入 " + peek().lexeme
              + "，执行 NEXT(w)"
              + " @(" + std::to_string(peek().line) + "," + std::to_string(peek().column) + ")");
    ++current_;
    return true;
}

Token Parser::consume(TokenType type, const std::string& lexeme, const std::string& reason)
{
    if (check(type, lexeme))
    {
        Token token = tokens_[current_++];
        std::string expected = tokenTypeToString(type) + (lexeme.empty() ? "" : (":" + lexeme));
        traceStep("判定(y): 期望 " + expected + "，读入 " + token.lexeme
                  + "，执行 NEXT(w)"
                  + " @(" + std::to_string(token.line) + "," + std::to_string(token.column) + ")");
        return token;
    }
    std::string got = isAtEnd() ? "EOF" : peek().lexeme;
    traceStep("判定(n): 期望 " + tokenTypeToString(type) + (lexeme.empty() ? "" : (":" + lexeme))
              + "，实际 " + got + "，转 err"
              + (isAtEnd() ? "" : (" @(" + std::to_string(peek().line) + "," + std::to_string(peek().column) + ")")));
    errorAtCurrent(reason);
    return peek();
}

Token Parser::consumeIdentifier(const std::string& reason)
{
    return consume(TokenType::IDENTIFIER, "", reason);
}

void Parser::errorAtCurrent(const std::string& reason) const {
    errorAtToken(peek(), reason);
}

void Parser::errorAtToken(const Token& token, const std::string& reason) const {
    std::ostringstream oss;
    oss << "line " << token.line << ", column " << token.column << ": " << reason
    << "，当前 Token 为 '" << token.lexeme << "'";
    throw std::runtime_error(oss.str());
}

bool Parser::isBasicTypeToken() const {
    return check(TokenType::KEYWORD, "integer")
    || check(TokenType::KEYWORD, "real")
    || check(TokenType::KEYWORD, "char")
    || check(TokenType::KEYWORD, "boolean");
}

bool Parser::isStatementStart() const {
    return check(TokenType::IDENTIFIER)
    || check(TokenType::KEYWORD, "begin")
    || check(TokenType::KEYWORD, "if")
    || check(TokenType::KEYWORD, "while");
}

void Parser::checkVariableDeclared(const Token& nameToken) const {
    const SymbolEntry* symbol = symbolTable_.find(nameToken.lexeme);
    if (symbol == nullptr || symbol->kind == "program")
    {
        errorAtToken(nameToken, "使用了未声明变量: " + nameToken.lexeme);
    }
}

void Parser::checkLeftValue(const Token& baseToken, const Token* fieldToken) const {
    checkVariableDeclared(baseToken);

    const SymbolEntry* symbol = symbolTable_.find(baseToken.lexeme);
    if (fieldToken == nullptr)
    {
        return;
    }

    const RecordType* record = typeTable_.findRecordType(symbol->typeName);
    if (record == nullptr)
    {
        errorAtToken(*fieldToken, baseToken.lexeme + " 不是 record 变量，不能访问字段 " + fieldToken->lexeme);
    }

    if (typeTable_.findField(record->name, fieldToken->lexeme) == nullptr)
    {
        errorAtToken(*fieldToken, "record 类型 " + record->name + " 不存在字段: " + fieldToken->lexeme);
    }
}

std::string Parser::resolveArrayElementType(const Token& baseToken, const ExprResult& indexExpr) const {
    checkVariableDeclared(baseToken);
    if (indexExpr.type != "integer")
    {
        errorAtToken(baseToken, "数组下标必须是 integer，当前为: " + indexExpr.type);
    }

    const SymbolEntry* symbol = symbolTable_.find(baseToken.lexeme);
    if (symbol == nullptr)
    {
        errorAtToken(baseToken, "使用了未声明变量: " + baseToken.lexeme);
    }
    const ArrayType* array = typeTable_.findArrayType(symbol->typeName);
    if (array == nullptr)
    {
        errorAtToken(baseToken, baseToken.lexeme + " 不是数组变量，不能使用下标访问");
    }
    return array->elementType;
}

std::string Parser::resolveLValueType(const std::string& leftValue) const {
    std::size_t lbr = leftValue.find('[');
    if (lbr != std::string::npos)
    {
        std::string base = leftValue.substr(0, lbr);
        const SymbolEntry* symbol = symbolTable_.find(base);
        if (symbol == nullptr)
        {
            return "";
        }
        const ArrayType* array = typeTable_.findArrayType(symbol->typeName);
        if (array == nullptr)
        {
            return "";
        }
        return array->elementType;
    }

    std::size_t dotPos = leftValue.find('.');
    if (dotPos == std::string::npos)
    {
        const SymbolEntry* symbol = symbolTable_.find(leftValue);
        if (symbol == nullptr)
        {
            return "";
        }
        return symbol->typeName;
    }

    std::string base = leftValue.substr(0, dotPos);
    std::string field = leftValue.substr(dotPos + 1);
    const SymbolEntry* symbol = symbolTable_.find(base);
    if (symbol == nullptr)
    {
        return "";
    }
    const RecordType* record = typeTable_.findRecordType(symbol->typeName);
    if (record == nullptr)
    {
        return "";
    }
    const FieldInfo* fieldInfo = typeTable_.findField(record->name, field);
    if (fieldInfo == nullptr)
    {
        return "";
    }
    return fieldInfo->type;
}

std::string Parser::mergeNumericType(const std::string& leftType, const std::string& rightType, const Token& opToken) const {
    auto isNumeric = [](const std::string& t)
    {
        return t == "integer" || t == "real";
    };
    if (!isNumeric(leftType) || !isNumeric(rightType))
    {
        errorAtToken(opToken, "算术运算只支持 integer/real");
    }
    if (leftType == "real" || rightType == "real")
    {
        return "real";
    }
    return "integer";
}

bool Parser::canAssign(const std::string& targetType, const std::string& sourceType) const {
    if (targetType == sourceType)
    {
        return true;
    }
    // 允许 integer 隐式提升给 real。
    if (targetType == "real" && sourceType == "integer")
    {
        return true;
    }
    return false;
}

std::string Parser::newTemp(const std::string& typeName)
{
    std::string name = "t" + std::to_string(++tempIndex_);
    int size = typeTable_.getTypeSize(typeName);
    if (size <= 0)
    {
        size = 8;
    }
    int addr = -1;
    if (currentFunctionName_.empty())
    {
        addr = nextAddress_;
        nextAddress_ += size;
        addActivationRecord(programName_, name, "tv", typeName, addr, size);
    }
    else
    {
        addr = currentLocalOffset_ + currentFunctionTempSize_;
        currentFunctionTempSize_ += size;
        addActivationRecord(currentFunctionName_, name, "tv", typeName, addr, size);
    }
    symbolTable_.add({name, typeName, "temporary", addr, size});
    if (!currentFunctionName_.empty())
    {
        // currentFunctionTempSize_ 已在上面累加。
    }
    traceAction("语义动作: 建立临时变量 " + name + " : " + typeName);
    return name;
}

void Parser::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result)
{
    result_.quadruples.push_back({op, arg1, arg2, result});
    traceAction("语义动作: 生成四元式 (" + op + ", " + arg1 + ", " + arg2 + ", " + result + ")");
}

void Parser::addActivationRecord(const std::string& scope, const std::string& name, const std::string& category,
const std::string& type, int offset, int size)
{
    activationRecords_.push_back({scope, name, category, type, offset, size});
}

void Parser::buildActivationRecords()
{
    activationRecords_.clear();

    const auto& symbols = symbolTable_.entries();

    auto appendFrame = [&](const std::string& scope, int level)
    {
        int off = 0;
        addActivationRecord(scope, "Old SP", "meta", "-", off++, 1);
        addActivationRecord(scope, "返回地址", "meta", "-", off++, 1);
        addActivationRecord(scope, "全局Display", "meta", "-", off++, 1);

        std::vector<ParameterInfo> params;
        for (const auto& p : parameterInfos_)
        {
            if (p.functionName == scope)
            {
                params.push_back(p);
            }
        }
        std::sort(params.begin(), params.end(), [](const ParameterInfo& a, const ParameterInfo& b)
        {
            return a.offset < b.offset;
        });

        addActivationRecord(scope, "参数个数", "meta", "integer", off++, 1);
        for (const auto& p : params)
        {
            int occupy = (p.passMode == "vn") ? 2 : p.size;
            addActivationRecord(scope, p.name, p.passMode, p.type, off, occupy);
            off += occupy;
        }

        addActivationRecord(scope, "Display表", "display", "-", off, level + 1);
        off += (level + 1);

        std::vector<SymbolEntry> locals;
        for (const auto& s : symbols)
        {
            if (scope == programName_)
            {
                bool isGlobalVar = (s.kind == "variable" || s.kind == "record variable" || s.kind == "array variable");
                if (isGlobalVar)
                {
                    locals.push_back(s);
                }
            }
            else
            {
                std::string tag = " of " + scope;
                bool isLocalVar = s.kind.rfind("local ", 0) == 0 && s.kind.find(tag) != std::string::npos;
                if (isLocalVar)
                {
                    locals.push_back(s);
                }
            }
        }

        std::sort(locals.begin(), locals.end(), [](const SymbolEntry& a, const SymbolEntry& b)
        {
            return a.address < b.address;
        });

        for (const auto& v : locals)
        {
            addActivationRecord(scope, v.name, "v", v.typeName, off, v.size);
            off += v.size;
        }
    };

    appendFrame(programName_, 1);
    for (const auto& f : functionInfos_)
    {
        appendFrame(f.name, f.level);
    }
}

void Parser::buildBasicBlocks()
{
    result_.basicBlocks.clear();
    if (result_.quadruples.empty())
    {
        return;
    }

    std::set<int> leaders;
    leaders.insert(0);

    struct IfCtx {
        int ifIndex = -1;
        int elIndex = -1;
    };
    std::map<int, int> ifToIe;
    std::map<int, int> ifToEl;
    std::vector<IfCtx> ifStack;
    struct DoCtx {
        int doIndex = -1;
        int whIndex = -1;
    };
    std::vector<DoCtx> doStack;
    std::map<int, int> doToWe;
    std::map<int, int> weToWh;
    std::vector<int> whStack;
    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i)
    {
        const Quadruple& q = result_.quadruples[i];
        if (q.op == "if")
        {
            ifStack.push_back({i, -1});
        }
        else if (q.op == "el")
        {
            if (!ifStack.empty())
            {
                ifStack.back().elIndex = i;
            }
        }
        else if (q.op == "ie")
        {
            if (!ifStack.empty())
            {
                ifToIe[ifStack.back().ifIndex] = i;
                ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                ifStack.pop_back();
            }
        }
        else if (q.op == "wh")
        {
            whStack.push_back(i);
        }
        else if (q.op == "do")
        {
            int whIndex = whStack.empty() ? -1 : whStack.back();
            doStack.push_back({i, whIndex});
        }
        else if (q.op == "we")
        {
            if (!doStack.empty())
            {
                DoCtx ctx = doStack.back();
                doStack.pop_back();
                doToWe[ctx.doIndex] = i;
                if (ctx.whIndex >= 0)
                {
                    weToWh[i] = ctx.whIndex;
                }
            }
            if (!whStack.empty())
            {
                whStack.pop_back();
            }
        }
    }

    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i)
    {
        const Quadruple& q = result_.quadruples[i];

        if (q.op == "if")
        {
            if (i + 1 < static_cast<int>(result_.quadruples.size()))
            {
                leaders.insert(i + 1);
            }
            int falseTarget = -1;
            auto e = ifToEl.find(i);
            if (e != ifToEl.end() && e->second >= 0)
            {
                falseTarget = e->second + 1;
            }
            else
            {
                auto f = ifToIe.find(i);
                if (f != ifToIe.end())
                {
                    falseTarget = f->second;
                }
            }
            if (falseTarget >= 0 && falseTarget < static_cast<int>(result_.quadruples.size()))
            {
                leaders.insert(falseTarget);
            }
            continue;
        }

        if (q.op == "do")
        {
            if (i + 1 < static_cast<int>(result_.quadruples.size()))
            {
                leaders.insert(i + 1);
            }
            auto f = doToWe.find(i);
            if (f != doToWe.end())
            {
                int falseTarget = f->second + 1;
                if (falseTarget < static_cast<int>(result_.quadruples.size()))
                {
                    leaders.insert(falseTarget);
                }
            }
            continue;
        }

        if (q.op == "el")
        {
            // el 的跳转目标是当前 if 的 ie（已在 ifToIe 中可通过 ifIndex 查到）
            for (const auto& pair : ifToEl)
            {
                if (pair.second == i)
                {
                    auto ie = ifToIe.find(pair.first);
                    if (ie != ifToIe.end())
                    {
                        leaders.insert(ie->second);
                    }
                    break;
                }
            }
            continue;
        }

        if (q.op == "we")
        {
            auto w = weToWh.find(i);
            if (w != weToWh.end())
            {
                int condEntry = w->second + 1;
                if (condEntry < static_cast<int>(result_.quadruples.size()))
                {
                    leaders.insert(condEntry);
                }
            }
            if (i + 1 < static_cast<int>(result_.quadruples.size()))
            {
                leaders.insert(i + 1);
            }
        }
    }

    std::vector<int> sortedLeaders(leaders.begin(), leaders.end());
    std::sort(sortedLeaders.begin(), sortedLeaders.end());

    for (int i = 0; i < static_cast<int>(sortedLeaders.size()); ++i)
    {
        BasicBlock block;
        block.id = i;
        block.start = sortedLeaders[i];
        int nextStart = (i + 1 < static_cast<int>(sortedLeaders.size()))
        ? sortedLeaders[i + 1]
        : static_cast<int>(result_.quadruples.size());
        block.end = nextStart - 1;
        result_.basicBlocks.push_back(block);
    }
}

void Parser::traceEnterRule(const std::string& ruleName)
{
    std::string indent(static_cast<std::size_t>(parserTraceDepth_) * 2, ' ');
    parserTraceTree_.push_back(indent + "入口 子程序 " + ruleName);
    parserRuleStack_.push_back(ruleName);
    ++parserTraceDepth_;
}

void Parser::traceExitRule(const std::string& ruleName)
{
    if (parserTraceDepth_ > 0)
    {
        --parserTraceDepth_;
    }
    std::string indent(static_cast<std::size_t>(parserTraceDepth_) * 2, ' ');
    parserTraceTree_.push_back(indent + "出口 子程序 " + ruleName);
    if (!parserRuleStack_.empty())
    {
        parserRuleStack_.pop_back();
    }
}

void Parser::traceStep(const std::string& message)
{
    parserStepLog_.push_back(message);
    parserStepRuleNames_.push_back(currentRuleContext());
}

void Parser::traceAction(const std::string& message)
{
    parserActionLog_.push_back(message);
    parserActionRuleNames_.push_back(currentRuleContext());
}

std::string Parser::currentRuleContext() const
{
    if (parserRuleStack_.empty())
    {
        return "<GLOBAL>";
    }
    return parserRuleStack_.back();
}



