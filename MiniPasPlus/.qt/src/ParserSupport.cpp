#include "Parser.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

Parser::TraceScope::TraceScope(Parser& parser, std::string rule)
    : parser(parser), rule(std::move(rule))
{
    // 进入一个 parseXXX 函数时记录调用树节点。
    // 这只服务 GUI 的“递归分析过程”页面，不影响语法分析本身。
    parser.traceEnter(this->rule);
}

Parser::TraceScope::~TraceScope()
{
    // 离开 parseXXX 函数时恢复调用深度。
    parser.traceExit();
}

void Parser::traceEnter(const std::string& rule)
{
    const int nodeId = ++traceNodeId_;
    result_.parserTrace.push_back({traceDepth_, rule, nodeId, "入口 子程序 " + rule});
    traceNodeStack_.push_back(nodeId);
    ++traceDepth_;
}

void Parser::traceExit()
{
    if (traceDepth_ > 0)
    {
        --traceDepth_;
    }
    if (!traceNodeStack_.empty())
    {
        traceNodeStack_.pop_back();
    }
}

void Parser::logParserStep(const std::string& rule, const std::string& text)
{
    (void)rule;
    const int nodeId = traceNodeStack_.empty() ? 0 : traceNodeStack_.back();
    result_.parserSteps.push_back({"", nodeId, text});
}

void Parser::logParserAction(const std::string& rule, const std::string& text)
{
    (void)rule;
    const int nodeId = traceNodeStack_.empty() ? 0 : traceNodeStack_.back();
    result_.parserActions.push_back({"", nodeId, text});
}

// 查看当前 Token，但不移动 current_。
// Parser 大部分判断都从 peek() 开始。
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
    // check 只判断，不消耗 Token。
    // type 必须一致；如果传了 lexeme，文本也必须一致。
    //
    // 例如 check(KEYWORD, "if") 表示：
    // 当前 Token 是否是关键字 if？
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
    // match 用于“可有可无”的语法。
    // 匹配成功：current_ 前进一格，返回 true。
    // 匹配失败：什么都不做，返回 false。
    //
    // 例子：if 语句里的 else 是可选的，所以用 match("else")。
    if (!check(type, lexeme))
    {
        return false;
    }
    const Token& token = tokens_[current_];
    logParserStep("TOKEN", "可选匹配成功: " + token.lexeme + " @(" + std::to_string(token.line) + "," + std::to_string(token.column) + ")");
    ++current_;
    return true;
}

Token Parser::consume(TokenType type, const std::string& lexeme, const std::string& reason)
{
    // consume 用于“必须出现”的语法。
    // 匹配成功：读走 Token。
    // 匹配失败：立刻报错。
    //
    // 例子：程序必须以 program 开头，所以用 consume(KEYWORD, "program", ...)
    if (check(type, lexeme))
    {
        Token token = tokens_[current_++];
        logParserStep("TOKEN", "匹配 Token: " + token.lexeme + " @(" + std::to_string(token.line) + "," + std::to_string(token.column) + ")");
        return token;
    }
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
    // 把错误位置和原因拼成异常消息。
    // CompilerFacade 会捕获这个异常，并写入 CompileResult。
    std::ostringstream oss;
    oss << "line " << token.line << ", column " << token.column << ": " << reason
    << "，当前 Token 为 '" << token.lexeme << "'";
    throw std::runtime_error(oss.str());
}

bool Parser::isBasicTypeToken() const {
    // MiniPas 支持的基本类型。
    // 自定义 record/array 类型不在这里判断，而是在 parseTypeName 里查 TypeTable。
    return check(TokenType::KEYWORD, "integer")
    || check(TokenType::KEYWORD, "real")
    || check(TokenType::KEYWORD, "char")
    || check(TokenType::KEYWORD, "boolean");
}

bool Parser::isStatementStart() const {
    // 判断当前 Token 能否作为一条语句的开始。
    // parseStmtList 用它来决定语句列表是否结束。
    return check(TokenType::IDENTIFIER)
    || check(TokenType::KEYWORD, "begin")
    || check(TokenType::KEYWORD, "if")
    || check(TokenType::KEYWORD, "while");
}

void Parser::checkVariableDeclared(const Token& nameToken) const {
    // 语义检查：变量必须先声明后使用。
    // 如果符号表里找不到，说明用户写了未声明变量。
    const SymbolEntry* symbol = symbolTable_.find(nameToken.lexeme);
    if (symbol == nullptr || symbol->kind == "program")
    {
        errorAtToken(nameToken, "使用了未声明变量: " + nameToken.lexeme);
    }
}

