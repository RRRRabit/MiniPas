#ifndef MINIPASPLUS_TOKEN_H
#define MINIPASPLUS_TOKEN_H

#include <string>

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
};

std::string tokenTypeToString(TokenType type);

#endif
