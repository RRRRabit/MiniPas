#include "Lexer.h"
#include <cctype>
#include <cstdlib>
#include <utility>

Lexer::Lexer(std::string source)
: source_(std::move(source)), pos_(0), line_(1), column_(1)
{
    // 关键字表。
    //
    // Lexer 读到一个由字母组成的单词后，会先把它转成小写，再来这里查表。
    // 查到了就是 KEYWORD；查不到就是 IDENTIFIER。
    keywordTable_ = {
        {1, "program"}, {2, "var"}, {3, "integer"}, {4, "real"},
        {5, "char"}, {6, "begin"}, {7, "end"}, {8, "type"},
        {9, "record"}, {10, "array"}, {11, "of"}, {12, "function"},
        {13, "boolean"}, {14, "while"}, {15, "do"},
        {16, "if"}, {17, "then"}, {18, "else"}
    };

    // 界符/运算符表。
    //
    // 这里的编号用于生成课程设计中的二元式，例如 (p,4) 表示 ":="。
    // 注意：有些符号在 TokenType 上归为 OPERATOR，但展示编号仍从 delimiterTable_ 查。
    delimiterTable_ = {
        {1, ","}, {2, ":"}, {3, ";"}, {4, ":="}, {5, "*"},
        {6, "/"}, {7, "+"}, {8, "-"}, {9, "."}, {10, "("},
        {11, ")"}, {12, "="}, {13, "["}, {14, "]"}, {15, ".."},
        {16, "<"}, {17, ">"}, {18, "!="}
    };
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    // 词法分析主循环：从 source_ 的第 0 个字符开始，一直扫到末尾。
    //
    // 每次循环只识别一个“最小单词”：
    // - 一个关键字/标识符
    // - 一个常量
    // - 一个运算符或界符
    // - 一个错误 Token
    while (!isAtEnd())
    {
        // 空格、换行、Tab 对语法没有意义，先跳过。
        skipWhitespace();
        if (isAtEnd())
        {
            break;
        }

        // 注释也不进入 Token 序列。
        // 如果当前位置确实是注释，skipComment 返回 true，然后 continue 重新开始扫描。
        if (skipComment())
        {
            continue;
        }

        // 记录当前 Token 开始位置。即使后面 advance() 改变 line/column，
        // 这个 Token 的报错位置仍然应该指向起始字符。
        int startLine = line_;
        int startColumn = column_;
        char ch = peek();

        // =========================
        // 分支 1：字母开头 -> 关键字或标识符
        // =========================
        //
        // MiniPas 里变量名/函数名/类型名都按“字母开头，后面接字母或数字”处理。
        if (std::isalpha(static_cast<unsigned char>(ch)))
        {
            std::string word;
            while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek())))
            {
                word.push_back(advance());
            }

            // MiniPas 关键字不区分大小写，所以 PROGRAM 和 program 都识别为 program。
            // 但普通标识符保留原拼写，例如 MyVar 仍存 MyVar。
            std::string lowerWord = toLower(word);
            int keywordIndex = findKeywordIndex(lowerWord);
            if (keywordIndex > 0)
            {
                // 查到关键字表：生成 KEYWORD Token。
                tokens.push_back({TokenType::KEYWORD, lowerWord, startLine, startColumn, "k", keywordIndex});
            }
            else
            {
                // 没查到关键字表：当作用户自定义标识符，加入 I 表。
                int idIndex = addIdentifier(word);
                tokens.push_back({TokenType::IDENTIFIER, word, startLine, startColumn, "i", idIndex});
            }
        }
        // =========================
        // 分支 2：数字开头 -> 整数常量或实数常量
        // =========================
        else if (std::isdigit(static_cast<unsigned char>(ch)))
        {
            std::string number;
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            {
                number.push_back(advance());
            }

            std::string type = "integer";
            // 如果数字后面是单个 "."，并且 "." 后面还是数字，就识别为实数。
            // 这里必须排除 ".."，因为 array[1..5] 里的 ".." 是数组上下界符号。
            if (!isAtEnd() && peek() == '.' && peekNext() != '.' && std::isdigit(static_cast<unsigned char>(peekNext())))
            {
                type = "real";
                number.push_back(advance());
                while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                {
                    number.push_back(advance());
                }
            }

            // 错误例子：
            //   123abc
            //   12.abc
            //
            // 数字后面直接接字母或非法小数点，不能当作合法常量。
            if (!isAtEnd()
            && (std::isalpha(static_cast<unsigned char>(peek()))
            || (peek() == '.' && peekNext() != '.')))
            {
                std::string bad = number;
                while (!isAtEnd() && !std::isspace(static_cast<unsigned char>(peek())))
                {
                    char next = peek();
                    if (next == ';' || next == ',' || next == ')' || next == ']')
                    {
                        break;
                    }
                    bad.push_back(advance());
                }
                tokens.push_back({TokenType::ERROR, bad, startLine, startColumn, "err", -1});
                continue;
            }

            // 合法数字常量加入 C 表。
            int constIndex = addConstant(number, type, std::strtod(number.c_str(), nullptr));
            tokens.push_back({TokenType::CONSTANT, number, startLine, startColumn, "c", constIndex});
        }
        // =========================
        // 分支 3：单引号开头 -> 字符常量
        // =========================
        //
        // 支持最简单的 Pascal 风格字符常量：'a'
        // 当前没有处理转义字符，例如 '\n'。
        else if (ch == '\'')
        {
            std::string value;
            value.push_back(advance());
            if (!isAtEnd() && peek() != '\n' && peek() != '\'')
            {
                value.push_back(advance());
            }
            if (!isAtEnd() && peek() == '\'')
            {
                // 成功读到右单引号，说明字符常量完整。
                value.push_back(advance());
                int constIndex = addConstant(value, "char", value.size() >= 2 ? static_cast<double>(value[1]) : 0.0);
                tokens.push_back({TokenType::CONSTANT, value, startLine, startColumn, "c", constIndex});
            }
            else
            {
                // 没有右单引号，生成 ERROR Token。
                tokens.push_back({TokenType::ERROR, value, startLine, startColumn, "err", -1});
            }
        }
        // =========================
        // 分支 4：其它符号 -> 运算符、界符或错误
        // =========================
        else
        {
            std::string text;
            text.push_back(advance());

            // 两字符运算符：:=
            if (text == ":" && !isAtEnd() && peek() == '=')
            {
                text.push_back(advance());
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            }
            // 两字符运算符：!=
            else if (text == "!" && !isAtEnd() && peek() == '=')
            {
                text.push_back(advance());
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            }
            // 两字符界符：..，用于数组上下界 array[1..5]
            else if (text == "." && !isAtEnd() && peek() == '.')
            {
                text.push_back(advance());
                tokens.push_back({TokenType::DELIMITER, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            }
            // 单字符运算符。
            else if (text == "+" || text == "-" || text == "*" || text == "/" || text == "="
            || text == "<" || text == ">")
            {
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            }
            // 单字符界符，例如 ; , ( ) [ ] .
            else if (findDelimiterIndex(text) > 0)
            {
                tokens.push_back({TokenType::DELIMITER, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            }
            else
            {
                // 不属于任何合法符号，就记为词法错误。
                tokens.push_back({TokenType::ERROR, text, startLine, startColumn, "err", -1});
            }
        }
    }

    // 给 Parser 一个明确的结束标记。
    // Parser 最后会 consume END_OF_FILE，用来确认程序结尾后没有多余内容。
    tokens.push_back({TokenType::END_OF_FILE, "EOF", line_, column_, "e", -1});
    return tokens;
}

const std::vector<std::string>& Lexer::getIdentifierTable() const {
    return identifiers_;
}

const std::vector<std::string>& Lexer::getConstantTable() const {
    return constants_;
}

const std::vector<ConstantEntry>& Lexer::getConstantEntries() const {
    return constantEntries_;
}

const std::vector<KeywordEntry>& Lexer::getKeywordTable() const {
    return keywordTable_;
}

const std::vector<DelimiterEntry>& Lexer::getDelimiterTable() const {
    return delimiterTable_;
}

char Lexer::peek() const {
    // 查看当前字符，但不移动 pos_。
    return source_[pos_];
}

char Lexer::peekNext() const {
    // 查看下一个字符，用来判断 ":="、"!="、".." 这类双字符符号。
    if (pos_ + 1 >= source_.size())
    {
        return '\0';
    }
    return source_[pos_ + 1];
}

char Lexer::advance()
{
    // 读出当前字符，并把位置往后移动一格。
    // 同时维护 line_/column_，保证错误信息能定位到行列。
    char ch = source_[pos_++];
    if (ch == '\n')
    {
        ++line_;
        column_ = 1;
    }
    else
    {
        ++column_;
    }
    return ch;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespace()
{
    // 连续跳过所有空白字符。
    // std::isspace 会识别空格、换行、Tab 等。
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek())))
    {
        advance();
    }
}

