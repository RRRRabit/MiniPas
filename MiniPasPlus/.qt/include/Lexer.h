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
    // 源程序全文，以及当前扫描位置。
    std::string source_;
    std::size_t pos_;

    // 当前行列号，用于报错定位。
    int line_;
    int column_;

    // 三张词法分析相关表。
    // identifiers_ 只存名字字符串；
    // constants_ 只存常量文本；
    // constantEntries_ 额外存常量类型和值，方便 GUI 展示。
    std::vector<std::string> identifiers_;
    std::vector<std::string> constants_;
    std::vector<ConstantEntry> constantEntries_;

    // 固定表：关键字表和界符表在 Lexer 构造时初始化。
    std::vector<KeywordEntry> keywordTable_;
    std::vector<DelimiterEntry> delimiterTable_;

    // 查看当前字符，但不移动 pos_。
    char peek() const;

    // 查看下一个字符，用于识别 :=、<=、>= 这种双字符界符。
    char peekNext() const;

    // 读取当前字符，并把 pos_/line_/column_ 向前推进。
    char advance();

    // 是否已经扫描到源程序末尾。
    bool isAtEnd() const;

    // 跳过空格、换行、制表符。
    void skipWhitespace();

    // 跳过 { ... } 注释，返回是否真的跳过了注释。
    bool skipComment();

    // 往标识符表/常量表中加入新项。
    // 如果已经存在，就返回原来的下标，避免表里重复存同一个名字。
    int addIdentifier(const std::string& value);
    int addConstant(const std::string& text, const std::string& type, double numberValue);

    // 查表函数。找不到时返回 -1。
    int findKeywordIndex(const std::string& word) const;
    int findDelimiterIndex(const std::string& symbol) const;

    // Pascal 关键字大小写不敏感，所以识别关键字前统一转小写。
    std::string toLower(const std::string& text) const;
};

#endif
