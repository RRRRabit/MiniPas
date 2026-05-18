#include "Parser.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

class RuleTraceGuard
{
public:
    RuleTraceGuard(Parser* parser, const std::string& ruleName)
        : parser_(parser), ruleName_(ruleName)
    {
        parser_->traceEnterRule(ruleName_);
    }

    ~RuleTraceGuard()
    {
        parser_->traceExitRule(ruleName_);
    }

private:
    Parser* parser_;
    std::string ruleName_;
};

Parser::Parser(std::vector<Token> tokens)
: tokens_(std::move(tokens)),
current_(0),
tempIndex_(0),
nextAddress_(0),
currentParamOffset_(0),
currentLocalOffset_(0),
currentFunctionTempSize_(0)
{}

CompileResult Parser::parse()
{
    result_ = CompileResult{};
    result_.tokens = tokens_;
    symbolTable_ = SymbolTable{};
    typeTable_ = TypeTable{};
    current_ = 0;
    tempIndex_ = 0;
    nextAddress_ = 0;
    currentParamOffset_ = 0;
    currentLocalOffset_ = 0;
    currentFunctionTempSize_ = 0;
    programName_.clear();
    currentFunctionName_.clear();
    functionInfos_.clear();
    parameterInfos_.clear();
    activationRecords_.clear();
    parserTraceDepth_ = 0;
    parserRuleStack_.clear();
    parserTraceTree_.clear();
    parserStepLog_.clear();
    parserActionLog_.clear();
    parserStepRuleNames_.clear();
    parserActionRuleNames_.clear();

    parseProgram();
    consume(TokenType::END_OF_FILE, "", "程序结束后不应再有其它内容");
    buildBasicBlocks();
    buildActivationRecords();
    result_.symbols = symbolTable_.entries();
    result_.recordTypes = typeTable_.recordTypes();
    result_.arrayTypes = typeTable_.arrayTypes();
    result_.functionTable = functionInfos_;
    result_.parameterTable = parameterInfos_;
    result_.activationRecords = activationRecords_;
    result_.parserTraceTree = parserTraceTree_;
    result_.parserStepLog = parserStepLog_;
    result_.parserActionLog = parserActionLog_;
    result_.parserStepRuleNames = parserStepRuleNames_;
    result_.parserActionRuleNames = parserActionRuleNames_;
    return result_;
}

// PROGRAM -> program id DECL_PART COMPOUND_STMT .
void Parser::parseProgram()
{
    RuleTraceGuard trace(this, "<PROGRAM>");
    consume(TokenType::KEYWORD, "program", "程序必须以 program 开头");
    Token programName = consumeIdentifier("program 后面应是程序名标识符");
    programName_ = programName.lexeme;
    symbolTable_.add({programName_, "program", "program", -1, 0});
    emit("program", programName_, "_", "_");

    parseDeclPart();
    parseCompoundStmt();
    consume(TokenType::DELIMITER, ".", "程序结尾缺少 .");
    emit("end", programName_, "_", "_");
}

// DECL_PART -> TYPE_DECL_PART FUNCTION_DECL_PART VAR_DECL_PART
void Parser::parseDeclPart()
{
    RuleTraceGuard trace(this, "<DECLARATION_PART>");
    parseTypeDeclPart();
    parseFunctionDeclPart();
    parseVarDeclPart();
}

// TYPE_DECL_PART -> type TYPE_DECL_LIST | ε
void Parser::parseTypeDeclPart()
{
    if (match(TokenType::KEYWORD, "type"))
    {
        parseTypeDeclList();
    }
}

// TYPE_DECL_LIST -> TYPE_DECL TYPE_DECL_LIST | ε
void Parser::parseTypeDeclList()
{
    while (check(TokenType::IDENTIFIER))
    {
        parseTypeDecl();
    }
}

