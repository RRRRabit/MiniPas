#include "CompilerFacade.h"
#include "CompilerBackendStages.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "VirtualMachine.h"

#include <chrono>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace {
// 把某个编译阶段的错误统一写进 CompileResult。
//
// 这个函数的作用不是“发现错误”，而是“整理错误”：
// Lexer / Parser / VM 发现问题后，CompilerFacade 会把阶段、位置、原因、建议统一放进 result。
// 这样 CLI 和 GUI 都能用同一个格式展示错误。
void setCompileError(CompileResult& result,
                     const std::string& stage,
                     const std::string& reason,
                     int line = 0,
                     int column = 0,
                     const std::string& token = "",
                     const std::string& expected = "",
                     const std::string& suggestion = "")
{
    // success=false 表示本次编译失败。后续调用者只要检查 success 即可。
    result.success = false;
    result.errorStage = stage;
    result.errorLine = line;
    result.errorColumn = column;
    result.errorToken = token;
    result.expectedToken = expected;
    result.errorSuggestion = suggestion;

    // 用 stringstream 拼接多行错误信息，比连续字符串相加更清晰。
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

// Parser 报错时通常会抛出类似：
// line 3, column 5: 变量声明缺少 :
// 这个函数从错误字符串里提取 line 和 column，方便 GUI 定位高亮。
// 提取失败也没关系，返回 false，错误仍然可以显示。
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

} // namespace

// 编译器总入口。
//
// 你答辩时一定要优先讲这个函数，因为它把所有模块串起来了。
// 输入：sourceCode，一整段 MiniPas 源程序文本。
// 输出：CompileResult，里面装着 Token、符号表、四元式、VM 指令、运行结果或错误信息。
//
// 主流程：
// 1. Lexer：源代码字符串 -> Token 序列。
// 2. Parser：Token 序列 -> 符号表/类型表/函数表/四元式/基本块。
// 3. BackendOptimization：原始四元式 -> 优化后四元式。
// 4. generateVmInstructions：四元式 -> VM 指令。
// 5. VirtualMachine：执行 VM 指令，得到最终变量值。
CompileResult CompilerFacade::compileAndRun(const std::string& sourceCode)
{
    CompileResult finalResult;

    try
    {
        // =========================
        // 第 1 阶段：词法分析
        // =========================
        //
        // Lexer 会从左到右扫描 sourceCode，把字符流切成 Token。
        // 例如 "a := 1" 会变成：
        // IDENTIFIER(a), OPERATOR(:=), CONSTANT(1)
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();

        // 这些表主要用于展示和后续语法分析：
        // tokens：Token 序列，是 Parser 的输入。
        // keywordTable/delimiterTable：关键字表和界符表。
        // identifierTable：出现过的变量名、函数名等。
        // constantTable/constantEntries：出现过的常量。
        finalResult.tokens = tokens;
        finalResult.keywordTable = lexer.getKeywordTable();
        finalResult.delimiterTable = lexer.getDelimiterTable();
        finalResult.identifierTable = lexer.getIdentifierTable();
        finalResult.constantTable = lexer.getConstantTable();
        finalResult.constantEntries = lexer.getConstantEntries();

        // Lexer 不直接 throw，而是把非法字符做成 ERROR Token。
        // 这里统一检查 ERROR Token，如果存在，说明词法阶段失败。
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
            return finalResult;
        }

        // =========================
        // 第 2 阶段：语法分析 + 语义检查 + 四元式生成
        // =========================
        //
        // Parser 使用递归下降方法：
        // parseProgram -> parseDeclPart -> parseStmt -> parseExpression ...
        //
        // 它不仅检查语法，还会做语义检查：
        // 变量是否声明、类型是否兼容、数组下标是不是 integer、函数参数是否匹配。
        Parser parser(tokens);
        CompileResult parseResult = parser.parse();

        // Parser 产生的结果复制到最终结果里。
        // 这些数据有些用于运行，有些用于 GUI 展示和答辩说明。
        finalResult.symbols = parseResult.symbols;
        finalResult.recordTypes = parseResult.recordTypes;
        finalResult.arrayTypes = parseResult.arrayTypes;
        finalResult.functionTable = parseResult.functionTable;
        finalResult.parameterTable = parseResult.parameterTable;
        finalResult.quadruples = parseResult.quadruples;
        finalResult.basicBlocks = parseResult.basicBlocks;

        // 递归下降分析过程日志也必须从 Parser 结果复制出来。
        // 它们不参与后续优化和 VM 执行，但 GUI 的“递归分析过程”页面依赖这些数据：
        // - parserTrace：左侧递归调用树；
        // - parserSteps：右上 Token 匹配/预测日志；
        // - parserActions：右下四元式和临时变量动作日志。
        finalResult.parserTrace = parseResult.parserTrace;
        finalResult.parserSteps = parseResult.parserSteps;
        finalResult.parserActions = parseResult.parserActions;

        // =========================
        // 第 3 阶段：四元式优化
        // =========================
        //
        // 优化器以基本块为单位做局部优化：
        // 常量折叠、复制传播、公共子表达式消除、代数化简、删除无用临时变量。
        // 注意：优化结果主要用于展示优化效果；VM 当前仍使用原始四元式生成指令，
        // 这样控制流和函数调用路径更稳定，也更容易解释。
        finalResult.optimizedQuadruples = backend::optimizeByBasicBlocks(
        finalResult.quadruples, finalResult.basicBlocks);

        // =========================
        // 第 4 阶段：四元式 -> VM 指令
        // =========================
        //
        // 例如：
        // (+, a, b, t1)  -> ADD t1 a b
        // (:=, t1, _, c) -> MOV c t1
        //
        // 函数表和参数表用于处理 CALL/RET 和形参绑定。
        finalResult.vmInstructions = vm::generateVmInstructions(
        finalResult.quadruples, finalResult.functionTable, finalResult.parameterTable);

        try
        {
            // =========================
            // 第 5 阶段：VM 执行
            // =========================
            //
            // VM 像一个小型解释器：
            // 它维护 programCounter，每次执行一条指令，直到遇到 HALT。
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
            // 如果 VM 执行出现异常，比如除 0 或跳转错误，就回退到简单四元式解释器。
            // 这样至少能给出一部分基础算术程序的运行结果。
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
        // Parser 或其它阶段抛出的异常会来到这里。
        // 这里不继续抛出，而是转换成 CompileResult，方便 CLI/GUI 展示。
        const std::string raw = ex.what();
        int line = 0;
        int col = 0;
        tryParseLineColumn(raw, line, col);

        std::string stage = "语法/语义";
        std::string suggestion = "检查报错位置附近的语句结构、标识符声明和类型是否匹配";
        if (raw.find("VM 运行时错误") != std::string::npos || raw.find("VM 执行步数超限") != std::string::npos) {
            stage = "VM";
            suggestion = "检查跳转标签、除法分母和循环退出条件";
        }

        setCompileError(finalResult, stage, raw, line, col, "", "", suggestion);
    }

    return finalResult;
}

