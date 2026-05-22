// 文件说明：实现递归下降语法与语义分析主逻辑，生成四元式并划分基本块。

#include "Parser.h"
#include <stdexcept>
#include <algorithm>
#include <set>

// 构造 Parser，接收 Lexer 输出的 Token 序列作为输入。
Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens)
{
}

// 语法/语义分析总入口：驱动递归下降并汇总阶段结果。
CompileResult Parser::parse() {
    result_.tokens = tokens_;
    parseProgram(); // 从 program 规则开始递归下降。
    buildBasicBlocks(); // 四元式生成完后，再按控制流切基本块。

    // 各内部表汇总到 CompileResult，供外层统一使用。
    result_.symbols = symbolTable_.all();
    result_.recordTypes = typeTable_.records();
    result_.arrayTypes = typeTable_.arrays();
    result_.functionTable = functions_;
    result_.parameterTable = parameters_;
    result_.success = true;
    return result_;
}

// 查看当前 Token（不前进）。
const Token& Parser::peek() const
{
    return tokens_[current_];
}

// 判断当前 Token 是否匹配期望类型和可选词素。
bool Parser::check(TokenType type, const std::string& text) const {
    if (peek().type != type)
    {
        return false;
    }
    return text.empty() || peek().lexeme == text;
}

// 尝试匹配：成功则前进一个 Token，失败不报错。
bool Parser::match(TokenType type, const std::string& text) {
    if (!check(type, text))
    {
        return false;
    }
    ++current_; // 前进到下一个 Token
    return true;
}

// 强制匹配：失败立即抛错，用于文法中的“必须出现”符号。
Token Parser::consume(TokenType type, const std::string& text, const std::string& message) {
    if (check(type, text)) {
        Token token = tokens_[current_++];
        result_.parserSteps.push_back({"TOKEN", "匹配 Token: " + token.lexeme});
        return token;
    }
    throw std::runtime_error(message);
}

// 进入某条语法规则，记录递归深度和调用轨迹。
void Parser::enterRule(const std::string& rule) {
    result_.parserTrace.push_back({traceDepth_++, rule, "进入 " + rule});
}

// 离开当前语法规则，回退递归深度。
void Parser::leaveRule() {
    if (traceDepth_ > 0)
    {
        --traceDepth_;
    }
}

// 记录语义动作日志。
void Parser::logAction(const std::string& rule, const std::string& text) {
    result_.parserActions.push_back({rule, text});
}

// PROGRAM -> program id TYPE_PART FUNCTION_PART VAR_PART COMPOUND_STMT .
void Parser::parseProgram() {
    enterRule("PROGRAM");

    // 先读取程序头。
    consume(TokenType::Keyword, "program", "程序必须以 program 开头");
    Token programName = consume(TokenType::Identifier, "", "program 后面缺少程序名");

    symbolTable_.add({programName.lexeme, "program", "program", 0, 0}); // 写入符号表
    emit("program", programName.lexeme, "_", "_"); // 生成四元式

    // 声明区必须在主体 begin...end 之前处理。
    parseTypeDeclPart();
    parseFunctionDeclPart();
    parseVarDeclPart();

    parseCompoundStmt();

    consume(TokenType::Delimiter, ".", "程序结尾缺少 .");
    emit("end", programName.lexeme, "_", "_"); // 生成四元式
    leaveRule();
}

