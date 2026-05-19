#include "CompilerFacade.h"
#include "CompilerBackendStages.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "SimplePrecedence.h"
#include "VirtualMachine.h"

#include <chrono>
#include <exception>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {
void setCompileError(CompileResult& result,
                     const std::string& stage,
                     const std::string& reason,
                     int line = 0,
                     int column = 0,
                     const std::string& token = "",
                     const std::string& expected = "",
                     const std::string& suggestion = "")
{
    result.success = false;
    result.errorStage = stage;
    result.errorLine = line;
    result.errorColumn = column;
    result.errorToken = token;
    result.expectedToken = expected;
    result.errorSuggestion = suggestion;

    std::ostringstream oss;
    oss << "阶段: " << stage << "\n";
    if (line > 0 && column > 0) {
        oss << "位置: line " << line << ", column " << column << "\n";
    }
    if (!token.empty()) {
        oss << "当前Token: " << token << "\n";
    }
    if (!expected.empty()) {
        oss << "期望: " << expected << "\n";
    }
    oss << "原因: " << reason;
    if (!suggestion.empty()) {
        oss << "\n建议: " << suggestion;
    }
    result.errorMessage = oss.str();
}

bool tryParseLineColumn(const std::string& msg, int& line, int& col)
{
    const std::string lineKey = "line ";
    const std::string colKey = ", column ";
    std::size_t linePos = msg.find(lineKey);
    std::size_t colPos = msg.find(colKey);
    if (linePos == std::string::npos || colPos == std::string::npos || colPos <= linePos + lineKey.size()) {
        return false;
    }
    try {
        line = std::stoi(msg.substr(linePos + lineKey.size(), colPos - (linePos + lineKey.size())));
        std::size_t reasonPos = msg.find(':', colPos + colKey.size());
        std::size_t colEnd = reasonPos == std::string::npos ? msg.size() : reasonPos;
        col = std::stoi(msg.substr(colPos + colKey.size(), colEnd - (colPos + colKey.size())));
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> collectAdditionalDiagnostics(const std::string& sourceCode)
{
    std::vector<std::string> diagnostics;
    std::vector<std::string> lines;
    std::stringstream ss(sourceCode);
    std::string oneLine;
    while (std::getline(ss, oneLine)) {
        lines.push_back(oneLine);
    }

    auto trim = [](std::string s) {
        std::size_t l = s.find_first_not_of(" \t\r\n");
        std::size_t r = s.find_last_not_of(" \t\r\n");
        if (l == std::string::npos) return std::string();
        return s.substr(l, r - l + 1);
    };

    int beginCount = 0;
    int endCount = 0;
    std::regex beginRe("\\bbegin\\b", std::regex_constants::icase);
    std::regex endRe("\\bend\\b", std::regex_constants::icase);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string line = lines[i];
        beginCount += static_cast<int>(std::distance(
            std::sregex_iterator(line.begin(), line.end(), beginRe), std::sregex_iterator()));
        endCount += static_cast<int>(std::distance(
            std::sregex_iterator(line.begin(), line.end(), endRe), std::sregex_iterator()));

        std::string t = trim(line);
        if (t.empty()) {
            continue;
        }

        if (t.find("IF ") != std::string::npos && t.find("THEN") == std::string::npos) {
            diagnostics.push_back("line " + std::to_string(i + 1) + ": IF 条件后缺少 THEN 关键字。");
        }
        if (t == "BEG" || t == "BEG:" || t == "BEGIN:") {
            diagnostics.push_back("line " + std::to_string(i + 1) + ": 复合语句起始关键字应为 BEGIN。");
        }
    }

    for (std::size_t i = 1; i < lines.size(); ++i) {
        std::string current = trim(lines[i]);
        if (current == "ELSE") {
            std::size_t prev = i;
            while (prev > 0) {
                --prev;
                std::string prevLine = trim(lines[prev]);
                if (prevLine.empty()) {
                    continue;
                }
                if (!prevLine.empty() && prevLine.back() != ';' && prevLine != "THEN" && prevLine != "BEGIN") {
                    diagnostics.push_back("line " + std::to_string(prev + 1) + ": 该语句行末可能缺少分号 ';'，会导致 ELSE 匹配失败。");
                }
                break;
            }
        }
    }

    if (endCount < beginCount) {
        diagnostics.push_back("全局检查: BEGIN/END 数量不平衡，缺少 END。");
    }

    return diagnostics;
}
} // namespace