bool Lexer::skipComment()
{
    // 块注释：{ ... }
    // 这里会一直跳到右花括号；如果源码结束也不会崩溃。
    if (peek() == '{')
        {
            advance();
            while (!isAtEnd() && peek() != '}')
        {
            advance();
        }
        if (!isAtEnd())
        {
            advance();
        }
        return true;
    }

    // 行注释：// ...
    // 一直跳到换行符，换行符本身留给下一轮 skipWhitespace 处理。
    if (peek() == '/' && peekNext() == '/')
    {
        while (!isAtEnd() && peek() != '\n')
        {
            advance();
        }
        return true;
    }

    return false;
}

int Lexer::addIdentifier(const std::string& value)
{
    // 标识符表去重。
    // 同一个变量名多次出现，应该使用同一个编号。
    for (std::size_t i = 0; i < identifiers_.size(); ++i)
    {
        if (identifiers_[i] == value)
        {
            return static_cast<int>(i) + 1;
        }
    }
    identifiers_.push_back(value);
    return static_cast<int>(identifiers_.size());
}

int Lexer::addConstant(const std::string& text, const std::string& type, double numberValue)
{
    // 常量表也去重。
    // 注意：文本和类型都相同才认为是同一个常量。
    for (std::size_t i = 0; i < constantEntries_.size(); ++i)
    {
        if (constantEntries_[i].text == text && constantEntries_[i].type == type)
        {
            return static_cast<int>(i) + 1;
        }
    }
    int index = static_cast<int>(constantEntries_.size()) + 1;
    constants_.push_back(text);
    constantEntries_.push_back({index, text, type, numberValue});
    return index;
}

