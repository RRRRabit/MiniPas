#include "Parser.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

Parser::Parser(std::vector<Token> tokens)
: tokens_(std::move(tokens)),
current_(0),
tempIndex_(0),
nextAddress_(0),
currentParamOffset_(0),
currentLocalOffset_(0),
currentFunctionTempSize_(0),
traceDepth_(0)
{}

// Parser 总入口。
//
// CompilerFacade 调用 parser.parse() 后，Parser 会从 PROGRAM 规则开始分析整份程序。
// 如果中途遇到语法/语义错误，会通过 errorAtCurrent/errorAtToken 抛出异常。
// 如果成功，返回的 CompileResult 里会包含：
// - 符号表
// - record/array 类型表
// - 函数表和参数表
// - 四元式
// - 基本块
CompileResult Parser::parse()
{
    // 每次 parse 都要重置状态，避免同一个 Parser 对象复用时残留旧数据。
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
    traceDepth_ = 0;

    // 从文法开始符号 PROGRAM 开始分析。
    parseProgram();

    // 程序主体和结尾 "." 读完后，后面只能是 EOF。
    consume(TokenType::END_OF_FILE, "", "程序结束后不应再有其它内容");

    // 基本块是优化阶段要用的结构。
    buildBasicBlocks();

    // 把 Parser 内部表复制到结果对象，交给后续阶段使用。
    result_.symbols = symbolTable_.entries();
    result_.recordTypes = typeTable_.recordTypes();
    result_.arrayTypes = typeTable_.arrayTypes();
    result_.functionTable = functionInfos_;
    result_.parameterTable = parameterInfos_;
    return result_;
}

// PROGRAM -> program id DECL_PART COMPOUND_STMT .
void Parser::parseProgram()
{
    TraceScope trace(*this, "PROGRAM");
    // PROGRAM 是整个 MiniPas 程序的最外层结构：
    //     program 程序名
    //     声明部分
    //     begin
    //         语句
    //     end.
    consume(TokenType::KEYWORD, "program", "程序必须以 program 开头");
    Token programName = consumeIdentifier("program 后面应是程序名标识符");
    programName_ = programName.lexeme;
    symbolTable_.add({programName_, "program", "program", -1, 0});

    // 生成程序开始四元式，主要用于展示程序边界。
    emit("program", programName_, "_", "_");

    parseDeclPart();
    parseCompoundStmt();
    consume(TokenType::DELIMITER, ".", "程序结尾缺少 .");

    // 生成程序结束四元式。
    emit("end", programName_, "_", "_");
}

// DECL_PART -> TYPE_DECL_PART FUNCTION_DECL_PART VAR_DECL_PART
void Parser::parseDeclPart()
{
    TraceScope trace(*this, "DECL_PART");
    // 声明部分按固定顺序解析：
    // type 声明 -> function 声明 -> var 声明。
    // 这比完整 Pascal 简化很多，也让代码更容易讲。
    parseTypeDeclPart();
    parseFunctionDeclPart();
    parseVarDeclPart();
}

// TYPE_DECL_PART -> type TYPE_DECL_LIST | ε
void Parser::parseTypeDeclPart()
{
    TraceScope trace(*this, "TYPE_DECL_PART");
    // type 声明是可选的。
    // 如果当前 Token 是 type，就继续读类型声明列表；否则直接跳过。
    if (match(TokenType::KEYWORD, "type"))
    {
        parseTypeDeclList();
    }
}

// TYPE_DECL_LIST -> TYPE_DECL TYPE_DECL_LIST | ε
void Parser::parseTypeDeclList()
{
    TraceScope trace(*this, "TYPE_DECL_LIST");
    while (check(TokenType::IDENTIFIER))
    {
        parseTypeDecl();
    }
}

// TYPE_DECL -> id = TYPE_SPEC ;
void Parser::parseTypeDecl()
{
    TraceScope trace(*this, "TYPE_DECL");
    // 类型声明形如：
    //     arr = array[1..5] of integer;
    //     person = record ... end;
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

    // 类型名也加入符号表，这样可以检测类型名和变量名重复。
    int size = typeTable_.getTypeSize(typeName.lexeme);
    symbolTable_.add({typeName.lexeme, typeName.lexeme, "type", -1, size});
}

