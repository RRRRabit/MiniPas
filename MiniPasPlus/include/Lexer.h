#ifndef MINIPASPLUS_LEXER_H
#define MINIPASPLUS_LEXER_H

#include "Token.h"
#include <string>
#include <vector>

// 词法分析器：把源程序字符串切分成 Token 序列。
class Lexer {
public:
    explicit Lexer(std::string source);

    // 执行词法分析，返回完整 Token 序列。
    std::vector<Token> tokenize();

    // 标识符表：保存扫描过程中出现过的标识符。
    const std::vector<std::string>& getIdentifierTable() const;

    // 常数表：保存扫描过程中出现过的整数、实数常量。
    const std::vector<std::string>& getConstantTable() const;

private:
    std::string source_;
    std::size_t pos_;
    int line_;
    int column_;
    std::vector<std::string> identifiers_;
    std::vector<std::string> constants_;

    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();
    void addUnique(std::vector<std::string>& table, const std::string& value);
};

#endif