// TYPE_DECL -> id = TYPE_SPEC ;
void Parser::parseTypeDecl()
{
    Token typeName = consumeIdentifier("type 声明中缺少类型名");
    if (typeTable_.findRecordType(typeName.lexeme) != nullptr || typeTable_.findArrayType(typeName.lexeme) != nullptr)
    {
        errorAtCurrent("类型重复声明: " + typeName.lexeme);
    }
    if (symbolTable_.find(typeName.lexeme) != nullptr)
    {
        errorAtCurrent("标识符重复声明: " + typeName.lexeme);
    }

    consume(TokenType::OPERATOR, "=", "类型名后面缺少 =");
    parseTypeSpec(typeName.lexeme);
    consume(TokenType::DELIMITER, ";", "类型声明后缺少 ;");

    int size = typeTable_.getTypeSize(typeName.lexeme);
    symbolTable_.add({typeName.lexeme, typeName.lexeme, "type", -1, size});
}

// TYPE_SPEC -> record FIELD_DECL_LIST end | array [ low .. high ] of TYPE_NAME
void Parser::parseTypeSpec(const std::string& typeName)
{
    if (check(TokenType::KEYWORD, "record"))
    {
        parseRecordTypeDecl(typeName);
        return;
    }
    if (check(TokenType::KEYWORD, "array"))
    {
        parseArrayTypeDecl(typeName);
        return;
    }
    errorAtCurrent("类型定义应为 record 或 array");
}

void Parser::parseRecordTypeDecl(const std::string& typeName)
{
    consume(TokenType::KEYWORD, "record", "= 后面应是 record");
    std::vector<FieldInfo> fields = parseFieldDeclList();
    consume(TokenType::KEYWORD, "end", "record 字段声明后缺少 end");
    typeTable_.addRecordType(typeName, fields);
}

void Parser::parseArrayTypeDecl(const std::string& typeName)
{
    consume(TokenType::KEYWORD, "array", "= 后面应是 array");
    consume(TokenType::DELIMITER, "[", "array 后面缺少 [");
    Token lowToken = consume(TokenType::CONSTANT, "", "数组下界应为整数常数");
    consume(TokenType::DELIMITER, "..", "数组上下界之间缺少 ..");
    Token highToken = consume(TokenType::CONSTANT, "", "数组上界应为整数常数");
    consume(TokenType::DELIMITER, "]", "数组上界后缺少 ]");
    consume(TokenType::KEYWORD, "of", "数组声明缺少 of");
    std::string elementType = parseTypeName();

    int low = std::stoi(lowToken.lexeme);
    int high = std::stoi(highToken.lexeme);
    if (low > high)
    {
        errorAtToken(lowToken, "数组下界不能大于上界");
    }
    typeTable_.addArrayType(typeName, low, high, elementType);
}

// FIELD_DECL_LIST -> FIELD_DECL FIELD_DECL_LIST | ε
std::vector<FieldInfo> Parser::parseFieldDeclList()
{
    std::vector<FieldInfo> fields;
    while (check(TokenType::IDENTIFIER))
    {
        fields.push_back(parseFieldDecl());
    }
    return fields;
}

// FIELD_DECL -> id : BASIC_TYPE ;
FieldInfo Parser::parseFieldDecl()
{
    Token fieldName = consumeIdentifier("record 字段声明缺少字段名");
    consume(TokenType::DELIMITER, ":", "record 字段名后面缺少 :");
    std::string type = parseBasicType();
    consume(TokenType::DELIMITER, ";", "record 字段声明后缺少 ;");
    return {fieldName.lexeme, type};
}

// BASIC_TYPE -> integer | real | char | boolean
std::string Parser::parseBasicType()
{
    if (isBasicTypeToken())
    {
        std::string type = peek().lexeme;
        ++current_;
        return type;
    }
    errorAtCurrent("应为基本类型 integer、real 或 char");
    return "";
}

void Parser::parseFunctionDeclPart()
{
    while (check(TokenType::KEYWORD, "function"))
    {
        parseFunctionDecl();
    }
}