void Parser::checkLeftValue(const Token& baseToken, const Token* fieldToken) const {
    // 检查左值是否合法。
    //
    // 情况 1：a
    // 只需要检查 a 是否声明。
    //
    // 情况 2：p.age
    // 先检查 p 是否声明，再检查 p 是不是 record 类型，最后检查 age 字段是否存在。
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
    // 解析数组元素类型，同时做语义检查。
    //
    // 对 arr[i] 来说：
    // 1. arr 必须声明过。
    // 2. i 的类型必须是 integer。
    // 3. arr 的类型必须真的是 array。
    // 4. 返回 array 的元素类型，例如 integer / real。
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
    // 根据左值文本推断它的类型。
    //
    // 支持三种形式：
    // 1. a       -> 查符号表里 a 的类型
    // 2. arr[i]  -> 查 arr 的数组元素类型
    // 3. p.age   -> 查 p 的 record 类型，再查 age 字段类型
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
    // 算术表达式类型合并。
    //
    // 规则：
    // - + - * / 两边都必须是 integer 或 real。
    // - 只要有一边是 real，结果就是 real。
    // - 两边都是 integer，结果就是 integer。
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
    // 赋值兼容检查。
    //
    // targetType 是左边变量类型，sourceType 是右边表达式类型。
    // 例如：
    //     realVar := intVar
    // targetType=real, sourceType=integer，允许。
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
    // 生成临时变量。
    //
    // 表达式中间结果不能丢，需要找地方保存。
    // 例如 a + b * 4：
    //     b * 4 的结果放进 t1
    //     a + t1 的结果放进 t2
    //
    // tempIndex_ 每次加 1，所以临时变量名依次是 t1、t2、t3。
    std::string name = "t" + std::to_string(++tempIndex_);
    int size = typeTable_.getTypeSize(typeName);
    if (size <= 0)
    {
        size = 8;
    }
    int addr = -1;
    if (currentFunctionName_.empty())
    {
        // 主程序中的临时变量放在全局地址空间。
        addr = nextAddress_;
        nextAddress_ += size;
    }
    else
    {
        // 函数内部的临时变量放在当前函数的局部空间后面。
        addr = currentLocalOffset_ + currentFunctionTempSize_;
        currentFunctionTempSize_ += size;
    }
    symbolTable_.add({name, typeName, "temporary", addr, size});
    logParserAction("TEMP", "生成临时变量: " + name + "，类型=" + typeName);
    return name;
}

void Parser::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result)
{
    // 生成一条四元式。
    //
    // Parser 不直接执行程序，而是把源程序翻译成四元式。
    // 后续优化器和 VM 都围绕四元式工作。
    logParserAction("QUADRUPLE", "生成四元式: (" + op + ", " + arg1 + ", " + arg2 + ", " + result + ")");
    result_.quadruples.push_back({op, arg1, arg2, result});
}

void Parser::buildBasicBlocks()
{
    // 基本块划分。
    //
    // 基本块是一段连续四元式，只有一个入口和一个出口。
    // 优化器按基本块做局部优化，因为基本块内部没有复杂跳转，分析更简单。
    result_.basicBlocks.clear();
    if (result_.quadruples.empty())
    {
        return;
    }

    std::set<int> leaders;
    // 第 0 条四元式一定是第一个基本块入口。
    leaders.insert(0);

    // 第一遍扫描：把 if/el/ie 和 wh/do/we 的配对关系找出来。
    //
    // Parser 生成的是结构化控制流标记：
    // - if / el / ie 表示 if-else
    // - wh / do / we 表示 while
    //
    // 基本块划分需要知道这些结构的边界。
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

    // 第二遍扫描：根据控制流结构确定每个基本块的入口 leader。
    //
    // 常见 leader：
    // - 程序第一条四元式
    // - if 条件后 then 分支入口
    // - else 分支入口
    // - while 循环体入口
    // - 循环结束后的下一条
    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i)
    {
        const Quadruple& q = result_.quadruples[i];

        if (q.op == "if")
        {
            // if 后面第一条通常是 then 分支入口。
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
                // 条件为假时跳到 else 分支或 if 结束位置。
                leaders.insert(falseTarget);
            }
            continue;
        }

        if (q.op == "do")
        {
            // do 后面第一条是 while 循环体入口。
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
                    // while 条件为假时，跳到循环后面的第一条四元式。
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

    // leader 已排序后，相邻两个 leader 之间就是一个基本块。
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