// type 部分：支持 record 和 array。
void Parser::parseTypeDeclPart() {
    // 解析 type 声明区。
    // 如果没有 type 关键字，说明程序没有自定义类型，直接返回。
    enterRule("TYPE_DECL_PART");
    if (!match(TokenType::Keyword, "type")) {
        leaveRule();
        return;
    }

    while (check(TokenType::Identifier)) {
        Token typeName = consume(TokenType::Identifier, "", "type 后面缺少类型名");
        consume(TokenType::Operator, "=", "类型名后缺少 =");

        if (check(TokenType::Keyword, "record")) {
            parseRecordTypeDecl(typeName.lexeme);
        } else if (check(TokenType::Keyword, "array")) { // 检查当前 Token 类型或文本
            parseArrayTypeDecl(typeName.lexeme);
        } else {
            throw std::runtime_error("type 声明当前支持 record 和 array");
        }

        symbolTable_.add({typeName.lexeme, typeName.lexeme, "type", 0, 0}); // 写入符号表
        consume(TokenType::Delimiter, ";", "类型声明后缺少 ;");
    }
    leaveRule();
}

void Parser::parseRecordTypeDecl(const std::string& typeName) {
    consume(TokenType::Keyword, "record", "record 类型缺少 record");

    std::vector<FieldInfo> fields;
    int offset = 0;
    while (!check(TokenType::Keyword, "end")) {
        Token field = consume(TokenType::Identifier, "", "record 字段缺少名字");
        consume(TokenType::Delimiter, ":", "字段名后缺少 :");
        std::string fieldType = parseTypeName();
        consume(TokenType::Delimiter, ";", "字段声明后缺少 ;");
        fields.push_back({field.lexeme, fieldType, offset++});
    }

    consume(TokenType::Keyword, "end", "record 缺少 end");
    typeTable_.addRecord(typeName, fields); // 写入类型表
}

void Parser::parseArrayTypeDecl(const std::string& typeName) {
    consume(TokenType::Keyword, "array", "array 类型缺少 array");
    consume(TokenType::Delimiter, "[", "array 后缺少 [");
    int low = std::stoi(consume(TokenType::Constant, "", "数组下界应为常量").lexeme);
    consume(TokenType::Delimiter, "..", "数组上下界之间缺少 ..");
    int high = std::stoi(consume(TokenType::Constant, "", "数组上界应为常量").lexeme);
    consume(TokenType::Delimiter, "]", "数组后缺少 ]");
    consume(TokenType::Keyword, "of", "array 缺少 of");
    std::string elementType = parseTypeName();

    typeTable_.addArray(typeName, low, high, elementType); // 写入类型表
}

// function 声明：记录函数名、参数、返回类型，并为函数体生成 ret 四元式。
void Parser::parseFunctionDeclPart() {
    while (check(TokenType::Keyword, "function")) {
        parseFunctionDecl();
    }
}

void Parser::parseFunctionDecl() {
    enterRule("FUNCTION_DECL");

    // 先解析函数头：函数名、参数列表、返回类型。
    consume(TokenType::Keyword, "function", "函数声明缺少 function");
    Token name = consume(TokenType::Identifier, "", "function 后缺少函数名");

    consume(TokenType::Delimiter, "(", "函数名后缺少 (");
    if (!check(TokenType::Delimiter, ")"))
    {
        parseParamList(name.lexeme);
    }
    consume(TokenType::Delimiter, ")", "参数列表后缺少 )");

    consume(TokenType::Delimiter, ":", "函数返回类型前缺少 :");
    std::string returnType = parseTypeName();
    consume(TokenType::Delimiter, ";", "函数头后缺少 ;");

    symbolTable_.add({name.lexeme, returnType, "function", 0, 0}); // 写入符号表

    // 函数自己的局部变量和函数体在函数头之后解析。
    parseVarDeclPart(name.lexeme);
    parseCompoundStmt();

    emit("ret", name.lexeme, "_", "_"); // 生成四元式
    consume(TokenType::Delimiter, ";", "函数体后缺少 ;");

    // 参数个数从参数表统计，最后写入函数表。
    int paramCount = 0;
    for (const auto& p : parameters_)
    {
        if (p.functionName == name.lexeme)
        {
            ++paramCount;
        }
    }
    functions_.push_back({name.lexeme, returnType, paramCount});
    leaveRule();
}

