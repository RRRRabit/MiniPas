#include "Parser.h"
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), current_(0), tempIndex_(0), nextAddress_(0) {}

CompileResult Parser::parse() {
    result_ = CompileResult{};
    result_.tokens = tokens_;
    symbolTable_ = SymbolTable{};
    typeTable_ = TypeTable{};
    current_ = 0;
    tempIndex_ = 0;
    nextAddress_ = 0;
    programName_.clear();

    parseProgram();
    consume(TokenType::END_OF_FILE, "", "程序结束后不应再有其它内容");
    result_.symbols = symbolTable_.entries();
    result_.recordTypes = typeTable_.recordTypes();
    result_.arrayTypes = typeTable_.arrayTypes();
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

    consume(TokenType::OPERATOR, "=", "类型名后面缺少 =");
    parseTypeSpec(typeName.lexeme);
    consume(TokenType::DELIMITER, ";", "类型声明后缺少 ;");
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
    consume(TokenType::DELIMITER, ".", "数组上下界之间缺少 ..");
    consume(TokenType::DELIMITER, ".", "数组上下界之间缺少 ..");
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

    consume(TokenType::DELIMITER, "(", "函数名后面缺少 (");
    if (!check(TokenType::DELIMITER, ")")) {
        parseParamList(functionName.lexeme);
    }
    consume(TokenType::DELIMITER, ")", "函数参数表后面缺少 )");
    consume(TokenType::DELIMITER, ":", "函数返回类型前缺少 :");
    std::string returnType = parseTypeName();
    consume(TokenType::DELIMITER, ";", "函数头后缺少 ;");

    symbolTable_.add({functionName.lexeme, returnType, "function", -1, typeTable_.getTypeSize(returnType)});
    parseVarDeclPart(false, functionName.lexeme);
    parseCompoundStmt();
    consume(TokenType::DELIMITER, ";", "函数体后缺少 ;");
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
        std::string kind = byRef ? "var parameter of " + functionName : "parameter of " + functionName;
        symbolTable_.add({name, typeName, kind, -1, typeTable_.getTypeSize(typeName)});
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
        symbolTable_.add({name, typeName, kind, nextAddress_, size});
        nextAddress_ += size;
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
    parseAssignStmt();
}

// ASSIGN_STMT -> LEFT_VALUE := EXPRESSION
void Parser::parseAssignStmt() {
    std::string leftValue = parseLeftValue();
    consume(TokenType::OPERATOR, ":=", "赋值语句缺少 :=");
    std::string exprValue = parseExpression();
    emit(":=", exprValue, "_", leftValue);
}

// LEFT_VALUE -> id | id . id
std::string Parser::parseLeftValue() {
    Token base = consumeIdentifier("赋值语句左部缺少变量名");
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
std::string Parser::parseExpression() {
    std::string left = parseTerm();
    while (check(TokenType::OPERATOR, "+") || check(TokenType::OPERATOR, "-")) {
        std::string op = peek().lexeme;
        ++current_;
        std::string right = parseTerm();
        std::string temp = newTemp();
        emit(op, left, right, temp);
        left = temp;
    }
    return left;
}

// TERM -> FACTOR { (*|/) FACTOR }
std::string Parser::parseTerm() {
    std::string left = parseFactor();
    while (check(TokenType::OPERATOR, "*") || check(TokenType::OPERATOR, "/")) {
        std::string op = peek().lexeme;
        ++current_;
        std::string right = parseFactor();
        std::string temp = newTemp();
        emit(op, left, right, temp);
        left = temp;
    }
    return left;
}

// FACTOR -> id | id . id | constant | ( EXPRESSION )
std::string Parser::parseFactor() {
    if (check(TokenType::IDENTIFIER)) {
        Token base = consumeIdentifier("表达式中缺少标识符");
        Token field;
        Token* fieldPtr = nullptr;
        if (match(TokenType::DELIMITER, ".")) {
            field = consumeIdentifier(". 后面应是字段名");
            fieldPtr = &field;
        }
        checkLeftValue(base, fieldPtr);
        return fieldPtr == nullptr ? base.lexeme : base.lexeme + "." + field.lexeme;
    }

    if (check(TokenType::CONSTANT)) {
        return tokens_[current_++].lexeme;
    }

    if (match(TokenType::DELIMITER, "(")) {
        std::string value = parseExpression();
        consume(TokenType::DELIMITER, ")", "表达式缺少右括号 )");
        return value;
    }

    errorAtCurrent("表达式因子应为变量、常数或括号表达式");
    return "";
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
    return check(TokenType::IDENTIFIER);
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

std::string Parser::newTemp() {
    std::string name = "t" + std::to_string(++tempIndex_);
    symbolTable_.add({name, "temporary", "temporary", -1, 8});
    return name;
}

void Parser::emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result) {
    result_.quadruples.push_back({op, arg1, arg2, result});
}