// TYPE_SPEC -> record FIELD_DECL_LIST end | array [ low .. high ] of TYPE_NAME
void Parser::parseTypeSpec(const std::string& typeName)
{
    TraceScope trace(*this, "TYPE_SPEC");
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
    TraceScope trace(*this, "RECORD_TYPE_DECL");
    // record 类型形如：
    //     person = record
    //         age: integer;
    //         score: real;
    //     end;
    consume(TokenType::KEYWORD, "record", "= 后面应是 record");
    std::vector<FieldInfo> fields = parseFieldDeclList();
    consume(TokenType::KEYWORD, "end", "record 字段声明后缺少 end");
    typeTable_.addRecordType(typeName, fields);
}

void Parser::parseArrayTypeDecl(const std::string& typeName)
{
    TraceScope trace(*this, "ARRAY_TYPE_DECL");
    // array 类型形如：
    //     arr = array[1..5] of integer;
    //
    // low/high 必须是整数常量。
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
    // 语义检查：数组下界不能大于上界。
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
    TraceScope trace(*this, "FIELD_DECL");
    Token fieldName = consumeIdentifier("record 字段声明缺少字段名");
    consume(TokenType::DELIMITER, ":", "record 字段名后面缺少 :");
    std::string type = parseBasicType();
    consume(TokenType::DELIMITER, ";", "record 字段声明后缺少 ;");
    return {fieldName.lexeme, type};
}

// BASIC_TYPE -> integer | real | char | boolean
std::string Parser::parseBasicType()
{
    TraceScope trace(*this, "BASIC_TYPE");
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
    TraceScope trace(*this, "FUNCTION_DECL_PART");
    while (check(TokenType::KEYWORD, "function"))
    {
        parseFunctionDecl();
    }
}

// FUNCTION_DECL -> function id ( PARAM_LIST ) : TYPE_NAME ; VAR_DECL_PART COMPOUND_STMT ;
void Parser::parseFunctionDecl()
{
    TraceScope trace(*this, "FUNCTION_DECL");
    // 函数声明形如：
    //     function add(a,b: integer): integer;
    //     begin
    //         add := a + b
    //     end;
    //
    // 本项目采用 Pascal 风格：给函数名赋值表示返回值。
    consume(TokenType::KEYWORD, "function", "函数声明应以 function 开头");
    Token functionName = consumeIdentifier("function 后面应是函数名");
    currentFunctionName_ = functionName.lexeme;
    currentParamOffset_ = 0;
    currentLocalOffset_ = 0;
    currentFunctionTempSize_ = 0;

    // paramStart 记录这个函数的参数从 parameterInfos_ 的哪个位置开始。
    // 后面 FunctionInfo 会保存 paramStart 和 paramCount。
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

    // entryQuad 是函数体入口四元式下标。
    // VM 生成 CALL 指令时会跳到这里。
    int entryQuad = static_cast<int>(result_.quadruples.size());
    symbolTable_.add({functionName.lexeme, returnType, "function", -1, typeTable_.getTypeSize(returnType)});
    parseVarDeclPart(false, functionName.lexeme);
    parseCompoundStmt();
    // 函数默认在函数体末尾返回函数名对应的值。
    // 例如 add := a + b 后，ret add 就会把 add 当前值作为返回值。
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
    TraceScope trace(*this, "PARAM_LIST");
    parseParam(functionName);
    while (match(TokenType::DELIMITER, ";"))
    {
        parseParam(functionName);
    }
}

// PARAM -> var? ID_LIST : TYPE_NAME
void Parser::parseParam(const std::string& functionName)
{
    TraceScope trace(*this, "PARAM");
    // 参数形如：
    //     a,b: integer
    // 或：
    //     var x: integer
    //
    // byRef=true 表示 var 参数；当前函数调用阶段主要支持值参。
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
        currentParamOffset_ += size;
    }
}

// VAR_DECL_PART -> var VAR_DECL_LIST | ε
void Parser::parseVarDeclPart(bool required, const std::string& scope)
{
    TraceScope trace(*this, "VAR_DECL_PART");
    // var 声明在主程序里是必需的，在函数内部是可选的。
    // required=true 时，如果没看到 var 就报错。
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
    TraceScope trace(*this, "VAR_DECL_LIST");
    while (check(TokenType::IDENTIFIER))
    {
        parseVarDecl(scope);
    }
}

