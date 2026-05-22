// 文件说明：声明递归下降语法/语义分析器接口。负责生成符号表、类型表、四元式及基本块。

#pragma once

#include <string>
#include <vector>
#include "CompileResult.h"
#include "SymbolTable.h"
#include "TypeTable.h"

// Parser 是递归下降语法分析器。
//
// 每个 parseXXX 函数对应一个语法结构。
class Parser {
public:
    // 构造语法分析器并保存 Lexer 生成的 Token 序列。
    explicit Parser(const std::vector<Token>& tokens);

    // 启动递归下降分析并返回汇总结果。
    CompileResult parse();

private:
    struct ExprResult {
        std::string place;
        std::string type;
    };

    std::vector<Token> tokens_;
    int current_ = 0;
    int tempIndex_ = 0;

    CompileResult result_;
    SymbolTable symbolTable_;
    TypeTable typeTable_;
    std::vector<FunctionInfo> functions_;
    std::vector<ParameterInfo> parameters_;
    int traceDepth_ = 0;

    // 查看当前 Token 但不前进。
    const Token& peek() const;

    // 判断当前 Token 是否符合期望类型和文本。
    bool check(TokenType type, const std::string& text = "") const;

    // 当前 Token 符合要求时前进一个位置。
    bool match(TokenType type, const std::string& text = "");

    // 强制读取指定 Token，不符合时报告错误。
    Token consume(TokenType type, const std::string& text, const std::string& message);

    // 记录进入一条语法规则。
    void enterRule(const std::string& rule);

    // 记录离开当前语法规则。
    void leaveRule();

    // 记录语义动作文本。
    void logAction(const std::string& rule, const std::string& text);

    // 解析整个程序结构。
    void parseProgram();

    // 解析 type 声明区。
    void parseTypeDeclPart();

    // 解析 record 类型声明。
    void parseRecordTypeDecl(const std::string& typeName);

    // 解析 array 类型声明。
    void parseArrayTypeDecl(const std::string& typeName);

    // 解析函数声明区。
    void parseFunctionDeclPart();

    // 解析单个函数声明。
    void parseFunctionDecl();

    // 解析函数参数列表。
    void parseParamList(const std::string& functionName);

    // 解析变量声明区。
    void parseVarDeclPart(const std::string& scope = "");

    // 解析 begin 到 end 包围的复合语句。
    void parseCompoundStmt();

    // 解析一组由分号分隔的语句。
    void parseStmtList();

    // 根据当前 Token 分派到具体语句解析函数。
    void parseStmt();

    // 解析赋值语句。
    void parseAssignStmt();

    // 解析函数调用语句。
    void parseCallStmt();

    // 解析 if 语句。
    void parseIfStmt();

    // 解析 while 语句。
    void parseWhileStmt();

    // 解析赋值号左侧的变量、数组元素或记录字段。
    std::string parseLeftValue();

    // 解析关系条件表达式。
    ExprResult parseCondition();

    // 解析加减层表达式。
    ExprResult parseExpression();

    // 解析乘除层表达式。
    ExprResult parseTerm();

    // 解析表达式中的最小组成部分。
    ExprResult parseFactor();

    // 解析函数调用表达式并返回调用结果位置。
    ExprResult parseFunctionCall(const Token& functionName);

    // 解析基础类型名或自定义类型名。
    std::string parseTypeName();

    // 创建一个新的临时变量名。
    std::string newTemp(const std::string& type);

    // 生成一条四元式。
    void emit(const std::string& op, const std::string& a, const std::string& b, const std::string& r);

    // 根据控制流边界划分基本块。
    void buildBasicBlocks();

    // 下面这些函数专门负责语义检查。
    // 学生讲到“变量未声明、赋值类型不兼容、数组下标、record 字段、函数参数”时，
    // 可以直接指向这些函数。
    // 要求标识符已经声明。
    const Symbol& requireVariableDeclared(const std::string& name) const;

    // 判断赋值时源类型能否放入目标类型。
    bool canAssign(const std::string& targetType, const std::string& sourceType) const;

    // 检查数组下标并返回数组元素类型。
    std::string checkArrayIndexAndGetElementType(const std::string& arrayName, const ExprResult& index) const;

    // 检查记录字段并返回字段类型。
    std::string checkRecordFieldAndGetType(const std::string& recordVarName, const std::string& fieldName) const;

    // 查找某个函数的全部形参。
    std::vector<ParameterInfo> findFunctionParams(const std::string& functionName) const;

    // 检查函数调用实参和形参是否匹配。
    void checkArgumentList(const std::string& functionName, const std::vector<ExprResult>& args) const;
};