// FUNCTION_DECL -> function id ( PARAM_LIST ) : TYPE_NAME ; VAR_DECL_PART COMPOUND_STMT ;
void Parser::parseFunctionDecl()
{
    consume(TokenType::KEYWORD, "function", "函数声明应以 function 开头");
    Token functionName = consumeIdentifier("function 后面应是函数名");
    currentFunctionName_ = functionName.lexeme;
    currentParamOffset_ = 0;
    currentLocalOffset_ = 0;
    currentFunctionTempSize_ = 0;
    int paramStart = static_cast<int>(parameterInfos_.size());

    consume(TokenType::DELIMITER, "(", "函数名后面缺少 (");
    if (!check(TokenType::DELIMITER, ")"))
    {
        parseParamList(functionName.lexeme);
    }
    consume(TokenType::DELIMITER, ")", "函数参数表后面缺少 )");
    consume(TokenType::DELIMITER, ":", "函数返回类型前缺少 :");
    std::string returnType = parseTypeName();
    consume(TokenType::DELIMITER, ";", "函数头后缺少 ;");

    int entryQuad = static_cast<int>(result_.quadruples.size());
    symbolTable_.add({functionName.lexeme, returnType, "function", -1, typeTable_.getTypeSize(returnType)});
    parseVarDeclPart(false, functionName.lexeme);
    parseCompoundStmt();
    // 函数默认在函数体末尾返回函数名对应的值（Pascal 风格：通过给函数名赋值返回）。
    emit("ret", functionName.lexeme, "_", "_");
    consume(TokenType::DELIMITER, ";", "函数体后缺少 ;");

    FunctionInfo info;
    info.name = functionName.lexeme;
    info.returnType = returnType;
    info.level = 2;
    info.offset = 0;
    info.paramStart = paramStart;
    info.paramCount = static_cast<int>(parameterInfos_.size()) - paramStart;
    info.entryQuad = entryQuad;
    info.localSize = currentLocalOffset_;
    info.tempSize = currentFunctionTempSize_;
    functionInfos_.push_back(info);
    currentFunctionName_.clear();
}

// PARAM_LIST -> PARAM { ; PARAM }
void Parser::parseParamList(const std::string& functionName)
{
    parseParam(functionName);
    while (match(TokenType::DELIMITER, ";"))
    {
        parseParam(functionName);
    }
}

// PARAM -> var? ID_LIST : TYPE_NAME
void Parser::parseParam(const std::string& functionName)
{
    bool byRef = match(TokenType::KEYWORD, "var");
    std::vector<std::string> names = parseIdList();
    consume(TokenType::DELIMITER, ":", "参数名后面缺少 :");
    std::string typeName = parseTypeName();
    for (const auto& name : names)
    {
        int size = typeTable_.getTypeSize(typeName);
        std::string passMode = byRef ? "vn" : "vf";
        // 先填 PARAM 形参表，再按同一规则复制到 SYNBL 总表。
        parameterInfos_.push_back({functionName, name, typeName, passMode, currentParamOffset_, size});
        std::string kind = byRef ? "var parameter of " + functionName : "parameter of " + functionName;
        symbolTable_.add({name, typeName, kind, currentParamOffset_, size});
        addActivationRecord(functionName, name, passMode, typeName, currentParamOffset_, size);
        currentParamOffset_ += size;
    }
}

// VAR_DECL_PART -> var VAR_DECL_LIST | ε
void Parser::parseVarDeclPart(bool required, const std::string& scope)
{
    if (!match(TokenType::KEYWORD, "var"))
    {
        if (required)
        {
            errorAtCurrent("声明部分缺少 var");
        }
        return;
    }
    parseVarDeclList(scope);
}

// VAR_DECL_LIST -> VAR_DECL VAR_DECL_LIST | ε
void Parser::parseVarDeclList(const std::string& scope)
{
    while (check(TokenType::IDENTIFIER))
    {
        parseVarDecl(scope);
    }
}

// VAR_DECL -> ID_LIST : TYPE_NAME ;
void Parser::parseVarDecl(const std::string& scope)
{
    std::vector<std::string> names = parseIdList();
    consume(TokenType::DELIMITER, ":", "变量名后面缺少 :");
    std::string typeName = parseTypeName();
    consume(TokenType::DELIMITER, ";", "变量声明后缺少 ;");

    for (const auto& name : names)
    {
        if (symbolTable_.find(name) != nullptr)
        {
            errorAtCurrent("标识符重复声明: " + name);
        }
        int size = typeTable_.getTypeSize(typeName);
        std::string kind = typeTable_.findRecordType(typeName) ? "record variable" : "variable";
        if (typeTable_.findArrayType(typeName))
        {
            kind = "array variable";
        }
        if (!scope.empty())
        {
            kind = "local " + kind + " of " + scope;
        }
        if (scope.empty())
        {
            symbolTable_.add({name, typeName, kind, nextAddress_, size});
            addActivationRecord(programName_, name, kind, typeName, nextAddress_, size);
            nextAddress_ += size;
        }
        else
        {
            symbolTable_.add({name, typeName, kind, currentLocalOffset_, size});
            addActivationRecord(scope, name, "local", typeName, currentLocalOffset_, size);
            currentLocalOffset_ += size;
        }
    }
}

