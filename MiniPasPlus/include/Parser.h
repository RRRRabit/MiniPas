#ifndef MINIPASPLUS_PARSER_H
#define MINIPASPLUS_PARSER_H

#include "CompileResult.h"
#include <map>
#include <string>
#include <vector>

// 语法分析器：按照 MiniPas+ 文法进行递归下降分析。
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // 分析 Token 序列，并生成符号表、record 类型表和四元式。
    CompileResult parse();

private:
    friend class RuleTraceGuard;
    struct ExprResult {
        std::string place;
        std::string type;
    };

    std::vector<Token> tokens_;
    std::size_t current_;
    CompileResult result_;
    SymbolTable symbolTable_;
    TypeTable typeTable_;
    int tempIndex_;
    int nextAddress_;
    int currentParamOffset_;
    int currentLocalOffset_;
    int currentFunctionTempSize_;
    std::string programName_;
    std::string currentFunctionName_;
    std::vector<FunctionInfo> functionInfos_;
    std::vector<ParameterInfo> parameterInfos_;
    std::vector<ActivationRecordItem> activationRecords_;
    int parserTraceDepth_ = 0;
    std::vector<std::string> parserRuleStack_;
    std::vector<std::string> parserTraceTree_;
    std::vector<std::string> parserStepLog_;
    std::vector<std::string> parserActionLog_;
    std::vector<std::string> parserStepRuleNames_;
    std::vector<std::string> parserActionRuleNames_;

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
    void addActivationRecord(const std::string& scope, const std::string& name, const std::string& category,
                             const std::string& type, int offset, int size);
    void buildActivationRecords();
    void buildBasicBlocks();
    void traceEnterRule(const std::string& ruleName);
    void traceExitRule(const std::string& ruleName);
    void traceStep(const std::string& message);
    void traceAction(const std::string& message);
    std::string currentRuleContext() const;
};

#endif
