#include "Lexer.h"
#include <cctype>
#include <set>
#include <utility>

Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    const std::set<std::string> keywords = {
        "program", "type", "record", "var", "begin", "end",
        "integer", "real", "char"
    };

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) {
            break;
        }

        int startLine = line_;
        int startColumn = column_;
        char ch = peek();

        if (std::isalpha(static_cast<unsigned char>(ch))) {
            std::string word;
            while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
                word.push_back(advance());
            }

            if (keywords.count(word)) {
                tokens.push_back({TokenType::KEYWORD, word, startLine, startColumn});
            } else {
                addUnique(identifiers_, word);
                tokens.push_back({TokenType::IDENTIFIER, word, startLine, startColumn});
            }
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            std::string number;
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                number.push_back(advance());
            }

            // 实数常量必须形如 12.34；如果点号后面不是数字，点号留给界符处理。
            if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
                number.push_back(advance());
                while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    number.push_back(advance());
                }
            }
            addUnique(constants_, number);
            tokens.push_back({TokenType::CONSTANT, number, startLine, startColumn});
        } else {
            std::string text;
            text.push_back(advance());
            if (text == ":" && !isAtEnd() && peek() == '=') {
                text.push_back(advance());
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn});
            } else if (text == "+" || text == "-" || text == "*" || text == "/" || text == "=") {
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn});
            } else if (text == ";" || text == ":" || text == "," || text == "." || text == "(" || text == ")") {
                tokens.push_back({TokenType::DELIMITER, text, startLine, startColumn});
            } else {
                // 非法字符不直接中断分析，而是生成 ERROR Token，便于界面或控制台定位问题。
                tokens.push_back({TokenType::ERROR, text, startLine, startColumn});
            }
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "EOF", line_, column_});
    return tokens;
}

const std::vector<std::string>& Lexer::getIdentifierTable() const {
    return identifiers_;
}

const std::vector<std::string>& Lexer::getConstantTable() const {
    return constants_;
}

char Lexer::peek() const {
    return source_[pos_];
}

char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[pos_ + 1];
}

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

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

void Lexer::addUnique(std::vector<std::string>& table, const std::string& value) {
    for (const auto& item : table) {
        if (item == value) {
            return;
        }
    }
    table.push_back(value);
}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
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