void Parser::parseParamList(const std::string& functionName) {
    while (true) {
        bool byRef = match(TokenType::Keyword, "var");
        std::vector<std::string> names;
        names.push_back(consume(TokenType::Identifier, "", "参数缺少名字").lexeme);
        while (match(TokenType::Delimiter, ",")) {
            names.push_back(consume(TokenType::Identifier, "", "逗号后缺少参数名").lexeme);
        }
        consume(TokenType::Delimiter, ":", "参数名后缺少 :");
        std::string type = parseTypeName();

        for (const auto& name : names) {
            std::string mode = byRef ? "vn" : "vf";
            parameters_.push_back({functionName, name, type, mode});
            symbolTable_.add({name, type, "parameter of " + functionName, 0, 0}); // 写入符号表
        }

        if (!match(TokenType::Delimiter, ";"))
        {
            break;
        }
    }
}

// var 声明：支持全局变量和函数局部变量。
void Parser::parseVarDeclPart(const std::string& scope) {
    // 解析 var 声明区。
    // scope 为空：全局变量；
    // scope 非空：函数局部变量。
    enterRule("VAR_DECL_PART");
    if (!match(TokenType::Keyword, "var")) {
        leaveRule();
        return;
    }

    while (check(TokenType::Identifier)) {
        std::vector<std::string> names;
        names.push_back(consume(TokenType::Identifier, "", "变量声明缺少名字").lexeme);
        while (match(TokenType::Delimiter, ",")) {
            names.push_back(consume(TokenType::Identifier, "", "逗号后缺少变量名").lexeme);
        }

        consume(TokenType::Delimiter, ":", "变量名后缺少 :");
        std::string type = parseTypeName();
        consume(TokenType::Delimiter, ";", "变量声明后缺少 ;");

        for (const auto& name : names) {
            std::string kind = scope.empty() ? "variable" : "local variable of " + scope;
            if (typeTable_.findRecord(type))
            {
                kind = "record " + kind;
            }
            if (typeTable_.findArray(type))
            {
                kind = "array " + kind;
            }
            symbolTable_.add({name, type, kind, 0, 0}); // 写入符号表
        }
    }
    leaveRule();
}

// 解析复合语句块 begin ... end。
void Parser::parseCompoundStmt() {
    consume(TokenType::Keyword, "begin", "复合语句缺少 begin");
    parseStmtList();
    consume(TokenType::Keyword, "end", "复合语句缺少 end");
}

// 解析语句序列（分号分隔）。
void Parser::parseStmtList() {
    if (check(TokenType::Keyword, "end"))
    {
        return;
    }
    parseStmt();
    while (match(TokenType::Delimiter, ";")) {
        if (check(TokenType::Keyword, "end") || check(TokenType::Keyword, "else"))
        {
            break;
        }
        parseStmt();
    }
}

// 语句分发器：按首 Token 决定进入哪类语句解析。
void Parser::parseStmt() {
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
}

// 解析赋值语句，并执行左值/类型语义检查。
void Parser::parseAssignStmt() {
    std::string left = parseLeftValue();
    consume(TokenType::Operator, ":=", "赋值语句缺少 :=");

    // 右侧可能是常量、变量、函数调用或运算表达式。
    ExprResult right = parseExpression();
    std::string leftType = "integer";

    // 根据左值形态找到真正需要检查的目标类型。
    if (left.find('[') != std::string::npos) {
        std::string baseName = left.substr(0, left.find('['));
        const Symbol& arraySymbol = requireVariableDeclared(baseName);
        const ArrayType* arrayType = typeTable_.findArray(arraySymbol.typeName);
        if (arrayType != nullptr)
        {
            leftType = arrayType->elementType;
        }
    } else if (left.find('.') != std::string::npos) {
        std::string baseName = left.substr(0, left.find('.'));
        std::string fieldName = left.substr(left.find('.') + 1);
        leftType = checkRecordFieldAndGetType(baseName, fieldName);
    } else {
        leftType = requireVariableDeclared(left).typeName;
    }

    // 类型不兼容时直接报告语义错误，不继续生成四元式。
    if (!canAssign(leftType, right.type)) {
        throw std::runtime_error("赋值类型不兼容: " + leftType + " := " + right.type);
    }

    emit(":=", right.place, "_", left); // 生成四元式
}