CompileResult CompilerFacade::compileAndRun(const std::string& sourceCode)
{
    CompileResult finalResult;

    try
    {
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();
        finalResult.tokens = tokens;
        finalResult.keywordTable = lexer.getKeywordTable();
        finalResult.delimiterTable = lexer.getDelimiterTable();
        finalResult.identifierTable = lexer.getIdentifierTable();
        finalResult.constantTable = lexer.getConstantTable();
        finalResult.constantEntries = lexer.getConstantEntries();
        finalResult.simplePrecedence = simple_precedence::analyzeFromTokens(tokens);
        finalResult.simplePrecedenceAll = simple_precedence::analyzeAllExpressionsFromTokens(tokens);

        std::vector<Token> lexicalErrors;
        for (const auto& token : tokens) {
            if (token.type == TokenType::ERROR) {
                lexicalErrors.push_back(token);
            }
        }
        if (!lexicalErrors.empty()) {
            const Token& first = lexicalErrors.front();
            setCompileError(finalResult,
                            "词法",
                            "非法字符",
                            first.line,
                            first.column,
                            first.lexeme,
                            "合法关键字/标识符/界符/常量",
                            "检查是否存在乱码、全角符号或非法字符");
            for (std::size_t i = 1; i < lexicalErrors.size(); ++i) {
                const Token& t = lexicalErrors[i];
                finalResult.additionalErrors.push_back(
                    "line " + std::to_string(t.line)
                    + ", column " + std::to_string(t.column)
                    + ": 非法字符 '" + t.lexeme + "'。"
                );
            }
            std::vector<std::string> diagnostics = collectAdditionalDiagnostics(sourceCode);
            finalResult.additionalErrors.insert(finalResult.additionalErrors.end(), diagnostics.begin(), diagnostics.end());
            return finalResult;
        }

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
        finalResult.parserTraceTree = parseResult.parserTraceTree;
        finalResult.parserStepLog = parseResult.parserStepLog;
        finalResult.parserActionLog = parseResult.parserActionLog;
        finalResult.parserStepRuleNames = parseResult.parserStepRuleNames;
        finalResult.parserActionRuleNames = parseResult.parserActionRuleNames;

        finalResult.optimizedQuadruples = backend::optimizeByBasicBlocks(
        finalResult.quadruples, finalResult.basicBlocks);
        std::vector<BasicBlock> optimizedBlocks = backend::buildBasicBlocksFromQuads(
        finalResult.optimizedQuadruples);
        BackendGenerationResult target = backend::generateTargetArtifacts(
        finalResult.optimizedQuadruples, optimizedBlocks, finalResult.symbols);
        finalResult.targetCodes = std::move(target.codes);
        finalResult.targetTrace = std::move(target.trace);
        finalResult.vmInstructions = vm::generateVmInstructions(
        finalResult.quadruples, finalResult.functionTable);

        try
        {
            auto vmStart = std::chrono::steady_clock::now();
            VirtualMachine vm;
            vm.execute(finalResult.vmInstructions);
            auto vmEnd = std::chrono::steady_clock::now();
            finalResult.vmRuntimeMs = std::chrono::duration<double, std::milli>(
            vmEnd - vmStart).count();
            finalResult.runtimeValues = vm.values();
            finalResult.vmErrorMessage.clear();
        }
        catch (const std::exception& vmEx)
        {
            finalResult.vmFallbackUsed = true;
            finalResult.vmErrorMessage = vmEx.what();
            Interpreter interpreter;
            interpreter.execute(finalResult.quadruples);
            finalResult.runtimeValues = interpreter.values();
        }

        finalResult.success = true;
    }
    catch (const std::exception& ex)
    {
        const std::string raw = ex.what();
        int line = 0;
        int col = 0;
        tryParseLineColumn(raw, line, col);

        std::string stage = "语法/语义";
        std::string suggestion = "检查报错位置附近的语句结构、标识符声明和类型是否匹配";
        if (raw.find("简单优先") != std::string::npos) {
            stage = "简单优先";
            suggestion = "先简化表达式，再检查运算符和括号是否成对";
        } else if (raw.find("VM 运行时错误") != std::string::npos || raw.find("VM 执行步数超限") != std::string::npos) {
            stage = "VM";
            suggestion = "检查跳转标签、除法分母和循环退出条件";
        }

        setCompileError(finalResult, stage, raw, line, col, "", "", suggestion);
        finalResult.additionalErrors = collectAdditionalDiagnostics(sourceCode);
    }

    return finalResult;
}

