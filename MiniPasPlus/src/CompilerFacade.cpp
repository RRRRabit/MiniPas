
#include "CompilerFacade.h"
#include "Lexer.h"
#include "Parser.h"
#include "BackendOptimization.h"
#include "TargetCodeDemo.h"
#include "ActivationRecordDemo.h"
#include "RuntimeResultDemo.h"
#include <exception>

using std::exception;
using std::string;

CompileResult CompilerFacade::compileAndRun(const string &sourceCode)
{
    CompileResult result;

    try
    {
        Lexer lexer(sourceCode);
        result.tokens = lexer.tokenize(); // 源码先切成 Token，Parser 后面只看 Token。

        // 词法表直接来自 Lexer，界面和后续说明都复用这些结果。
        result.keywordTable = lexer.keywordTable();
        result.delimiterTable = lexer.delimiterTable();
        result.identifierTable = lexer.identifierTable();

        result.constantTable.clear();
        for (const auto &entry : lexer.constantTable())
        {
            result.constantTable.push_back(entry.text);
        }
        result.constantEntries = lexer.constantTable();

        Parser parser(result.tokens);
        CompileResult parseResult = parser.parse(); // 递归下降分析会产出语义表和四元式。

        // Parser 的结果合并回总结果对象。
        result.symbols = parseResult.symbols;
        result.recordTypes = parseResult.recordTypes;
        result.arrayTypes = parseResult.arrayTypes;
        result.functionTable = parseResult.functionTable;
        result.parameterTable = parseResult.parameterTable;
        result.quadruples = parseResult.quadruples;
        result.basicBlocks = parseResult.basicBlocks;
        result.parserTrace = parseResult.parserTrace;
        result.parserSteps = parseResult.parserSteps;
        result.parserActions = parseResult.parserActions;

        BackendOptimization optimizer;
        result.optimizedQuadruples = optimizer.optimizeByBasicBlocks(result.quadruples, result.basicBlocks);

        TargetCodeDemo targetCode;
        result.targetCodeItems = targetCode.generate(result.optimizedQuadruples, result.basicBlocks);

        ActivationRecordDemo activationRecord;
        result.activationRecords = activationRecord.build(result.symbols, result.functionTable, result.parameterTable);

        RuntimeResultDemo runtimeResult;
        result.runtimeValues = runtimeResult.makePlaceholder(result.symbols);

        result.success = true;
    }
    catch (const exception &e)
    {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

// 兼容旧接口：保持 compile 可用，内部直接复用 compileAndRun。
CompileResult CompilerFacade::compile(const string &sourceCode)
{
    return compileAndRun(sourceCode);
}
