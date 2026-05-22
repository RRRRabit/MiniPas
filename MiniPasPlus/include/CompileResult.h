
#pragma once

#include <string>
#include <vector>
#include "Token.h"
#include "SymbolTable.h"
#include "TypeTable.h"
#include "Quadruple.h"

struct FunctionInfo {
    std::string name;
    std::string returnType;
    int paramCount = 0;
};

struct ParameterInfo {
    std::string functionName;
    std::string name;
    std::string type;
    std::string passMode; // vf 值参，vn 变参
};

struct BasicBlock {
    int id = 0;
    int start = 0;
    int end = 0;
};

struct ParserTraceNode {
    int depth = 0;
    std::string rule;
    std::string text;
};

struct ParserLogItem {
    std::string rule;
    std::string text;
};

struct TargetCodeItem {
    std::string basicBlock;
    std::string quadrupleText;
    std::string targetCodeText;
    std::string rdl;
    std::string sem;
};

struct ActivationRecordItem {
    std::string scope;
    std::string name;
    int offset = 0;
    int size = 0;
};

struct RuntimeValue {
    std::string name;
    std::string value;
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
struct CompileResult {
    bool success = false;                 // 整体是否成功
    std::string errorMessage;             // 主错误文本
    std::string errorStage;               // 错误阶段（词法/语法/语义/运行）
    int errorLine = 0;                    // 错误行号（无则为 0）
    int errorColumn = 0;                  // 错误列号（无则为 0）
    std::string errorToken;               // 出错时当前 Token
    std::string expectedToken;            // 期望 Token
    std::string errorSuggestion;          // 修复建议
    std::vector<std::string> additionalErrors; // 额外错误列表（多错误）

    std::vector<Token> tokens;                 // Token 序列（词法主产物）
    std::vector<KeywordEntry> keywordTable;    // 关键字表
    std::vector<DelimiterEntry> delimiterTable;// 界符表
    std::vector<std::string> identifierTable;  // I 表
    std::vector<std::string> constantTable;    // C 表（仅常量文本）
    std::vector<ConstantEntry> constantEntries;// C 表（文本 + 类型）

    std::vector<Symbol> symbols;               // 符号表
    std::vector<RecordType> recordTypes;       // record 类型定义
    std::vector<ArrayType> arrayTypes;         // array 类型定义
    std::vector<FunctionInfo> functionTable;   // 函数表
    std::vector<ParameterInfo> parameterTable; // 参数表

    std::vector<Quadruple> quadruples;          // 原始四元式
    std::vector<Quadruple> optimizedQuadruples; // 优化后四元式
    std::vector<BasicBlock> basicBlocks;        // 基本块划分结果

    std::vector<ParserTraceNode> parserTrace;   // 递归调用树数据
    std::vector<ParserLogItem> parserSteps;     // 语法匹配步骤日志
    std::vector<ParserLogItem> parserActions;   // 语义动作日志

    bool vmFallbackUsed = false;          // 是否触发 VM 回退
    double vmRuntimeMs = 0.0;             // VM 运行耗时（毫秒）
    std::string vmErrorMessage;           // VM 错误信息
    std::vector<RuntimeValue> runtimeValues; // 最终运行结果
};





