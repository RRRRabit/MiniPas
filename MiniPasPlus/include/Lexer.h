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

    // 常数表：保存扫描过程中出现过的常数字面量。
    const std::vector<std::string>& getConstantTable() const;

    // 结构化常数表：保存常数字面量、类型和值。
    const std::vector<ConstantEntry>& getConstantEntries() const;

    // 固定关键字表和界符表。
    const std::vector<KeywordEntry>& getKeywordTable() const;
    const std::vector<DelimiterEntry>& getDelimiterTable() const;

private:
    std::string source_;
    std::size_t pos_;
    int line_;
    int column_;
    std::vector<std::string> identifiers_;
    std::vector<std::string> constants_;
    std::vector<ConstantEntry> constantEntries_;
    std::vector<KeywordEntry> keywordTable_;
    std::vector<DelimiterEntry> delimiterTable_;

    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();
    bool skipComment();
    int addIdentifier(const std::string& value);
    int addConstant(const std::string& text, const std::string& type, double numberValue);
    int findKeywordIndex(const std::string& word) const;
    int findDelimiterIndex(const std::string& symbol) const;
    std::string toLower(const std::string& text) const;
};

#endif