// 解析函数调用语句（无返回值上下文）。
void Parser::parseCallStmt() {
    Token functionName = consume(TokenType::Identifier, "", "函数调用缺少函数名");
    parseFunctionCall(functionName);
}

// 解析 if-then-else，并生成结构化控制流四元式。
void Parser::parseIfStmt() {
    // if 语句会生成结构化控制流四元式：
    // if ... then ... else ...
    // 对应标记：if / el / ie
    enterRule("IF_STMT");
    consume(TokenType::Keyword, "if", "if 语句缺少 if");
    ExprResult cond = parseCondition();
    emit("if", cond.place, "_", "_"); // 生成四元式

    consume(TokenType::Keyword, "then", "if 条件后缺少 then");
    parseStmt();

    if (match(TokenType::Keyword, "else")) {
        emit("el", "_", "_", "_"); // 生成四元式
        parseStmt();
    }
    emit("ie", "_", "_", "_"); // 生成四元式
    leaveRule();
}

// 解析 while-do 循环，并生成循环控制四元式。
void Parser::parseWhileStmt() {
    // while 循环对应标记：
    // wh（循环开始） / do（条件跳转） / we（循环结束）
    enterRule("WHILE_STMT");
    consume(TokenType::Keyword, "while", "while 语句缺少 while");
    emit("wh", "_", "_", "_"); // 生成四元式

    ExprResult cond = parseCondition();
    emit("do", cond.place, "_", "_"); // 生成四元式

    consume(TokenType::Keyword, "do", "while 条件后缺少 do");
    parseStmt();
    emit("we", "_", "_", "_"); // 生成四元式
    leaveRule();
}

// 解析左值：变量、数组元素、记录字段。
std::string Parser::parseLeftValue() {
    Token base = consume(TokenType::Identifier, "", "左值缺少变量名");
    requireVariableDeclared(base.lexeme);

    if (match(TokenType::Delimiter, "[")) {
        ExprResult index = parseExpression();
        consume(TokenType::Delimiter, "]", "数组下标后缺少 ]");
        checkArrayIndexAndGetElementType(base.lexeme, index);
        return base.lexeme + "[" + index.place + "]";
    }

    if (match(TokenType::Delimiter, ".")) {
        Token field = consume(TokenType::Identifier, "", ". 后缺少字段名");
        checkRecordFieldAndGetType(base.lexeme, field.lexeme);
        return base.lexeme + "." + field.lexeme;
    }

    return base.lexeme;
}

// 解析条件表达式，生成关系运算四元式。
Parser::ExprResult Parser::parseCondition() {
    ExprResult left = parseExpression();
    std::string op = consume(TokenType::Operator, "", "条件缺少关系运算符").lexeme;
    ExprResult right = parseExpression();
    std::string temp = newTemp("boolean");
    emit(op, left.place, right.place, temp); // 生成四元式
    return {temp, "boolean"};
}

// 解析加减层表达式。
Parser::ExprResult Parser::parseExpression() {
    ExprResult left = parseTerm();

    // 先让 parseTerm 处理乘除，再在这里处理加减。
    while (check(TokenType::Operator, "+") || check(TokenType::Operator, "-")) {
        std::string op = peek().lexeme;
        ++current_; // 前进到下一个 Token
        ExprResult right = parseTerm();
        std::string temp = newTemp(left.type == "real" || right.type == "real" ? "real" : "integer");
        emit(op, left.place, right.place, temp); // 生成四元式
        left = {temp, "integer"};
    }
    return left;
}

