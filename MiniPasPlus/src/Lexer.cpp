// 文件说明：实现词法扫描细节。负责跳过空白注释、识别标识符/常量/运算符并维护词法表。

#include "Lexer.h"
#include <cctype>
#include <set>

// 构造词法分析器并初始化关键字表、界符表。
Lexer::Lexer(const std::string& source) : source_(source) {
    // 关键字表是固定表。
    keywords_ = {
        {1, "program"}, {2, "type"}, {3, "var"}, {4, "function"},
        {5, "begin"}, {6, "end"}, {7, "if"}, {8, "then"},
        {9, "else"}, {10, "while"}, {11, "do"}, {12, "array"},
        {13, "record"}, {14, "of"}, {15, "integer"}, {16, "real"},
        {17, "char"}, {18, "boolean"}
    };

    // 界符表也是固定表。
    // 这些符号本身不会因为用户程序变化而变化。
    delimiters_ = {
        {1, ";"}, {2, ","}, {3, "."}, {4, ":"}, {5, "("},
        {6, ")"}, {7, "["}, {8, "]"}, {9, ".."}
    };
}

// 查看当前字符但不前进。
char Lexer::peek() const {
    return isAtEnd() ? '\0' : source_[pos_];
}

// 查看下一个字符但不前进。
char Lexer::peekNext() const {
    return pos_ + 1 >= static_cast<int>(source_.size()) ? '\0' : source_[pos_ + 1];
}

// 消费一个字符并维护行列位置。
char Lexer::advance() {
    char ch = source_[pos_++];
    if (ch == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return ch;
}

// 判断是否扫描到源代码末尾。
bool Lexer::isAtEnd() const {
    return pos_ >= static_cast<int>(source_.size());
}

// 跳过空白和注释
void Lexer::skipBlankAndComment() {
    while (!isAtEnd()) {
        // 先跳过空白/注释，再看是字母、数字还是符号。
        if (std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
        } else if (peek() == '{') {
            while (!isAtEnd() && peek() != '}') advance();
            if (!isAtEnd()) advance();
        } else {
            break;
        }
    }
}

// 读取标识符或关键字。
Token Lexer::readIdentifierOrKeyword() {
    int startColumn = column_;
    std::string text;
    while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
        text += advance();
    }

    static std::set<std::string> keywordSet = {
        "program", "type", "var", "function", "begin", "end", "if", "then",
        "else", "while", "do", "array", "record", "of", "integer", "real", "char", "boolean"
    };

    if (keywordSet.count(text)) {
        // 单词在关键字集合中：生成 Keyword Token。
        return {TokenType::Keyword, text, line_, startColumn};
    }

    // 非关键字单词当作标识符，加入 I 表（去重）。
    rememberIdentifier(text);
    return {TokenType::Identifier, text, line_, startColumn};
}

// 读取数字常量（按整数处理）。
Token Lexer::readNumber() {
    int startColumn = column_;
    std::string text;
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        text += advance();
    }
    rememberConstant(text, "integer");
    return {TokenType::Constant, text, line_, startColumn};
}

// 读取运算符或界符，支持双字符运算符。
Token Lexer::readOperatorOrDelimiter() {
    int startColumn = column_;
    std::string text;
    text += advance();

    // 先处理双字符符号，避免把它们拆成两个 Token。
    if ((text == ":" && peek() == '=') ||
        (text == "." && peek() == '.') ||
        (text == "<" && peek() == '=') ||
        (text == ">" && peek() == '=') ||
        (text == "!" && peek() == '=')) {
        text += advance();
    }

    // 运算符进入 Operator Token。
    if (text == "+" || text == "-" || text == "*" || text == "/" || text == ":=" ||
        text == "<" || text == ">" || text == "=" || text == "!=" || text == "<=" || text == ">=") {
        return {TokenType::Operator, text, line_, startColumn};
    }

    // 剩下的符号按界符处理。
    return {TokenType::Delimiter, text, line_, startColumn};
}

// 词法分析总入口：把整段文本扫描为 Token 序列。
std::vector<Token> Lexer::tokenize() {
    // Token 表就是在这个循环里生成的。
    // 每次识别出一个单词/数字/符号，就 push_back 到 tokens。
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        // 每次循环都尝试识别“下一个 Token”。
        // 这里的扫描顺序固定：先跳过空白/注释，再看是字母、数字还是符号。
        skipBlankAndComment();
        if (isAtEnd()) break;

        if (std::isalpha(static_cast<unsigned char>(peek()))) {
            // 字母开头：可能是关键字，也可能是标识符。
            tokens.push_back(readIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            // 数字开头：读取常量。先按整数处理。
            tokens.push_back(readNumber());
        } else {
            // 其他字符：按运算符或界符读取。
            tokens.push_back(readOperatorOrDelimiter());
        }
    }
    // 最后补一个 EOF Token，让 Parser 知道源程序已经结束。
    tokens.push_back({TokenType::EndOfFile, "<EOF>", line_, column_});
    return tokens;
}

// 把标识符加入 I 表（去重）。
void Lexer::rememberIdentifier(const std::string& name) {
    // 标识符表不重复存同一个名字。
    for (const auto& old : identifiers_) {
        if (old == name) return;
    }
    identifiers_.push_back(name);
}

// 把常量加入 C 表。
void Lexer::rememberConstant(const std::string& text, const std::string& type) {
    // 常量表保存常量文本和类型。
    constants_.push_back({text, type});
}

// 返回扫描过程中收集到的标识符表。
const std::vector<std::string>& Lexer::identifierTable() const { return identifiers_; }

// 返回扫描过程中收集到的常量表。
const std::vector<ConstantEntry>& Lexer::constantTable() const { return constants_; }

// 返回关键字表（固定表）。
const std::vector<KeywordEntry>& Lexer::keywordTable() const { return keywords_; }

// 返回界符表（固定表）。
const std::vector<DelimiterEntry>& Lexer::delimiterTable() const { return delimiters_; }




