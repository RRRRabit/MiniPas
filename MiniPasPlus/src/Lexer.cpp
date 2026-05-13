#include "Lexer.h"
#include <cctype>
#include <cstdlib>
#include <utility>

Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1) {
    keywordTable_ = {
        {1, "program"}, {2, "var"}, {3, "integer"}, {4, "real"},
        {5, "char"}, {6, "begin"}, {7, "end"}, {8, "type"},
        {9, "record"}, {10, "array"}, {11, "of"}, {12, "function"},
        {13, "boolean"}
    };

    delimiterTable_ = {
        {1, ","}, {2, ":"}, {3, ";"}, {4, ":="}, {5, "*"},
        {6, "/"}, {7, "+"}, {8, "-"}, {9, "."}, {10, "("},
        {11, ")"}, {12, "="}, {13, "["}, {14, "]"}, {15, ".."}
    };
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) {
            break;
        }
        if (skipComment()) {
            continue;
        }

        int startLine = line_;
        int startColumn = column_;
        char ch = peek();

        if (std::isalpha(static_cast<unsigned char>(ch))) {
            std::string word;
            while (!isAtEnd() && std::isalnum(static_cast<unsigned char>(peek()))) {
                word.push_back(advance());
            }

            std::string lowerWord = toLower(word);
            int keywordIndex = findKeywordIndex(lowerWord);
            if (keywordIndex > 0) {
                tokens.push_back({TokenType::KEYWORD, lowerWord, startLine, startColumn, "k", keywordIndex});
            } else {
                int idIndex = addIdentifier(word);
                tokens.push_back({TokenType::IDENTIFIER, word, startLine, startColumn, "i", idIndex});
            }
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            std::string number;
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                number.push_back(advance());
            }

            std::string type = "integer";
            if (!isAtEnd() && peek() == '.' && peekNext() != '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
                type = "real";
                number.push_back(advance());
                while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    number.push_back(advance());
                }
            }

            if (!isAtEnd()
                && (std::isalpha(static_cast<unsigned char>(peek()))
                    || (peek() == '.' && peekNext() != '.'))) {
                std::string bad = number;
                while (!isAtEnd() && !std::isspace(static_cast<unsigned char>(peek()))) {
                    char next = peek();
                    if (next == ';' || next == ',' || next == ')' || next == ']') {
                        break;
                    }
                    bad.push_back(advance());
                }
                tokens.push_back({TokenType::ERROR, bad, startLine, startColumn, "err", -1});
                continue;
            }

            int constIndex = addConstant(number, type, std::strtod(number.c_str(), nullptr));
            tokens.push_back({TokenType::CONSTANT, number, startLine, startColumn, "c", constIndex});
        } else if (ch == '\'') {
            std::string value;
            value.push_back(advance());
            if (!isAtEnd() && peek() != '\n' && peek() != '\'') {
                value.push_back(advance());
            }
            if (!isAtEnd() && peek() == '\'') {
                value.push_back(advance());
                int constIndex = addConstant(value, "char", value.size() >= 2 ? static_cast<double>(value[1]) : 0.0);
                tokens.push_back({TokenType::CONSTANT, value, startLine, startColumn, "c", constIndex});
            } else {
                tokens.push_back({TokenType::ERROR, value, startLine, startColumn, "err", -1});
            }
        } else {
            std::string text;
            text.push_back(advance());

            if (text == ":" && !isAtEnd() && peek() == '=') {
                text.push_back(advance());
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            } else if (text == "." && !isAtEnd() && peek() == '.') {
                text.push_back(advance());
                tokens.push_back({TokenType::DELIMITER, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            } else if (text == "+" || text == "-" || text == "*" || text == "/" || text == "=") {
                tokens.push_back({TokenType::OPERATOR, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            } else if (findDelimiterIndex(text) > 0) {
                tokens.push_back({TokenType::DELIMITER, text, startLine, startColumn, "p", findDelimiterIndex(text)});
            } else {
                tokens.push_back({TokenType::ERROR, text, startLine, startColumn, "err", -1});
            }
        }
    }

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

bool Lexer::skipComment() {
    if (peek() == '{') {
        advance();
        while (!isAtEnd() && peek() != '}') {
            advance();
        }
        if (!isAtEnd()) {
            advance();
        }
        return true;
    }

    if (peek() == '/' && peekNext() == '/') {
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
        return true;
    }

    return false;
}

int Lexer::addIdentifier(const std::string& value) {
    for (std::size_t i = 0; i < identifiers_.size(); ++i) {
        if (identifiers_[i] == value) {
            return static_cast<int>(i) + 1;
        }
    }
    identifiers_.push_back(value);
    return static_cast<int>(identifiers_.size());
}

int Lexer::addConstant(const std::string& text, const std::string& type, double numberValue) {
    for (std::size_t i = 0; i < constantEntries_.size(); ++i) {
        if (constantEntries_[i].text == text && constantEntries_[i].type == type) {
            return static_cast<int>(i) + 1;
        }
    }
    int index = static_cast<int>(constantEntries_.size()) + 1;
    constants_.push_back(text);
    constantEntries_.push_back({index, text, type, numberValue});
    return index;
}

int Lexer::findKeywordIndex(const std::string& word) const {
    for (const auto& entry : keywordTable_) {
        if (entry.word == word) {
            return entry.index;
        }
    }
    return -1;
}

int Lexer::findDelimiterIndex(const std::string& symbol) const {
    for (const auto& entry : delimiterTable_) {
        if (entry.symbol == symbol) {
            return entry.index;
        }
    }
    return -1;
}

std::string Lexer::toLower(const std::string& text) const {
    std::string result;
    for (char ch : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
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

std::string tokenCodeToString(const Token& token) {
    if (token.type == TokenType::END_OF_FILE) {
        return "(e,_)";
    }
    if (token.type == TokenType::ERROR) {
        return "(err,_)";
    }
    return "(" + token.code + "," + std::to_string(token.value) + ")";
}
