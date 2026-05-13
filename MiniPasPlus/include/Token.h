#ifndef MINIPASPLUS_TOKEN_H
#define MINIPASPLUS_TOKEN_H

#include <string>
#include <vector>

// Token 类型：描述词法分析识别出的单词类别。
enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    CONSTANT,
    DELIMITER,
    OPERATOR,
    END_OF_FILE,
    ERROR
};

// Token 结构体：保存单词类别、原始文本以及所在行列。
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    std::string code; // 课程示例中的 k/i/c/p/e，例如 (k,1)
    int value = -1;   // 在对应表中的编号，从 1 开始；EOF 和 ERROR 为 -1
};

// 关键字表项：用于展示课程设计中的 K 表。
struct KeywordEntry {
    int index;
    std::string word;
};

// 界符表项：用于展示课程设计中的 P 表，运算符也归入此表。
struct DelimiterEntry {
    int index;
    std::string symbol;
};

// 常数表项：保存常数字面量、类型和值。
struct ConstantEntry {
    int index;
    std::string text;
    std::string type;
    double numberValue = 0.0;
};

std::string tokenTypeToString(TokenType type);
std::string tokenCodeToString(const Token& token);

#endif
