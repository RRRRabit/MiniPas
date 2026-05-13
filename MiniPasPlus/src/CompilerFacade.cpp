#include "CompilerFacade.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool isNumberLiteral(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool isCommutativeOp(const std::string& op) {
    return op == "+" || op == "*";
}

bool isArithmeticOp(const std::string& op) {
    return op == "+" || op == "-" || op == "*" || op == "/";
}

std::string formatNumber(double value) {
    std::string text = std::to_string(value);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads, const std::vector<BasicBlock>& blocks) {
    if (quads.empty()) {
        return {};
    }

    std::vector<Quadruple> optimized = quads;
    for (const auto& block : blocks) {
        if (block.start < 0 || block.end < block.start || block.end >= static_cast<int>(quads.size())) {
            continue;
        }

        std::unordered_map<std::string, std::string> alias;
        std::unordered_map<std::string, std::string> exprToValue;
        std::vector<Quadruple> blockOut;

        auto resolve = [&](const std::string& x) {
            if (x == "_" || x.empty()) {
                return x;
            }
            if (isNumberLiteral(x)) {
                return x;
            }
            auto it = alias.find(x);
            int guard = 0;
            std::string cur = x;
            while (it != alias.end() && it->second != cur && guard < 32) {
                cur = it->second;
                if (isNumberLiteral(cur)) {
                    break;
                }
                it = alias.find(cur);
                ++guard;
            }
            return cur;
        };

        auto killExprByVar = [&](const std::string& var) {
            for (auto it = exprToValue.begin(); it != exprToValue.end();) {
                const std::string& key = it->first;
                const std::string& value = it->second;
                bool hit = key.find("|" + var + "|") != std::string::npos
                    || (key.size() >= var.size() && key.compare(key.size() - var.size(), var.size(), var) == 0)
                    || value == var;
                if (hit) {
                    it = exprToValue.erase(it);
                } else {
                    ++it;
                }
            }
        };

        for (int i = block.start; i <= block.end; ++i) {
            const Quadruple& q = quads[i];

            if (q.op == ":=") {
                std::string src = resolve(q.arg1);
                killExprByVar(q.result);
                alias[q.result] = src;
                if (src != q.result) {
                    blockOut.push_back({":=", src, "_", q.result});
                }
                continue;
            }

            if (isArithmeticOp(q.op)) {
                std::string left = resolve(q.arg1);
                std::string right = resolve(q.arg2);

                if (isCommutativeOp(q.op) && left > right) {
                    std::swap(left, right);
                }

                if (q.op == "+" && right == "0") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "-" && right == "0") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "*" && (left == "0" || right == "0")) {
                    killExprByVar(q.result);
                    alias[q.result] = "0";
                    blockOut.push_back({":=", "0", "_", q.result});
                    continue;
                }
                if (q.op == "*" && right == "1") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "/" && right == "1") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }

                if (isNumberLiteral(left) && isNumberLiteral(right)) {
                    double lv = std::strtod(left.c_str(), nullptr);
                    double rv = std::strtod(right.c_str(), nullptr);
                    double fv = 0.0;
                    if (q.op == "+") {
                        fv = lv + rv;
                    } else if (q.op == "-") {
                        fv = lv - rv;
                    } else if (q.op == "*") {
                        fv = lv * rv;
                    } else if (q.op == "/" && rv != 0.0) {
                        fv = lv / rv;
                    } else {
                        blockOut.push_back({q.op, left, right, q.result});
                        killExprByVar(q.result);
                        alias[q.result] = q.result;
                        continue;
                    }
                    std::string folded = formatNumber(fv);
                    killExprByVar(q.result);
                    alias[q.result] = folded;
                    blockOut.push_back({":=", folded, "_", q.result});
                    continue;
                }

                std::string key = q.op + "|" + left + "|" + right;
                auto found = exprToValue.find(key);
                if (found != exprToValue.end()) {
                    killExprByVar(q.result);
                    alias[q.result] = found->second;
                    if (found->second != q.result) {
                        blockOut.push_back({":=", found->second, "_", q.result});
                    }
                    continue;
                }

                killExprByVar(q.result);
                alias[q.result] = q.result;
                blockOut.push_back({q.op, left, right, q.result});
                exprToValue[key] = q.result;
                continue;
            }

            // 其余控制流四元式直接保留。
            blockOut.push_back(q);
        }

        int outIndex = 0;
        for (int i = block.start; i <= block.end; ++i) {
            if (outIndex < static_cast<int>(blockOut.size())) {
                optimized[i] = blockOut[outIndex++];
            } else {
                optimized[i] = {"nop", "_", "_", "_"};
            }
        }
    }

    std::vector<Quadruple> compacted;
    compacted.reserve(optimized.size());
    for (const auto& q : optimized) {
        if (q.op != "nop") {
            compacted.push_back(q);
        }
    }
    return compacted;
}

} // namespace

CompileResult CompilerFacade::compileAndRun(const std::string& sourceCode) {
    CompileResult finalResult;

    try {
        // 1. 词法分析：生成 Token 序列、标识符表和常数表。
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();

        finalResult.tokens = tokens;
        finalResult.keywordTable = lexer.getKeywordTable();
        finalResult.delimiterTable = lexer.getDelimiterTable();
        finalResult.identifierTable = lexer.getIdentifierTable();
        finalResult.constantTable = lexer.getConstantTable();
        finalResult.constantEntries = lexer.getConstantEntries();

        for (const auto& token : tokens) {
            if (token.type == TokenType::ERROR) {
                finalResult.success = false;
                finalResult.errorMessage = "词法错误: line " + std::to_string(token.line)
                    + ", column " + std::to_string(token.column)
                    + " 非法字符 '" + token.lexeme + "'";
                return finalResult;
            }
        }

        // 2. 语法分析：递归下降分析，同时填写表并生成四元式。
        Parser parser(tokens);
        CompileResult parseResult = parser.parse();

        finalResult.symbols = parseResult.symbols;
        finalResult.recordTypes = parseResult.recordTypes;
        finalResult.arrayTypes = parseResult.arrayTypes;
        finalResult.functionTable = parseResult.functionTable;
        finalResult.parameterTable = parseResult.parameterTable;
        finalResult.activationRecords = parseResult.activationRecords;
        finalResult.quadruples = parseResult.quadruples;
        finalResult.basicBlocks = parseResult.basicBlocks;
        finalResult.optimizedQuadruples = optimizeByBasicBlocks(finalResult.quadruples, finalResult.basicBlocks);

        // 3. 解释执行：运行四元式，得到最终变量值。
        Interpreter interpreter;
        interpreter.execute(finalResult.quadruples);
        finalResult.runtimeValues = interpreter.values();

        finalResult.success = true;
    } catch (const std::exception& ex) {
        finalResult.success = false;
        finalResult.errorMessage = ex.what();
    }

    return finalResult;
}