// ID_LIST -> id { , id }
std::vector<std::string> Parser::parseIdList()
{
    std::vector<std::string> names;
    names.push_back(consumeIdentifier("变量声明缺少标识符").lexeme);
    while (match(TokenType::DELIMITER, ","))
    {
        names.push_back(consumeIdentifier(", 后面应是标识符").lexeme);
    }
    return names;
}

// TYPE_NAME -> BASIC_TYPE | id
std::string Parser::parseTypeName()
{
    if (isBasicTypeToken())
    {
        std::string type = peek().lexeme;
        ++current_;
        return type;
    }

    if (check(TokenType::IDENTIFIER))
    {
        std::string typeName = peek().lexeme;
        if (typeTable_.findRecordType(typeName) == nullptr && typeTable_.findArrayType(typeName) == nullptr)
        {
            errorAtCurrent("使用了未声明的类型: " + typeName);
        }
        ++current_;
        return typeName;
    }

    errorAtCurrent("应为类型名");
    return "";
}

// COMPOUND_STMT -> begin STMT_LIST end
void Parser::parseCompoundStmt()
{
    RuleTraceGuard trace(this, "<COMPOUND_STATEMENT>");
    consume(TokenType::KEYWORD, "begin", "复合语句缺少 begin");
    parseStmtList();
    consume(TokenType::KEYWORD, "end", "复合语句缺少 end");
}

// STMT_LIST -> STMT { ; STMT }
void Parser::parseStmtList()
{
    if (!isStatementStart())
    {
        return;
    }

    parseStmt();
    while (match(TokenType::DELIMITER, ";"))
    {
        if (!isStatementStart())
        {
            break;
        }
        parseStmt();
    }
}

// STMT -> ASSIGN_STMT
void Parser::parseStmt()
{
    RuleTraceGuard trace(this, "<STATEMENT>");
    if (check(TokenType::KEYWORD, "begin"))
    {
        parseCompoundStmt();
        return;
    }
    if (check(TokenType::KEYWORD, "if"))
    {
        parseIfStmt();
        return;
    }
    if (check(TokenType::KEYWORD, "while"))
    {
        parseWhileStmt();
        return;
    }
    if (check(TokenType::IDENTIFIER)
    && current_ + 1 < tokens_.size()
    && tokens_[current_ + 1].type == TokenType::DELIMITER
    && tokens_[current_ + 1].lexeme == "(")
    {
        parseCallStmt();
        return;
    }
    parseAssignStmt();
}

// ASSIGN_STMT -> LEFT_VALUE := EXPRESSION
void Parser::parseAssignStmt()
{
    RuleTraceGuard trace(this, "<ASSIGNMENT_STATEMENT>");
    std::string leftValue = parseLeftValue();
    consume(TokenType::OPERATOR, ":=", "赋值语句缺少 :=");
    ExprResult expr = parseExpression();
    std::string targetType = resolveLValueType(leftValue);
    if (!canAssign(targetType, expr.type))
    {
        errorAtCurrent("赋值类型不兼容: " + targetType + " := " + expr.type);
    }
    emit(":=", expr.place, "_", leftValue);
}

void Parser::parseCallStmt()
{
    Token functionToken = consumeIdentifier("函数调用缺少函数名");
    parseFunctionCall(functionToken);
}

void Parser::parseWhileStmt()
{
    RuleTraceGuard trace(this, "<WHILE_STATEMENT>");
    consume(TokenType::KEYWORD, "while", "循环语句缺少 while");
    emit("wh", "_", "_", "_");

    ExprResult cond = parseCondition();
    emit("do", cond.place, "_", "_");

    consume(TokenType::KEYWORD, "do", "while 条件后缺少 do");
    parseStmt();
    emit("we", "_", "_", "_");
}

