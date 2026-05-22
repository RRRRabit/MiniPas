
#pragma once

#include <string>
#include <vector>

using std::string;
using std::vector;
#include "Token.h"
#include "SymbolTable.h"
#include "TypeTable.h"
#include "Quadruple.h"

struct FunctionInfo
{
    string name;
    string returnType;
    int paramCount = 0;
};

struct ParameterInfo
{
    string functionName;
    string name;
    string type;
    string passMode; // vf 值参，vn 变参
};

struct BasicBlock
{
    int id = 0;
    int start = 0;
    int end = 0;
};

struct ParserTraceNode // 语法递归调用轨迹
{
    int depth = 0;
    string rule;
    string text;
};

struct ParserLogItem // 语法步骤/动作日志
{
    string rule;
    string text;
};

struct TargetCodeItem
{
    string basicBlock;
    string quadrupleText;
    string targetCodeText;
    string rdl;
    string sem;
};

struct ActivationRecordItem
{
    string scope; // 作用域
    string name;
    int offset = 0;
    int size = 0;
};

struct RuntimeValue
{
    string name;
    string value;
};

// CompileResult 是编译报告。
//
// - Token 表来自 tokens。
// - 标识符表来自 identifierTable。
// - 常量表来自 constantEntries。
// - 符号表来自 symbols。
// - 类型表来自 recordTypes / arrayTypes。
// - 函数表和参数表来自 functionTable / parameterTable。
// - 原始四元式表来自 quadruples。
// - 优化后四元式表来自 optimizedQuadruples。
// - 基本块表来自 basicBlocks。
// - 递归分析过程来自 parserTrace / parserSteps / parserActions。
// - 目标代码来自 targetCodeItems。
// - 活动记录来自 activationRecords。
// - VM 运行结果来自 runtimeValues。
struct CompileResult
{
    bool success = false;                      // 整体是否成功
    string errorMessage;                  // 主错误文本
    string errorStage;                    // 错误阶段（词法/语法/语义/运行）
    int errorLine = 0;                         // 错误行号（无则为 0）
    int errorColumn = 0;                       // 错误列号（无则为 0）
    string errorToken;                    // 出错时当前 Token
    string expectedToken;                 // 期望 Token
    string errorSuggestion;               // 修复建议
    vector<string> additionalErrors; // 额外错误列表（多错误）

    vector<Token> tokens;                  // Token 序列（词法主产物）
    vector<KeywordEntry> keywordTable;     // 关键字表
    vector<DelimiterEntry> delimiterTable; // 界符表
    vector<string> identifierTable;   // I 表
    vector<string> constantTable;     // C 表（仅常量文本）
    vector<ConstantEntry> constantEntries; // C 表（文本 + 类型）

    vector<Symbol> symbols;               // 符号表
    vector<RecordType> recordTypes;       // record 类型定义
    vector<ArrayType> arrayTypes;         // array 类型定义
    vector<FunctionInfo> functionTable;   // 函数表
    vector<ParameterInfo> parameterTable; // 参数表

    vector<Quadruple> quadruples;          // 原始四元式
    vector<Quadruple> optimizedQuadruples; // 优化后四元式
    vector<BasicBlock> basicBlocks;        // 基本块划分结果

    vector<ParserTraceNode> parserTrace; // 递归调用树数据
    vector<ParserLogItem> parserSteps;   // 语法匹配步骤日志
    vector<ParserLogItem> parserActions; // 语义动作日志

    bool vmFallbackUsed = false;             // 是否触发 VM 回退
    double vmRuntimeMs = 0.0;                // VM 运行耗时（毫秒）
    string vmErrorMessage;              // VM 错误信息
    vector<RuntimeValue> runtimeValues; // 最终运行结果
};