// VAR_DECL -> ID_LIST : TYPE_NAME ;
void Parser::parseVarDecl(const std::string& scope)
{
    TraceScope trace(*this, "VAR_DECL");
    // 变量声明形如：
    //     a,b: integer;
    //
    // scope 为空表示全局变量；scope 非空表示某个函数的局部变量。
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

        // 根据类型表判断这是普通变量、record 变量还是 array 变量。
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
            // 全局变量使用 nextAddress_ 分配地址。
            symbolTable_.add({name, typeName, kind, nextAddress_, size});
            nextAddress_ += size;
        }
        else
        {
            // 函数局部变量使用 currentLocalOffset_ 分配相对偏移。
            symbolTable_.add({name, typeName, kind, currentLocalOffset_, size});
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
    TraceScope trace(*this, "TYPE_NAME");
    // 类型名可以是基本类型，也可以是用户用 type 声明过的 record/array 类型。
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
            // 这个额外判断是为了给常见拼写错误更友好的提示：
            // 如果声明区本应写类型名，却看到了 id :=，很可能是 BEGIN 写错了。
            if (current_ + 1 < tokens_.size()
                && tokens_[current_ + 1].type == TokenType::OPERATOR
                && tokens_[current_ + 1].lexeme == ":=")
            {
                errorAtCurrent("声明区语法错误：此处应为类型名，但读到了赋值语句。请检查是否将 BEGIN 误写为标识符（如 BEG）。");
            }
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
    TraceScope trace(*this, "COMPOUND_STMT");
    // 复合语句就是 begin ... end。
    // 主程序主体和函数体都使用这个结构。
    consume(TokenType::KEYWORD, "begin", "复合语句缺少 begin");
    parseStmtList();

    if (!check(TokenType::KEYWORD, "end"))
    {
        if (check(TokenType::KEYWORD, "else"))
        {
            errorAtCurrent("条件分支结构错误：ELSE 未与 IF 正确匹配。请检查 THEN 分支末尾是否存在多余分号 ';'。");
        }
        errorAtCurrent("复合语句缺少 end");
    }

    consume(TokenType::KEYWORD, "end", "复合语句缺少 end");
}

// STMT_LIST -> STMT { ; STMT }
void Parser::parseStmtList()
{
    TraceScope trace(*this, "STMT_LIST");
    // 语句列表允许为空。
    // 如果当前 Token 不是任何语句开头，直接返回。
    if (!isStatementStart())
    {
        return;
    }

    parseStmt();
    while (match(TokenType::DELIMITER, ";"))
    {
        // 分号后面如果没有下一条语句，就结束。
        // 这允许 begin a:=1; end 这种末尾带分号的写法。
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
    TraceScope trace(*this, "STMT");
    // 语句分发器：根据当前 Token 判断到底是哪种语句。
    // 这是读 Parser 时非常关键的函数。
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
        // 标识符后面紧跟 "("，认为是函数调用语句。
        parseCallStmt();
        return;
    }

    // 否则，标识符开头默认按赋值语句处理。
    parseAssignStmt();
}

// ASSIGN_STMT -> LEFT_VALUE := EXPRESSION
void Parser::parseAssignStmt()
{
    TraceScope trace(*this, "ASSIGN_STMT");
    // 赋值语句形如：
    //     leftValue := expression
    //
    // leftValue 可以是 a、p.age、arr[i]。
    std::string leftValue = parseLeftValue();
    consume(TokenType::OPERATOR, ":=", "赋值语句缺少 :=");
    ExprResult expr = parseExpression();

    // 语义检查：右侧表达式类型能不能赋给左侧变量类型。
    std::string targetType = resolveLValueType(leftValue);
    if (!canAssign(targetType, expr.type))
    {
        errorAtCurrent("赋值类型不兼容: " + targetType + " := " + expr.type);
    }
    emit(":=", expr.place, "_", leftValue);
}

void Parser::parseCallStmt()
{
    TraceScope trace(*this, "CALL_STMT");
    Token functionToken = consumeIdentifier("函数调用缺少函数名");
    parseFunctionCall(functionToken);
}

void Parser::parseWhileStmt()
{
    TraceScope trace(*this, "WHILE_STMT");
    // while 语句四元式使用 wh/do/we 三个结构标记。
    // 之后 VM 指令生成阶段会把这些标记翻译成 FJ/JMP 跳转。
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
    TraceScope trace(*this, "IF_STMT");
    // if 语句四元式使用 if/el/ie 三个结构标记。
    // el 只有存在 else 分支时才生成。
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
    TraceScope trace(*this, "CONDITION");
    // 条件表达式目前只支持一个关系运算：
    //     expression < expression
    //     expression > expression
    //     expression = expression
    //     expression != expression
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
    // 关系运算结果放进 boolean 临时变量。
    emit(relop, left.place, right.place, boolTemp);
    return {boolTemp, "boolean"};
}