// IF_STMT -> if CONDITION then STMT [ else STMT ]
void Parser::parseIfStmt()
{
    RuleTraceGuard trace(this, "<IF_STATEMENT>");
    consume(TokenType::KEYWORD, "if", "条件语句缺少 if");
    ExprResult cond = parseCondition();
    emit("if", cond.place, "_", "_");

    consume(TokenType::KEYWORD, "then", "if 条件后缺少 then");
    parseStmt();

    if (match(TokenType::KEYWORD, "else"))
    {
        emit("el", "_", "_", "_");
        parseStmt();
    }
    emit("ie", "_", "_", "_");
}

Parser::ExprResult Parser::parseCondition()
{
    RuleTraceGuard trace(this, "<CONDITION>");
    ExprResult left = parseExpression();
    if (!(check(TokenType::OPERATOR, "<") || check(TokenType::OPERATOR, ">")
    || check(TokenType::OPERATOR, "=") || check(TokenType::OPERATOR, "!=")))
    {
        errorAtCurrent("条件表达式缺少关系运算符(<, >, =, !=)");
    }
    Token opToken = peek();
    std::string relop = opToken.lexeme;
    ++current_;
    ExprResult right = parseExpression();
    std::string boolTemp = newTemp("boolean");
    emit(relop, left.place, right.place, boolTemp);
    return {boolTemp, "boolean"};
}

// LEFT_VALUE -> id | id . id
std::string Parser::parseLeftValue()
{
    Token base = consumeIdentifier("赋值语句左部缺少变量名");
    if (match(TokenType::DELIMITER, "["))
    {
        ExprResult indexExpr = parseExpression();
        consume(TokenType::DELIMITER, "]", "数组下标后缺少 ]");
        std::string elementType = resolveArrayElementType(base, indexExpr);
        if (match(TokenType::DELIMITER, "."))
        {
            Token field = consumeIdentifier(". 后面应是字段名");
            errorAtToken(field, "暂不支持结构体数组字段访问: " + base.lexeme + "[...]." + field.lexeme);
        }
        (void)elementType;
        return base.lexeme + "[" + indexExpr.place + "]";
    }

    Token field;
    Token* fieldPtr = nullptr;

    if (match(TokenType::DELIMITER, "."))
    {
        field = consumeIdentifier(". 后面应是字段名");
        fieldPtr = &field;
    }

    checkLeftValue(base, fieldPtr);
    return fieldPtr == nullptr ? base.lexeme : base.lexeme + "." + field.lexeme;
}

// EXPRESSION -> TERM { (+|-) TERM }
Parser::ExprResult Parser::parseExpression()
{
    RuleTraceGuard trace(this, "<EXPRESSION>");
    ExprResult left = parseTerm();
    while (check(TokenType::OPERATOR, "+") || check(TokenType::OPERATOR, "-"))
    {
        Token opToken = peek();
        std::string op = peek().lexeme;
        ++current_;
        ExprResult right = parseTerm();
        std::string resultType = mergeNumericType(left.type, right.type, opToken);
        std::string temp = newTemp(resultType);
        emit(op, left.place, right.place, temp);
        left = {temp, resultType};
    }
    return left;
}

// TERM -> FACTOR { (*|/) FACTOR }
Parser::ExprResult Parser::parseTerm()
{
    RuleTraceGuard trace(this, "<TERM>");
    ExprResult left = parseFactor();
    while (check(TokenType::OPERATOR, "*") || check(TokenType::OPERATOR, "/"))
    {
        Token opToken = peek();
        std::string op = peek().lexeme;
        ++current_;
        ExprResult right = parseFactor();
        std::string resultType = mergeNumericType(left.type, right.type, opToken);
        std::string temp = newTemp(resultType);
        emit(op, left.place, right.place, temp);
        left = {temp, resultType};
    }
    return left;
}

