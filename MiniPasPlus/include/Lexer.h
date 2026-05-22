// 文件说明：声明词法分析器接口。输入源代码文本，输出 Token 序列与词法表。

#pragma once

#include <string>
#include <vector>
#include "Token.h"

// Lexer 负责词法分析。
//
// - Token 表：tokenize() 扫描源代码时生成。
// - 关键字表：Lexer 构造函数里一次性建立，内容固定。
// - 界符表：Lexer 构造函数里一次性建立，内容固定。
// - 标识符表：扫描到变量名/函数名/类型名时，由 rememberIdentifier 加入。
// - 常量表：扫描到数字或字符常量时，由 rememberConstant 加入。
class Lexer {
public:
    // 构造词法分析器并保存待扫描的源代码。
    explicit Lexer(const std::string& source);

    // 扫描完整源代码并返回 Token 序列。
    std::vector<Token> tokenize();

    // 返回扫描过程中收集到的标识符表。
    const std::vector<std::string>& identifierTable() const;

    // 返回扫描过程中收集到的常量表。
    const std::vector<ConstantEntry>& constantTable() const;

    // 返回固定关键字表。
    const std::vector<KeywordEntry>& keywordTable() const;

    // 返回固定界符表。
    const std::vector<DelimiterEntry>& delimiterTable() const;

private:
    std::string source_;
    int pos_ = 0;
    int line_ = 1;
    int column_ = 1;

    std::vector<std::string> identifiers_;

    std::vector<ConstantEntry> constants_;

    std::vector<KeywordEntry> keywords_;

    std::vector<DelimiterEntry> delimiters_;

    // 查看当前字符但不移动扫描位置。
    char peek() const;

    // 查看下一个字符但不移动扫描位置。
    char peekNext() const;

    // 读取当前字符并前进一个位置。
    char advance();

    // 判断扫描位置是否已经到达源码末尾。
    bool isAtEnd() const;

    // 跳过空白和注释。注释不进入 Token 表。
    void skipBlankAndComment();

    // 读取一个“字母开头的单词”。
    // 如果这个单词在关键字表里，就是 Keyword；
    // 否则就是 Identifier，并加入标识符表。
    Token readIdentifierOrKeyword();

    // 读取数字常量，并加入常量表。
    Token readNumber();

    // 读取运算符或界符。
    Token readOperatorOrDelimiter();

    // 把标识符加入标识符表。
    void rememberIdentifier(const std::string& name);

    // 把常量文本和类型加入常量表。
    void rememberConstant(const std::string& text, const std::string& type);
};



