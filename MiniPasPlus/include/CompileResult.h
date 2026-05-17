#ifndef MINIPASPLUS_COMPILERESULT_H
#define MINIPASPLUS_COMPILERESULT_H

#include "Quadruple.h"
#include "SymbolTable.h"
#include "Token.h"
#include "TypeTable.h"
#include <map>
#include <string>
#include <vector>

// 函数表项：对应课程资料中的 PFINFL。
struct FunctionInfo {
    std::string name;
    std::string returnType;
    int level = 1;
    int offset = 0;
    int paramCount = 0;
    int paramStart = 0;
    int entryQuad = -1;
    int localSize = 0;
    int tempSize = 0;
};

// 形参表项：PFINFL 中 PARAM 指针指向的参数信息。
struct ParameterInfo {
    std::string functionName;
    std::string name;
    std::string type;
    std::string passMode; // vf 表示值参，vn 表示变参
    int offset = 0;
    int size = 0;
};

// 活动记录项：描述某个作用域运行时值单元的相对布局。
struct ActivationRecordItem {
    std::string scope;
    std::string name;
    std::string category;
    std::string type;
    int offset = 0;
    int size = 0;
};

// 基本块：由连续四元式构成的单入口单出口代码片段。
struct BasicBlock {
    int id = 0;
    int start = 0;
    int end = 0;
};

// 目标代码生成跟踪表项：用于展示第9章中的 B/QT/OBJ/RDL/SEM 过程。
struct TargetTraceItem {
    std::string basicBlock;
    std::string quadruple;
    std::string targetCode;
    std::string rdl;
    std::string sem;
};

// 运行层 VM 指令：统一使用 OP/A/B/C 四字段，供后续虚拟机直接执行。
struct VMInstruction {
    std::string op;
    std::string a;
    std::string b;
    std::string c;
};

// 编译结果：把前端各阶段产生的数据统一打包，方便控制台或 GUI 展示。
struct CompileResult {
    bool success = false;
    std::string errorMessage;
    std::vector<Token> tokens;
    std::vector<KeywordEntry> keywordTable;
    std::vector<DelimiterEntry> delimiterTable;
    std::vector<std::string> identifierTable;
    std::vector<std::string> constantTable;
    std::vector<ConstantEntry> constantEntries;
    std::vector<Symbol> symbols;
    std::vector<RecordType> recordTypes;
    std::vector<ArrayType> arrayTypes;
    std::vector<FunctionInfo> functionTable;
    std::vector<ParameterInfo> parameterTable;
    std::vector<ActivationRecordItem> activationRecords;
    std::vector<Quadruple> quadruples;
    std::vector<Quadruple> optimizedQuadruples;
    std::vector<BasicBlock> basicBlocks;
    std::vector<std::string> targetCodes;
    std::vector<VMInstruction> vmInstructions;
    std::vector<TargetTraceItem> targetTrace;
    bool vmFallbackUsed = false;
    double vmRuntimeMs = 0.0;
    std::string vmErrorMessage;
    std::map<std::string, double> runtimeValues;
    std::vector<std::string> parserTraceTree;
    std::vector<std::string> parserStepLog;
    std::vector<std::string> parserActionLog;
    std::vector<std::string> parserStepRuleNames;
    std::vector<std::string> parserActionRuleNames;
};

#endif
