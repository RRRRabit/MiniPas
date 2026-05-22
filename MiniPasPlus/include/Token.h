// 文件说明：定义 Token 类型与词法表项结构，是词法与语法模块共享的数据基础。

#pragma once

#include <string>

using std::string;

// TokenType 表示 Token 类型。
// 词法分析器把源代码切成 Token，Parser 后面只处理 Token，不直接处理字符。
enum class TokenType
{               // 定义固定类别集合
    Keyword,    // program、type、var、function、begin、end、if、while 等
    Identifier, // 用户起的名字：变量名、函数名、类型名
    Constant,   // 数字或字符常量
    Operator,   // + - * / := < > = != <= >=
    Delimiter,  // ; , . : ( ) [ ] ..
    EndOfFile,  // 文件结束
    Error       // 非法字符
};

struct Token
{
    TokenType type;     // Token 类别（关键字、标识符、常量等）
    string lexeme; // Token 原文
    int line = 1;       // 出现行号（从 1 开始）
    int column = 1;     // 出现列号（从 1 开始）
};

struct KeywordEntry
{
    string word; // 关键字文本
};

struct DelimiterEntry
{
    string symbol; // 界符文本
};

struct ConstantEntry
{
    string text; // 常量原文
    string type; // 常量类型
};
