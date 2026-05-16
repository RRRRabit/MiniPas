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
      currentFunctionTempSize_(0) {}

CompileResult Parser::parse() {
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
    return result_;
}

// PROGRAM -> program id DECL_PART COMPOUND_STMT .
void Parser::parseProgram() {
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
void Parser::parseDeclPart() {
    parseTypeDeclPart();
    parseFunctionDeclPart();
    parseVarDeclPart();
}

// TYPE_DECL_PART -> type TYPE_DECL_LIST | ε
void Parser::parseTypeDeclPart() {
    if (match(TokenType::KEYWORD, "type")) {
        parseTypeDeclList();
    }
}

// TYPE_DECL_LIST -> TYPE_DECL TYPE_DECL_LIST | ε
void Parser::parseTypeDeclList() {
    while (check(TokenType::IDENTIFIER)) {
        parseTypeDecl();
    }
}

// TYPE_DECL -> id = TYPE_SPEC ;
void Parser::parseTypeDecl() {
    Token typeName = consumeIdentifier("type 声明中缺少类型名");
    if (typeTable_.findRecordType(typeName.lexeme) != nullptr || typeTable_.findArrayType(typeName.lexeme) != nullptr) {
        errorAtCurrent("类型重复声明: " + typeName.lexeme);
    }
    if (symbolTable_.find(typeName.lexeme) != nullptr) {
        errorAtCurrent("标识符重复声明: " + typeName.lexeme);
    }

    consume(TokenType::OPERATOR, "=", "类型名后面缺少 =");
    parseTypeSpec(typeName.lexeme);
    consume(TokenType::DELIMITER, ";", "类型声明后缺少 ;");

    int size = typeTable_.getTypeSize(typeName.lexeme);
    symbolTable_.add({typeName.lexeme, typeName.lexeme, "type", -1, size});
}

// TYPE_SPEC -> record FIELD_DECL_LIST end | array [ low .. high ] of TYPE_NAME
void Parser::parseTypeSpec(const std::string& typeName) {
    if (check(TokenType::KEYWORD, "record")) {
        parseRecordTypeDecl(typeName);
        return;
    }
    if (check(TokenType::KEYWORD, "array")) {
        parseArrayTypeDecl(typeName);
        return;
    }
    errorAtCurrent("类型定义应为 record 或 array");
}

void Parser::parseRecordTypeDecl(const std::string& typeName) {
    consume(TokenType::KEYWORD, "record", "= 后面应是 record");
    std::vector<FieldInfo> fields = parseFieldDeclList();
    consume(TokenType::KEYWORD, "end", "record 字段声明后缺少 end");
    typeTable_.addRecordType(typeName, fields);
}

void Parser::parseArrayTypeDecl(const std::string& typeName) {
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
    if (low > high) {
        errorAtToken(lowToken, "数组下界不能大于上界");
    }
    typeTable_.addArrayType(typeName, low, high, elementType);
}

// FIELD_DECL_LIST -> FIELD_DECL FIELD_DECL_LIST | ε
std::vector<FieldInfo> Parser::parseFieldDeclList() {
    std::vector<FieldInfo> fields;
    while (check(TokenType::IDENTIFIER)) {
        fields.push_back(parseFieldDecl());
    }
    return fields;
}

// FIELD_DECL -> id : BASIC_TYPE ;
FieldInfo Parser::parseFieldDecl() {
    Token fieldName = consumeIdentifier("record 字段声明缺少字段名");
    consume(TokenType::DELIMITER, ":", "record 字段名后面缺少 :");
    std::string type = parseBasicType();
    consume(TokenType::DELIMITER, ";", "record 字段声明后缺少 ;");
    return {fieldName.lexeme, type};
}

// BASIC_TYPE -> integer | real | char | boolean
std::string Parser::parseBasicType() {
    if (isBasicTypeToken()) {
        std::string type = peek().lexeme;
        ++current_;
        return type;
    }
    errorAtCurrent("应为基本类型 integer、real 或 char");
    return "";
}

void Parser::parseFunctionDeclPart() {
    while (check(TokenType::KEYWORD, "function")) {
        parseFunctionDecl();
    }
}

// FUNCTION_DECL -> function id ( PARAM_LIST ) : TYPE_NAME ; VAR_DECL_PART COMPOUND_STMT ;
void Parser::parseFunctionDecl() {
    consume(TokenType::KEYWORD, "function", "函数声明应以 function 开头");
    Token functionName = consumeIdentifier("function 后面应是函数名");
    currentFunctionName_ = functionName.lexeme;
    currentParamOffset_ = 0;
    currentLocalOffset_ = 0;
    currentFunctionTempSize_ = 0;
    int paramStart = static_cast<int>(parameterInfos_.size());

    consume(TokenType::DELIMITER, "(", "函数名后面缺少 (");
    if (!check(TokenType::DELIMITER, ")")) {
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
void Parser::parseParamList(const std::string& functionName) {
    parseParam(functionName);
    while (match(TokenType::DELIMITER, ";")) {
        parseParam(functionName);
    }
}

// PARAM -> var? ID_LIST : TYPE_NAME
void Parser::parseParam(const std::string& functionName) {
    bool byRef = match(TokenType::KEYWORD, "var");
    std::vector<std::string> names = parseIdList();
    consume(TokenType::DELIMITER, ":", "参数名后面缺少 :");
    std::string typeName = parseTypeName();
    for (const auto& name : names) {
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
void Parser::parseVarDeclPart(bool required, const std::string& scope) {
    if (!match(TokenType::KEYWORD, "var")) {
        if (required) {
            errorAtCurrent("声明部分缺少 var");
        }
        return;
    }
    parseVarDeclList(scope);
}

// VAR_DECL_LIST -> VAR_DECL VAR_DECL_LIST | ε
void Parser::parseVarDeclList(const std::string& scope) {
    while (check(TokenType::IDENTIFIER)) {
        parseVarDecl(scope);
    }
}

// VAR_DECL -> ID_LIST : TYPE_NAME ;
void Parser::parseVarDecl(const std::string& scope) {
    std::vector<std::string> names = parseIdList();
    consume(TokenType::DELIMITER, ":", "变量名后面缺少 :");
    std::string typeName = parseTypeName();
    consume(TokenType::DELIMITER, ";", "变量声明后缺少 ;");

    for (const auto& name : names) {
        if (symbolTable_.find(name) != nullptr) {
            errorAtCurrent("标识符重复声明: " + name);
        }
        int size = typeTable_.getTypeSize(typeName);
        std::string kind = typeTable_.findRecordType(typeName) ? "record variable" : "variable";
        if (typeTable_.findArrayType(typeName)) {
            kind = "array variable";
        }
        if (!scope.empty()) {
            kind = "local " + kind + " of " + scope;
        }
        if (scope.empty()) {
            symbolTable_.add({name, typeName, kind, nextAddress_, size});
            addActivationRecord(programName_, name, kind, typeName, nextAddress_, size);
            nextAddress_ += size;
        } else {
            symbolTable_.add({name, typeName, kind, currentLocalOffset_, size});
            addActivationRecord(scope, name, "local", typeName, currentLocalOffset_, size);
            currentLocalOffset_ += size;
        }
    }
}

// ID_LIST -> id { , id }
std::vector<std::string> Parser::parseIdList() {
    std::vector<std::string> names;
    names.push_back(consumeIdentifier("变量声明缺少标识符").lexeme);
    while (match(TokenType::DELIMITER, ",")) {
        names.push_back(consumeIdentifier(", 后面应是标识符").lexeme);
    }
    return names;
}

// TYPE_NAME -> BASIC_TYPE | id
std::string Parser::parseTypeName() {
    if (isBasicTypeToken()) {
        std::string type = peek().lexeme;
        ++current_;
        return type;
    }

    if (check(TokenType::IDENTIFIER)) {
        std::string typeName = peek().lexeme;
        if (typeTable_.findRecordType(typeName) == nullptr && typeTable_.findArrayType(typeName) == nullptr) {
            errorAtCurrent("使用了未声明的类型: " + typeName);
        }
        ++current_;
        return typeName;
    }

    errorAtCurrent("应为类型名");
    return "";
}

// COMPOUND_STMT -> begin STMT_LIST end
void Parser::parseCompoundStmt() {
    consume(TokenType::KEYWORD, "begin", "复合语句缺少 begin");
    parseStmtList();
    consume(TokenType::KEYWORD, "end", "复合语句缺少 end");
}

// STMT_LIST -> STMT { ; STMT }
void Parser::parseStmtList() {
    if (!isStatementStart()) {
        return;
    }

    parseStmt();
    while (match(TokenType::DELIMITER, ";")) {
        if (!isStatementStart()) {
            break;
        }
        parseStmt();
    }
}

// STMT -> ASSIGN_STMT
void Parser::parseStmt() {
    if (check(TokenType::KEYWORD, "begin")) {
        parseCompoundStmt();
        return;
    }
    if (check(TokenType::KEYWORD, "if")) {
        parseIfStmt();
        return;
    }
    if (check(TokenType::KEYWORD, "while")) {
        parseWhileStmt();
        return;
    }
    if (check(TokenType::IDENTIFIER)
        && current_ + 1 < tokens_.size()
        && tokens_[current_ + 1].type == TokenType::DELIMITER
        && tokens_[current_ + 1].lexeme == "(") {
        parseCallStmt();
        return;
    }
    parseAssignStmt();
}

// ASSIGN_STMT -> LEFT_VALUE := EXPRESSION
void Parser::parseAssignStmt() {
    std::string leftValue = parseLeftValue();
    consume(TokenType::OPERATOR, ":=", "赋值语句缺少 :=");
    ExprResult expr = parseExpression();
    std::string targetType = resolveLValueType(leftValue);
    if (!canAssign(targetType, expr.type)) {
        errorAtCurrent("赋值类型不兼容: " + targetType + " := " + expr.type);
    }
    emit(":=", expr.place, "_", leftValue);
}

void Parser::parseCallStmt() {
    Token functionToken = consumeIdentifier("函数调用缺少函数名");
    parseFunctionCall(functionToken);
}

void Parser::parseWhileStmt() {
    consume(TokenType::KEYWORD, "while", "循环语句缺少 while");
    emit("wh", "_", "_", "_");

    ExprResult cond = parseCondition();
    emit("do", cond.place, "_", "_");

    consume(TokenType::KEYWORD, "do", "while 条件后缺少 do");
    parseStmt();
    emit("we", "_", "_", "_");
}

// IF_STMT -> if CONDITION then STMT [ else STMT ]
void Parser::parseIfStmt() {
    consume(TokenType::KEYWORD, "if", "条件语句缺少 if");
    ExprResult cond = parseCondition();
    emit("if", cond.place, "_", "_");

    consume(TokenType::KEYWORD, "then", "if 条件后缺少 then");
    parseStmt();

    if (match(TokenType::KEYWORD, "else")) {
        emit("el", "_", "_", "_");
        parseStmt();
    }
    emit("ie", "_", "_", "_");
}

Parser::ExprResult Parser::parseCondition() {
    ExprResult left = parseExpression();
    if (!(check(TokenType::OPERATOR, "<") || check(TokenType::OPERATOR, ">")
          || check(TokenType::OPERATOR, "=") || check(TokenType::OPERATOR, "!="))) {
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
std::string Parser::parseLeftValue() {
    Token base = consumeIdentifier("赋值语句左部缺少变量名");
    if (match(TokenType::DELIMITER, "[")) {
        ExprResult indexExpr = parseExpression();
        consume(TokenType::DELIMITER, "]", "数组下标后缺少 ]");
        std::string elementType = resolveArrayElementType(base, indexExpr);
        if (match(TokenType::DELIMITER, ".")) {
            Token field = consumeIdentifier(". 后面应是字段名");
            errorAtToken(field, "暂不支持结构体数组字段访问: " + base.lexeme + "[...]." + field.lexeme);
        }
        (void)elementType;
        return base.lexeme + "[" + indexExpr.place + "]";
    }

    Token field;
    Token* fieldPtr = nullptr;

    if (match(TokenType::DELIMITER, ".")) {
        field = consumeIdentifier(". 后面应是字段名");
        fieldPtr = &field;
    }

    checkLeftValue(base, fieldPtr);
    return fieldPtr == nullptr ? base.lexeme : base.lexeme + "." + field.lexeme;
}

// EXPRESSION -> TERM { (+|-) TERM }
Parser::ExprResult Parser::parseExpression() {
    ExprResult left = parseTerm();
    while (check(TokenType::OPERATOR, "+") || check(TokenType::OPERATOR, "-")) {
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
Parser::ExprResult Parser::parseTerm() {
    ExprResult left = parseFactor();
    while (check(TokenType::OPERATOR, "*") || check(TokenType::OPERATOR, "/")) {
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
Parser::ExprResult Parser::parseFactor() {
    if (check(TokenType::IDENTIFIER)) {
        Token base = consumeIdentifier("表达式中缺少标识符");
        if (check(TokenType::DELIMITER, "(")) {
            return parseFunctionCall(base);
        }
        if (match(TokenType::DELIMITER, "[")) {
            ExprResult indexExpr = parseExpression();
            consume(TokenType::DELIMITER, "]", "数组下标后缺少 ]");
            std::string elementType = resolveArrayElementType(base, indexExpr);
            if (match(TokenType::DELIMITER, ".")) {
                Token field = consumeIdentifier(". 后面应是字段名");
                errorAtToken(field, "暂不支持结构体数组字段访问: " + base.lexeme + "[...]." + field.lexeme);
            }
            std::string place = base.lexeme + "[" + indexExpr.place + "]";
            return {place, elementType};
        }

        Token field;
        Token* fieldPtr = nullptr;
        if (match(TokenType::DELIMITER, ".")) {
            field = consumeIdentifier(". 后面应是字段名");
            fieldPtr = &field;
        }
        checkLeftValue(base, fieldPtr);
        std::string place = fieldPtr == nullptr ? base.lexeme : base.lexeme + "." + field.lexeme;
        return {place, resolveLValueType(place)};
    }

    if (check(TokenType::CONSTANT)) {
        Token c = tokens_[current_++];
        if (c.lexeme.size() >= 2 && c.lexeme.front() == '\'' && c.lexeme.back() == '\'') {
            return {c.lexeme, "char"};
        }
        if (c.lexeme.find('.') != std::string::npos) {
            return {c.lexeme, "real"};
        }
        return {c.lexeme, "integer"};
    }

    if (match(TokenType::DELIMITER, "(")) {
        ExprResult value = parseExpression();
        consume(TokenType::DELIMITER, ")", "表达式缺少右括号 )");
        return value;
    }

    errorAtCurrent("表达式因子应为变量、常数或括号表达式");
    return {"", ""};
}

Parser::ExprResult Parser::parseFunctionCall(const Token& functionToken) {
    const SymbolEntry* symbol = symbolTable_.find(functionToken.lexeme);
    if (symbol == nullptr || symbol->kind != "function") {
        errorAtToken(functionToken, "调用了未声明函数: " + functionToken.lexeme);
    }

    consume(TokenType::DELIMITER, "(", "函数调用缺少 (");
    std::vector<ExprResult> args;
    if (!check(TokenType::DELIMITER, ")")) {
        args.push_back(parseExpression());
        while (match(TokenType::DELIMITER, ",")) {
            args.push_back(parseExpression());
        }
    }
    consume(TokenType::DELIMITER, ")", "函数调用缺少 )");

    std::vector<ParameterInfo> params;
    for (const auto& p : parameterInfos_) {
        if (p.functionName == functionToken.lexeme) {
            params.push_back(p);
        }
    }
    std::sort(params.begin(), params.end(), [](const ParameterInfo& a, const ParameterInfo& b) {
        return a.offset < b.offset;
    });

    if (static_cast<int>(args.size()) != static_cast<int>(params.size())) {
        errorAtToken(functionToken, "函数 " + functionToken.lexeme + " 实参数量不匹配");
    }

    for (int i = 0; i < static_cast<int>(args.size()); ++i) {
        if (params[i].passMode == "vn") {
            errorAtToken(functionToken, "当前仅支持值参调用，函数 " + functionToken.lexeme + " 含 var 参数");
        }
        if (!canAssign(params[i].type, args[i].type)) {
            errorAtToken(functionToken, "函数 " + functionToken.lexeme + " 第 " + std::to_string(i + 1) + " 个参数类型不匹配");
        }
        emit("param", args[i].place, "vf", "_");
    }

    std::string retType = symbol->typeName;
    std::string resultPlace = "_";
    if (retType != "void" && !retType.empty()) {
        resultPlace = newTemp(retType);
    }
    emit("call", functionToken.lexeme, std::to_string(args.size()), resultPlace);
    return {resultPlace, retType};
}

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
    if (isAtEnd() && type != TokenType::END_OF_FILE) {
        return false;
    }
    if (peek().type != type) {
        return false;
    }
    return lexeme.empty() || peek().lexeme == lexeme;
}

bool Parser::match(TokenType type, const std::string& lexeme) {
    if (!check(type, lexeme)) {
        return false;
    }
    ++current_;
    return true;
}

Token Parser::consume(TokenType type, const std::string& lexeme, const std::string& reason) {
    if (check(type, lexeme)) {
        return tokens_[current_++];
    }
    errorAtCurrent(reason);
    return peek();
}

Token Parser::consumeIdentifier(const std::string& reason) {
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
    if (symbol == nullptr || symbol->kind == "program") {
        errorAtToken(nameToken, "使用了未声明变量: " + nameToken.lexeme);
    }
}

void Parser::checkLeftValue(const Token& baseToken, const Token* fieldToken) const {
    checkVariableDeclared(baseToken);

    const SymbolEntry* symbol = symbolTable_.find(baseToken.lexeme);
    if (fieldToken == nullptr) {
        return;
    }

    const RecordType* record = typeTable_.findRecordType(symbol->typeName);
    if (record == nullptr) {
        errorAtToken(*fieldToken, baseToken.lexeme + " 不是 record 变量，不能访问字段 " + fieldToken->lexeme);
    }

    if (typeTable_.findField(record->name, fieldToken->lexeme) == nullptr) {
        errorAtToken(*fieldToken, "record 类型 " + record->name + " 不存在字段: " + fieldToken->lexeme);
    }
}

std::string Parser::resolveArrayElementType(const Token& baseToken, const ExprResult& indexExpr) const {
    checkVariableDeclared(baseToken);
    if (indexExpr.type != "integer") {
        errorAtToken(baseToken, "数组下标必须是 integer，当前为: " + indexExpr.type);
    }

    const SymbolEntry* symbol = symbolTable_.find(baseToken.lexeme);
    if (symbol == nullptr) {
        errorAtToken(baseToken, "使用了未声明变量: " + baseToken.lexeme);
    }
    const ArrayType* array = typeTable_.findArrayType(symbol->typeName);
    if (array == nullptr) {
        errorAtToken(baseToken, baseToken.lexeme + " 不是数组变量，不能使用下标访问");
    }
    return array->elementType;
}

std::string Parser::resolveLValueType(const std::string& leftValue) const {
    std::size_t lbr = leftValue.find('[');
    if (lbr != std::string::npos) {
        std::string base = leftValue.substr(0, lbr);
        const SymbolEntry* symbol = symbolTable_.find(base);
        if (symbol == nullptr) {
            return "";
        }
        const ArrayType* array = typeTable_.findArrayType(symbol->typeName);
        if (array == nullptr) {
            return "";
        }
        return array->elementType;
    }

    std::size_t dotPos = leftValue.find('.');
    if (dotPos == std::string::npos) {
        const SymbolEntry* symbol = symbolTable_.find(leftValue);
        if (symbol == nullptr) {
            return "";
        }
        return symbol->typeName;
    }

    std::string base = leftValue.substr(0, dotPos);
    std::string field = leftValue.substr(dotPos + 1);
    const SymbolEntry* symbol = symbolTable_.find(base);
    if (symbol == nullptr) {
        return "";
    }
    const RecordType* record = typeTable_.findRecordType(symbol->typeName);
    if (record == nullptr) {
        return "";
    }
    const FieldInfo* fieldInfo = typeTable_.findField(record->name, field);
    if (fieldInfo == nullptr) {
        return "";
    }
    return fieldInfo->type;
}

std::string Parser::mergeNumericType(const std::string& leftType, const std::string& rightType, const Token& opToken) const {
    auto isNumeric = [](const std::string& t) {
        return t == "integer" || t == "real";
    };
    if (!isNumeric(leftType) || !isNumeric(rightType)) {
        errorAtToken(opToken, "算术运算只支持 integer/real");
    }
    if (leftType == "real" || rightType == "real") {
        return "real";
    }
    return "integer";
}

bool Parser::canAssign(const std::string& targetType, const std::string& sourceType) const {
    if (targetType == sourceType) {
        return true;
    }
    // 允许 integer 隐式提升给 real。
    if (targetType == "real" && sourceType == "integer") {
        return true;
    }
    return false;
}

std::string Parser::newTemp(const std::string& typeName) {
    std::string name = "t" + std::to_string(++tempIndex_);
    int size = typeTable_.getTypeSize(typeName);
    if (size <= 0) {
        size = 8;
    }
    int addr = -1;
    if (currentFunctionName_.empty()) {
        addr = nextAddress_;
        nextAddress_ += size;
        addActivationRecord(programName_, name, "tv", typeName, addr, size);
    } else {
        addr = currentLocalOffset_ + currentFunctionTempSize_;
        currentFunctionTempSize_ += size;
        addActivationRecord(currentFunctionName_, name, "tv", typeName, addr, size);
    }
    symbolTable_.add({name, typeName, "temporary", addr, size});
    if (!currentFunctionName_.empty()) {
        // currentFunctionTempSize_ 已在上面累加。
    }
    return name;
}

void Parser::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result) {
    result_.quadruples.push_back({op, arg1, arg2, result});
}

void Parser::addActivationRecord(const std::string& scope, const std::string& name, const std::string& category,
                                 const std::string& type, int offset, int size) {
    activationRecords_.push_back({scope, name, category, type, offset, size});
}

void Parser::buildActivationRecords() {
    activationRecords_.clear();

    const auto& symbols = symbolTable_.entries();

    auto appendFrame = [&](const std::string& scope, int level) {
        int off = 0;
        addActivationRecord(scope, "Old SP", "meta", "-", off++, 1);
        addActivationRecord(scope, "返回地址", "meta", "-", off++, 1);
        addActivationRecord(scope, "全局Display", "meta", "-", off++, 1);

        std::vector<ParameterInfo> params;
        for (const auto& p : parameterInfos_) {
            if (p.functionName == scope) {
                params.push_back(p);
            }
        }
        std::sort(params.begin(), params.end(), [](const ParameterInfo& a, const ParameterInfo& b) {
            return a.offset < b.offset;
        });

        addActivationRecord(scope, "参数个数", "meta", "integer", off++, 1);
        for (const auto& p : params) {
            int occupy = (p.passMode == "vn") ? 2 : p.size;
            addActivationRecord(scope, p.name, p.passMode, p.type, off, occupy);
            off += occupy;
        }

        addActivationRecord(scope, "Display表", "display", "-", off, level + 1);
        off += (level + 1);

        std::vector<SymbolEntry> locals;
        for (const auto& s : symbols) {
            if (scope == programName_) {
                bool isGlobalVar = (s.kind == "variable" || s.kind == "record variable" || s.kind == "array variable");
                if (isGlobalVar) {
                    locals.push_back(s);
                }
            } else {
                std::string tag = " of " + scope;
                bool isLocalVar = s.kind.rfind("local ", 0) == 0 && s.kind.find(tag) != std::string::npos;
                if (isLocalVar) {
                    locals.push_back(s);
                }
            }
        }

        std::sort(locals.begin(), locals.end(), [](const SymbolEntry& a, const SymbolEntry& b) {
            return a.address < b.address;
        });

        for (const auto& v : locals) {
            addActivationRecord(scope, v.name, "v", v.typeName, off, v.size);
            off += v.size;
        }
    };

    appendFrame(programName_, 1);
    for (const auto& f : functionInfos_) {
        appendFrame(f.name, f.level);
    }
}

void Parser::buildBasicBlocks() {
    result_.basicBlocks.clear();
    if (result_.quadruples.empty()) {
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
    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i) {
        const Quadruple& q = result_.quadruples[i];
        if (q.op == "if") {
            ifStack.push_back({i, -1});
        } else if (q.op == "el") {
            if (!ifStack.empty()) {
                ifStack.back().elIndex = i;
            }
        } else if (q.op == "ie") {
            if (!ifStack.empty()) {
                ifToIe[ifStack.back().ifIndex] = i;
                ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                ifStack.pop_back();
            }
        } else if (q.op == "wh") {
            whStack.push_back(i);
        } else if (q.op == "do") {
            int whIndex = whStack.empty() ? -1 : whStack.back();
            doStack.push_back({i, whIndex});
        } else if (q.op == "we") {
            if (!doStack.empty()) {
                DoCtx ctx = doStack.back();
                doStack.pop_back();
                doToWe[ctx.doIndex] = i;
                if (ctx.whIndex >= 0) {
                    weToWh[i] = ctx.whIndex;
                }
            }
            if (!whStack.empty()) {
                whStack.pop_back();
            }
        }
    }

    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i) {
        const Quadruple& q = result_.quadruples[i];

        if (q.op == "if") {
            if (i + 1 < static_cast<int>(result_.quadruples.size())) {
                leaders.insert(i + 1);
            }
            int falseTarget = -1;
            auto e = ifToEl.find(i);
            if (e != ifToEl.end() && e->second >= 0) {
                falseTarget = e->second + 1;
            } else {
                auto f = ifToIe.find(i);
                if (f != ifToIe.end()) {
                    falseTarget = f->second;
                }
            }
            if (falseTarget >= 0 && falseTarget < static_cast<int>(result_.quadruples.size())) {
                leaders.insert(falseTarget);
            }
            continue;
        }

        if (q.op == "do") {
            if (i + 1 < static_cast<int>(result_.quadruples.size())) {
                leaders.insert(i + 1);
            }
            auto f = doToWe.find(i);
            if (f != doToWe.end()) {
                int falseTarget = f->second + 1;
                if (falseTarget < static_cast<int>(result_.quadruples.size())) {
                    leaders.insert(falseTarget);
                }
            }
            continue;
        }

        if (q.op == "el") {
            // el 的跳转目标是当前 if 的 ie（已在 ifToIe 中可通过 ifIndex 查到）
            for (const auto& pair : ifToEl) {
                if (pair.second == i) {
                    auto ie = ifToIe.find(pair.first);
                    if (ie != ifToIe.end()) {
                        leaders.insert(ie->second);
                    }
                    break;
                }
            }
            continue;
        }

        if (q.op == "we") {
            auto w = weToWh.find(i);
            if (w != weToWh.end()) {
                int condEntry = w->second + 1;
                if (condEntry < static_cast<int>(result_.quadruples.size())) {
                    leaders.insert(condEntry);
                }
            }
            if (i + 1 < static_cast<int>(result_.quadruples.size())) {
                leaders.insert(i + 1);
            }
        }
    }

    std::vector<int> sortedLeaders(leaders.begin(), leaders.end());
    std::sort(sortedLeaders.begin(), sortedLeaders.end());

    for (int i = 0; i < static_cast<int>(sortedLeaders.size()); ++i) {
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
