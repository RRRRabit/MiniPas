#include "CompilerFacade.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include <exception>

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
        finalResult.quadruples = parseResult.quadruples;

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