int Lexer::findKeywordIndex(const std::string& word) const {
    // 线性查找关键字编号。关键字数量很少，没必要用复杂结构。
    for (const auto& entry : keywordTable_)
    {
        if (entry.word == word)
        {
            return entry.index;
        }
    }
    return -1;
}

int Lexer::findDelimiterIndex(const std::string& symbol) const {
    // 查找界符/运算符编号。
    for (const auto& entry : delimiterTable_)
    {
        if (entry.symbol == symbol)
        {
            return entry.index;
        }
    }
    return -1;
}

std::string Lexer::toLower(const std::string& text) const {
    // 把关键字统一转成小写，实现大小写不敏感。
    std::string result;
    for (char ch : text)
    {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

std::string tokenTypeToString(TokenType type)
{
    // 给 UI/CLI 显示用，把枚举值转换成人能读的字符串。
    switch (type)
    {
        case TokenType::KEYWORD: return "KEYWORD";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::CONSTANT: return "CONSTANT";
        case TokenType::DELIMITER: return "DELIMITER";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::ERROR: return "ERROR";
    }
    return "ERROR";
}

std::string tokenCodeToString(const Token& token)
{
    // 转成课程设计常见的二元式格式：
    // KEYWORD -> (k,编号)
    // IDENTIFIER -> (i,编号)
    // CONSTANT -> (c,编号)
    if (token.type == TokenType::END_OF_FILE)
    {
        return "(e,_)";
    }
    if (token.type == TokenType::ERROR)
    {
        return "(err,_)";
    }
    return "(" + token.code + "," + std::to_string(token.value) + ")";
}