// LEFT_VALUE -> id | id . id
std::string Parser::parseLeftValue()
{
    TraceScope trace(*this, "LEFT_VALUE");
    // 左值是“可以被赋值的位置”。
    // 支持：
    //     a
    //     p.age
    //     arr[i]
    Token base = consumeIdentifier("赋值语句左部缺少变量名");
    if (match(TokenType::DELIMITER, "["))
    {
        ExprResult indexExpr = parseExpression();
        consume(TokenType::DELIMITER, "]", "数组下标后缺少 ]");

        // 检查 base 必须是数组，并且 indexExpr 必须是 integer。
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
    TraceScope trace(*this, "EXPRESSION");
    // 表达式规则：
    //     EXPRESSION -> TERM { (+|-) TERM }
    //
    // 这里只处理加减。乘除交给 parseTerm。
    ExprResult left = parseTerm();
    while (check(TokenType::OPERATOR, "+") || check(TokenType::OPERATOR, "-"))
    {
        Token opToken = peek();
        std::string op = peek().lexeme;
        ++current_;
        ExprResult right = parseTerm();
        std::string resultType = mergeNumericType(left.type, right.type, opToken);
        std::string temp = newTemp(resultType);

        // 例如 a + b 生成：
        //     (+, a, b, t1)
        emit(op, left.place, right.place, temp);
        left = {temp, resultType};
    }
    return left;
}

// TERM -> FACTOR { (*|/) FACTOR }
Parser::ExprResult Parser::parseTerm()
{
    TraceScope trace(*this, "TERM");
    // 项规则：
    //     TERM -> FACTOR { (*|/) FACTOR }
    //
    // 因为 parseExpression 先调用 parseTerm，所以乘除优先级自然高于加减。
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
    TraceScope trace(*this, "FACTOR");
    // 因子是表达式的最小组成单位：
    // - 变量
    // - 数组元素
    // - record 字段
    // - 函数调用
    // - 常量
    // - 括号表达式
    if (check(TokenType::IDENTIFIER))
    {
        Token base = consumeIdentifier("表达式中缺少标识符");
        if (check(TokenType::DELIMITER, "("))
        {
            // id 后面紧跟 "("，说明这是函数调用表达式。
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
        // 常量因子不需要查符号表，直接根据字面量判断类型。
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
        // 括号表达式：(a + b)
        ExprResult value = parseExpression();
        consume(TokenType::DELIMITER, ")", "表达式缺少右括号 )");
        return value;
    }

    errorAtCurrent("表达式因子应为变量、常数或括号表达式");
    return {"", ""};
}

Parser::ExprResult Parser::parseFunctionCall(const Token& functionToken)
{
    TraceScope trace(*this, "FUNCTION_CALL");
    // 函数调用既可以作为语句：
    //     foo(1)
    // 也可以作为表达式：
    //     x := add(1, 2)
    const SymbolEntry* symbol = symbolTable_.find(functionToken.lexeme);
    if (symbol == nullptr || symbol->kind != "function")
    {
        errorAtToken(functionToken, "调用了未声明函数: " + functionToken.lexeme);
    }

    consume(TokenType::DELIMITER, "(", "函数调用缺少 (");
    std::vector<ExprResult> args;
    // 读取实参列表。每个实参本身都是一个表达式。
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
    // 从全局参数表里筛出当前函数的形参。
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
        // 当前 VM 实现只支持值参。遇到 var 参数主动报错，避免运行语义不清。
        if (params[i].passMode == "vn")
        {
            errorAtToken(functionToken, "当前仅支持值参调用，函数 " + functionToken.lexeme + " 含 var 参数");
        }
        if (!canAssign(params[i].type, args[i].type))
        {
            errorAtToken(functionToken, "函数 " + functionToken.lexeme + " 第 " + std::to_string(i + 1) + " 个参数类型不匹配");
        }
        // 每个实参生成一条 param 四元式。
        emit("param", args[i].place, "vf", "_");
    }

    std::string retType = symbol->typeName;
    std::string resultPlace = "_";
    if (retType != "void" && !retType.empty())
    {
        // 函数调用有返回值时，用临时变量接住返回结果。
        resultPlace = newTemp(retType);
    }
    // call 四元式格式：
    //     (call, 函数名, 参数个数, 返回值位置)
    emit("call", functionToken.lexeme, std::to_string(args.size()), resultPlace);
    return {resultPlace, retType};
}

