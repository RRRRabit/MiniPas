#ifndef MINIPASPLUS_PARSER_H
#define MINIPASPLUS_PARSER_H

#include "CompileResult.h"
#include <map>
#include <string>
#include <vector>

// 语法分析器：按照 MiniPas+ 文法进行递归下降分析。
//
// “递归下降”可以理解成：
// 文法里每一个非终结符，都写成一个 parseXXX 函数。
// 例如 PROGRAM 规则对应 parseProgram，表达式规则对应 parseExpression。
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // 分析 Token 序列，并生成符号表、类型表、函数表、四元式、基本块等结果。
    CompileResult parse();

private:
    // 表达式分析结果。
    //
    // place 表示表达式的值“存在哪里”：
    // - 变量名：a
    // - 常量：1
    // - 临时变量：t1
    //
    // type 表示表达式类型：
    // - integer
    // - real
    // - char
    // - boolean
    struct ExprResult {
        std::string place;
        std::string type;
    };

    // Token 序列和当前位置。
    // current_ 指向“下一个准备读取”的 Token。
    std::vector<Token> tokens_;
    std::size_t current_;

    // result_ 是 Parser 阶段自己的结果容器。
    // parse() 结束后会把它返回给 CompilerFacade。
    CompileResult result_;

    // 语义分析需要的表：
    // symbolTable_ 记录变量、函数、类型名等标识符。
    // typeTable_ 记录 record / array 类型。
    SymbolTable symbolTable_;
    TypeTable typeTable_;

    // 临时变量和地址分配用的计数器。
    // tempIndex_ 用来生成 t1、t2、t3。
    // nextAddress_ 用来给全局变量/临时变量分配相对地址。
    int tempIndex_;
    int nextAddress_;

    // 函数内部参数、局部变量和临时变量的偏移统计。
    int currentParamOffset_;
    int currentLocalOffset_;
    int currentFunctionTempSize_;

    // 当前程序名和当前正在分析的函数名。
    // 如果 currentFunctionName_ 为空，说明正在分析主程序。
    std::string programName_;
    std::string currentFunctionName_;

    // 函数表和参数表。
    // 函数调用检查、VM 指令生成都会用到它们。
    std::vector<FunctionInfo> functionInfos_;
    std::vector<ParameterInfo> parameterInfos_;

    // 递归分析过程展示用状态。
    // traceDepth_ 表示当前 parseXXX 调用深度，只影响 GUI 调用树缩进。
    int traceDepth_;
    int traceNodeId_;
    std::vector<int> traceNodeStack_;

    // TraceScope 是一个小型 RAII 工具：
    // 构造时记录“进入某个语法子程序”，析构时自动降低深度。
    // 这样 parseXXX 函数中途 return 或抛异常时，深度也能恢复。
    struct TraceScope {
        Parser& parser;
        std::string rule;
        TraceScope(Parser& parser, std::string rule);
        ~TraceScope();
    };

    // 下面三个函数只记录 GUI 展示日志，不影响真正的语法分析判断。
    void traceEnter(const std::string& rule);
    void traceExit();
    void logParserStep(const std::string& rule, const std::string& text);
    void logParserAction(const std::string& rule, const std::string& text);

    // 每个 parse 函数对应一个非终结符。
    void parseProgram();
    void parseDeclPart();
    void parseTypeDeclPart();
    void parseTypeDeclList();
    void parseTypeDecl();
    void parseTypeSpec(const std::string& typeName);
    void parseRecordTypeDecl(const std::string& typeName);
    void parseArrayTypeDecl(const std::string& typeName);
    std::vector<FieldInfo> parseFieldDeclList();
    FieldInfo parseFieldDecl();
    std::string parseBasicType();
    void parseFunctionDeclPart();
    void parseFunctionDecl();
    void parseParamList(const std::string& functionName);
    void parseParam(const std::string& functionName);
    void parseVarDeclPart(bool required = true, const std::string& scope = "");
    void parseVarDeclList(const std::string& scope = "");
    void parseVarDecl(const std::string& scope = "");
    std::vector<std::string> parseIdList();
    std::string parseTypeName();
    void parseCompoundStmt();
    void parseStmtList();
    void parseStmt();
    void parseAssignStmt();
    void parseCallStmt();
    void parseWhileStmt();
    void parseIfStmt();
    ExprResult parseCondition();
    std::string parseLeftValue();
    ExprResult parseFunctionCall(const Token& functionToken);
    ExprResult parseExpression();
    ExprResult parseTerm();
    ExprResult parseFactor();

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TokenType type, const std::string& lexeme = "") const;
    bool match(TokenType type, const std::string& lexeme = "");
    Token consume(TokenType type, const std::string& lexeme, const std::string& reason);
    Token consumeIdentifier(const std::string& reason);
    void errorAtCurrent(const std::string& reason) const;
    void errorAtToken(const Token& token, const std::string& reason) const;
    bool isBasicTypeToken() const;
    bool isStatementStart() const;
    void checkVariableDeclared(const Token& nameToken) const;
    void checkLeftValue(const Token& baseToken, const Token* fieldToken) const;
    std::string resolveArrayElementType(const Token& baseToken, const ExprResult& indexExpr) const;
    std::string resolveLValueType(const std::string& leftValue) const;
    std::string mergeNumericType(const std::string& leftType, const std::string& rightType, const Token& opToken) const;
    bool canAssign(const std::string& targetType, const std::string& sourceType) const;
    std::string newTemp(const std::string& typeName);
    void emit(const std::string& op, const std::string& arg1, const std::string& arg2, const std::string& result);
    void buildBasicBlocks();
};

#endif