// FACTOR -> id | id . id | constant | ( EXPRESSION )
Parser::ExprResult Parser::parseFactor()
{
    RuleTraceGuard trace(this, "<FACTOR>");
    if (check(TokenType::IDENTIFIER))
    {
        Token base = consumeIdentifier("表达式中缺少标识符");
        if (check(TokenType::DELIMITER, "("))
        {
            return parseFunctionCall(base);
        }
        if (match(TokenType::DELIMITER, "["))
        {
            ExprResult indexExpr = parseExpression();
            consume(TokenType::DELIMITER, "]", "数组下标后缺少 ]");
            std::string elementType = resolveArrayElementType(base, indexExpr);
            if (match(TokenType::DELIMITER, "."))
            {
                Token field = consumeIdentifier(". 后面应是字段名");
                errorAtToken(field, "暂不支持结构体数组字段访问: " + base.lexeme + "[...]." + field.lexeme);
            }
            std::string place = base.lexeme + "[" + indexExpr.place + "]";
            return {place, elementType};
        }

        Token field;
        Token* fieldPtr = nullptr;
        if (match(TokenType::DELIMITER, "."))
        {
            field = consumeIdentifier(". 后面应是字段名");
            fieldPtr = &field;
        }
        checkLeftValue(base, fieldPtr);
        std::string place = fieldPtr == nullptr ? base.lexeme : base.lexeme + "." + field.lexeme;
        return {place, resolveLValueType(place)};
    }

    if (check(TokenType::CONSTANT))
    {
        Token c = tokens_[current_++];
        if (c.lexeme.size() >= 2 && c.lexeme.front() == '\'' && c.lexeme.back() == '\'')
        {
            return {c.lexeme, "char"};
        }
        if (c.lexeme.find('.') != std::string::npos)
        {
            return {c.lexeme, "real"};
        }
        return {c.lexeme, "integer"};
    }

    if (match(TokenType::DELIMITER, "("))
    {
        ExprResult value = parseExpression();
        consume(TokenType::DELIMITER, ")", "表达式缺少右括号 )");
        return value;
    }

    errorAtCurrent("表达式因子应为变量、常数或括号表达式");
    return {"", ""};
}

Parser::ExprResult Parser::parseFunctionCall(const Token& functionToken)
{
    const SymbolEntry* symbol = symbolTable_.find(functionToken.lexeme);
    if (symbol == nullptr || symbol->kind != "function")
    {
        errorAtToken(functionToken, "调用了未声明函数: " + functionToken.lexeme);
    }

    consume(TokenType::DELIMITER, "(", "函数调用缺少 (");
    std::vector<ExprResult> args;
    if (!check(TokenType::DELIMITER, ")"))
    {
        args.push_back(parseExpression());
        while (match(TokenType::DELIMITER, ","))
        {
            args.push_back(parseExpression());
        }
    }
    consume(TokenType::DELIMITER, ")", "函数调用缺少 )");

    std::vector<ParameterInfo> params;
    for (const auto& p : parameterInfos_)
    {
        if (p.functionName == functionToken.lexeme)
        {
            params.push_back(p);
        }
    }
    std::sort(params.begin(), params.end(), [](const ParameterInfo& a, const ParameterInfo& b)
    {
        return a.offset < b.offset;
    });

    if (static_cast<int>(args.size()) != static_cast<int>(params.size()))
    {
        errorAtToken(functionToken, "函数 " + functionToken.lexeme + " 实参数量不匹配");
    }

    for (int i = 0; i < static_cast<int>(args.size()); ++i)
    {
        if (params[i].passMode == "vn")
        {
            errorAtToken(functionToken, "当前仅支持值参调用，函数 " + functionToken.lexeme + " 含 var 参数");
        }
        if (!canAssign(params[i].type, args[i].type))
        {
            errorAtToken(functionToken, "函数 " + functionToken.lexeme + " 第 " + std::to_string(i + 1) + " 个参数类型不匹配");
        }
        emit("param", args[i].place, "vf", "_");
    }

    std::string retType = symbol->typeName;
    std::string resultPlace = "_";
    if (retType != "void" && !retType.empty())
    {
        resultPlace = newTemp(retType);
    }
    emit("call", functionToken.lexeme, std::to_string(args.size()), resultPlace);
    return {resultPlace, retType};
}