// 解析乘除层表达式。
Parser::ExprResult Parser::parseTerm() {
    ExprResult left = parseFactor();

    // 连续乘除会从左到右生成临时结果。
    while (check(TokenType::Operator, "*") || check(TokenType::Operator, "/")) {
        std::string op = peek().lexeme;
        ++current_; // 前进到下一个 Token
        ExprResult right = parseFactor();
        std::string temp = newTemp(left.type == "real" || right.type == "real" ? "real" : "integer");
        emit(op, left.place, right.place, temp); // 生成四元式
        left = {temp, "integer"};
    }
    return left;
}

// 解析表达式因子：常量、标识符、调用、括号等。
Parser::ExprResult Parser::parseFactor() {
    if (check(TokenType::Constant)) {
        Token c = consume(TokenType::Constant, "", "缺少常量");
        return {c.lexeme, c.lexeme.find('.') == std::string::npos ? "integer" : "real"};
    }

    if (check(TokenType::Identifier)) {
        Token name = consume(TokenType::Identifier, "", "缺少标识符");

        // 标识符后面接括号时，它是函数调用。
        if (check(TokenType::Delimiter, "("))
        {
            return parseFunctionCall(name);
        }

        std::string place = name.lexeme;
        std::string type = requireVariableDeclared(name.lexeme).typeName;

        // 标识符后面接下标时，它按数组元素处理。
        if (match(TokenType::Delimiter, "[")) {
            ExprResult index = parseExpression();
            consume(TokenType::Delimiter, "]", "数组下标后缺少 ]");
            type = checkArrayIndexAndGetElementType(name.lexeme, index);
            place = name.lexeme + "[" + index.place + "]";
        }

        // 标识符后面接点号时，它按记录字段处理。
        if (match(TokenType::Delimiter, ".")) {
            Token field = consume(TokenType::Identifier, "", ". 后缺少字段名");
            type = checkRecordFieldAndGetType(name.lexeme, field.lexeme);
            place += "." + field.lexeme;
        }
        return {place, type};
    }

    if (match(TokenType::Delimiter, "(")) {
        // 括号内部仍然按完整表达式解析。
        ExprResult value = parseExpression();
        consume(TokenType::Delimiter, ")", "缺少 )");
        return value;
    }

    throw std::runtime_error("表达式因子错误");
}

// 解析函数调用表达式，检查实参与形参匹配。
Parser::ExprResult Parser::parseFunctionCall(const Token& functionName) {
    const Symbol& functionSymbol = requireVariableDeclared(functionName.lexeme);
    if (functionSymbol.kind != "function") {
        throw std::runtime_error(functionName.lexeme + " 不是函数，不能调用");
    }

    consume(TokenType::Delimiter, "(", "函数调用缺少 (");
    std::vector<ExprResult> args;
    if (!check(TokenType::Delimiter, ")")) {
        args.push_back(parseExpression());
        while (match(TokenType::Delimiter, ","))
        {
            args.push_back(parseExpression());
        }
    }
    consume(TokenType::Delimiter, ")", "函数调用缺少 )");

    checkArgumentList(functionName.lexeme, args);
    for (const auto& arg : args)
    {
        // const auto&：只读引用，避免拷贝参数表达式结果。
        emit("param", arg.place, "vf", "_");
    }

    std::string result = newTemp(functionSymbol.typeName);
    emit("call", functionName.lexeme, std::to_string(args.size()), result);
    return {result, functionSymbol.typeName};
}

// 解析类型名：基础类型或用户自定义类型名。
std::string Parser::parseTypeName() {
    if (check(TokenType::Keyword, "integer") || check(TokenType::Keyword, "real") ||
        check(TokenType::Keyword, "char") || check(TokenType::Keyword, "boolean")) { // 检查当前 Token 类型或文本
        return tokens_[current_++].lexeme;
    }
    return consume(TokenType::Identifier, "", "缺少类型名").lexeme;
}

