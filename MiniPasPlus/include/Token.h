#ifndef MINIPASPLUS_TOKEN_H
#define MINIPASPLUS_TOKEN_H

#include <string>
#include <vector>

// Token 类型：描述词法分析识别出的“单词类别”。
//
// 源代码一开始只是普通字符串，例如：
//     a := 10;
//
// Lexer 扫描后会把它切成多个 Token：
//     IDENTIFIER("a")
//     OPERATOR(":=")
//     CONSTANT("10")
//     DELIMITER(";")
//
// Parser 后面不再直接看字符，而是看这些 Token。
enum class TokenType {
    KEYWORD,     // 关键字：program、var、begin、end、if、while 等
    IDENTIFIER,  // 标识符：变量名、函数名、类型名，例如 a、sum、person
    CONSTANT,    // 常量：123、3.14、'a'
    DELIMITER,   // 界符：; , ( ) [ ] . 等
    OPERATOR,    // 运算符：+ - * / := < > = != 等
    END_OF_FILE, // 文件结束标记，Parser 用它判断程序是否读完
    ERROR        // 词法错误，例如非法字符或写坏的常量
};

// Token 结构体：保存一个词法单元的完整信息。
//
// 对初学者来说，重点看前四个字段：
// type   ：这个 Token 是关键字、标识符、常量还是运算符。
// lexeme ：源代码里的原始文本，比如 "while"、"a"、":="。
// line   ：第几行，报错定位用。
// column ：第几列，报错定位用。
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    // code/value 是为了贴近课程设计里的二元式表示。
    // 例如关键字表里的第 1 个关键字可以显示为 (k,1)，标识符表里的第 2 个标识符显示为 (i,2)。
    std::string code;
    int value = -1;
};

// 关键字表项：用于展示课程设计中的 K 表。
// index 是编号，word 是关键字文本。
struct KeywordEntry {
    int index;
    std::string word;
};

// 界符表项：用于展示课程设计中的 P 表。
// 本项目里一些运算符也放在这个表中展示。
struct DelimiterEntry {
    int index;
    std::string symbol;
};

// 常数表项：保存常数字面量、类型和值。
//
// text        ：源代码中的原样文本，例如 "12"、"3.5"、"'a'"。
// type        ：integer / real / char。
// numberValue ：数值形式。char 不依赖这个字段运行，所以默认 0。
struct ConstantEntry {
    int index;
    std::string text;
    std::string type;
    double numberValue = 0.0;
};

std::string tokenTypeToString(TokenType type);
std::string tokenCodeToString(const Token& token);

#endif
