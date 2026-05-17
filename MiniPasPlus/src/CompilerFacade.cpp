#include "CompilerFacade.h"
#include "CompilerBackendStages.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "VirtualMachine.h"

#include <chrono>
#include <exception>
#include <string>
#include <vector>

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

        for (const auto& token : tokens)
        {
            if (token.type == TokenType::ERROR)
            {
                finalResult.success = false;
                finalResult.errorMessage = "词法错误: line " + std::to_string(token.line)
                + ", column " + std::to_string(token.column)
                + " 非法字符 '" + token.lexeme + "'";
                return finalResult;
            }
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
        finalResult.success = false;
        finalResult.errorMessage = ex.what();
    }

    return finalResult;
}

