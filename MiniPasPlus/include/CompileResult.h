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
//
// Parser 读到 function 声明时，会建立一个 FunctionInfo。
// VM 生成 CALL 指令时，也要靠 entryQuad 找到函数体入口位置。
struct FunctionInfo {
    std::string name;       // 函数名，例如 add
    std::string returnType; // 返回类型，例如 integer
    int level = 1;          // 作用域层级，课程展示用
    int offset = 0;         // 偏移，课程展示用
    int paramCount = 0;     // 参数个数
    int paramStart = 0;     // 该函数第一个参数在 parameterTable 中的位置
    int entryQuad = -1;     // 函数体对应的第一条四元式下标
    int localSize = 0;      // 局部变量总大小
    int tempSize = 0;       // 临时变量总大小
};

// 形参表项：PFINFL 中 PARAM 指针指向的参数信息。
//
// 例如：
//     function add(a,b: integer): integer;
//
// 会生成两个 ParameterInfo：a 和 b。
struct ParameterInfo {
    std::string functionName; // 属于哪个函数
    std::string name;         // 参数名
    std::string type;         // 参数类型
    std::string passMode;     // vf 表示值参，vn 表示变参
    int offset = 0;           // 参数在参数区中的偏移
    int size = 0;             // 参数占用大小
};

// 基本块：由连续四元式构成的单入口单出口代码片段。
//
// 优化器按基本块处理四元式。
// 基本块内部没有复杂跳转，做常量传播、公共子表达式消除更简单。
struct BasicBlock {
    int id = 0;
    int start = 0;
    int end = 0;
};

// 运行层 VM 指令：统一使用 OP/A/B/C 四字段，供后续虚拟机直接执行。
//
// 例子：
//     {"ADD", "t1", "a", "b"} 表示 t1 = a + b
//     {"MOV", "a", "1", "_"}  表示 a = 1
struct VMInstruction {
    std::string op;
    std::string a;
    std::string b;
    std::string c;
};

// 递归下降分析树中的一个节点。
//
// 这是 GUI 展示用数据，不参与语法判断本身。
// Parser 每进入一个 parseXXX 函数，就追加一个 ParserTraceNode。
struct ParserTraceNode {
    int depth = 0;          // 调用深度，0 表示最外层 PROGRAM
    std::string rule;       // 规则名，例如 PROGRAM、STMT、EXPRESSION
    std::string text;       // GUI 上显示的文本
};

// 递归分析过程日志。
//
// rule 用来支持 GUI 点击调用树时联动高亮；
// text 是显示给人的说明，例如“匹配 Token: program”。
struct ParserLogItem {
    std::string rule;
    std::string text;
};

// 编译结果：把所有阶段产生的数据统一打包。
//
// 你可以把 CompileResult 理解成“编译报告”：
// - 成功时：里面有 Token、符号表、四元式、VM 指令、最终变量值。
// - 失败时：里面有错误阶段、错误位置、错误信息。
struct CompileResult {
    // ===== 编译是否成功，以及失败时的错误信息 =====
    bool success = false;
    std::string errorMessage;
    std::string errorStage;
    int errorLine = 0;
    int errorColumn = 0;
    std::string errorToken;
    std::string expectedToken;
    std::string errorSuggestion;
    std::vector<std::string> additionalErrors;

    // ===== 词法分析结果 =====
    std::vector<Token> tokens;
    std::vector<KeywordEntry> keywordTable;
    std::vector<DelimiterEntry> delimiterTable;
    std::vector<std::string> identifierTable;
    std::vector<std::string> constantTable;
    std::vector<ConstantEntry> constantEntries;

    // ===== 语法/语义分析结果 =====
    std::vector<Symbol> symbols;
    std::vector<RecordType> recordTypes;
    std::vector<ArrayType> arrayTypes;
    std::vector<FunctionInfo> functionTable;
    std::vector<ParameterInfo> parameterTable;

    // ===== 递归下降分析展示日志 =====
    //
    // 这些字段只给“递归分析过程”页面使用。
    // 删除它们不会影响编译执行，但 GUI 就无法展示调用树和步骤日志。
    std::vector<ParserTraceNode> parserTrace;
    std::vector<ParserLogItem> parserSteps;
    std::vector<ParserLogItem> parserActions;

    // ===== 中间代码和优化结果 =====
    std::vector<Quadruple> quadruples;
    std::vector<Quadruple> optimizedQuadruples;
    std::vector<BasicBlock> basicBlocks;

    // ===== VM 执行结果 =====
    std::vector<VMInstruction> vmInstructions;
    bool vmFallbackUsed = false;
    double vmRuntimeMs = 0.0;
    std::string vmErrorMessage;
    std::map<std::string, double> runtimeValues;
};

#endif