// 生成新的临时变量名并加入符号表。
std::string Parser::newTemp(const std::string& type) {
    std::string name = "t" + std::to_string(++tempIndex_);
    symbolTable_.add({name, type, "temporary", 0, 0}); // 写入符号表
    return name;
}

// 生成一条四元式并写入中间代码序列。
void Parser::emit(const std::string& op, const std::string& a, const std::string& b, const std::string& r) {
    logAction("EMIT", "生成四元式: (" + op + ", " + a + ", " + b + ", " + r + ")");
    result_.quadruples.push_back({op, a, b, r});
}

// 按控制流边界划分基本块。
void Parser::buildBasicBlocks() {
    int start = 0;
    int id = 1;

    // 扫描四元式，遇到控制流边界就结束当前块。
    for (int i = 0; i < static_cast<int>(result_.quadruples.size()); ++i) {
        const std::string& op = result_.quadruples[i].op;
        if ((op == "if" || op == "do" || op == "el" || op == "we") && i > start) {
            result_.basicBlocks.push_back({id++, start, i - 1});
            start = i;
        }
    }

    // 最后一段没有边界触发，需要单独补进基本块表。
    if (start < static_cast<int>(result_.quadruples.size())) {
        result_.basicBlocks.push_back({id, start, static_cast<int>(result_.quadruples.size()) - 1});
    }
}

// 要求标识符已声明，否则抛出语义错误。
const Symbol& Parser::requireVariableDeclared(const std::string& name) const {
    const Symbol* symbol = symbolTable_.find(name);
    if (symbol == nullptr) {
        throw std::runtime_error("标识符未声明: " + name);
    }
    return *symbol;
}

// 赋值兼容性判断（仅支持同类型或 integer 到 real）。
bool Parser::canAssign(const std::string& targetType, const std::string& sourceType) const {
    if (targetType == sourceType)
    {
        return true;
    }
    if (targetType == "real" && sourceType == "integer")
    {
        return true;
    }
    return false;
}

// 检查数组下标类型并返回元素类型。
std::string Parser::checkArrayIndexAndGetElementType(const std::string& arrayName, const ExprResult& index) const {
    if (index.type != "integer") {
        throw std::runtime_error("数组下标必须是 integer: " + arrayName);
    }

    const Symbol& symbol = requireVariableDeclared(arrayName);
    const ArrayType* arrayType = typeTable_.findArray(symbol.typeName);
    if (arrayType == nullptr) {
        throw std::runtime_error(arrayName + " 不是数组，不能使用 []");
    }

    return arrayType->elementType;
}

// 检查记录字段合法性并返回字段类型。
std::string Parser::checkRecordFieldAndGetType(const std::string& recordVarName, const std::string& fieldName) const {
    const Symbol& symbol = requireVariableDeclared(recordVarName);
    const FieldInfo* field = typeTable_.findField(symbol.typeName, fieldName);
    if (field == nullptr) {
        throw std::runtime_error("record 字段不存在: " + recordVarName + "." + fieldName);
    }

    return field->type;
}

// 找到某函数全部形参信息。
std::vector<ParameterInfo> Parser::findFunctionParams(const std::string& functionName) const {
    std::vector<ParameterInfo> result;
    for (const auto& parameter : parameters_) {
        if (parameter.functionName == functionName) {
            result.push_back(parameter);
        }
    }
    return result;
}

// 校验实参与形参数量和类型匹配性。
void Parser::checkArgumentList(const std::string& functionName, const std::vector<ExprResult>& args) const {
    std::vector<ParameterInfo> params = findFunctionParams(functionName);
    if (params.size() != args.size()) {
        throw std::runtime_error("函数实参数量不匹配: " + functionName);
    }

    for (int i = 0; i < static_cast<int>(args.size()); ++i) {
        if (!canAssign(params[i].type, args[i].type)) {
            throw std::runtime_error("函数参数类型不匹配: " + functionName);
        }
    }
}




