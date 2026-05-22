
#pragma once

#include <vector>
#include "CompileResult.h"

using std::vector;

// 它每个作用域里会有哪些运行时单元。
class ActivationRecordDemo
{
public:
    // 根据符号表、函数表和参数表整理活动记录。
    vector<ActivationRecordItem> build(const vector<Symbol> &symbols,
                                       const vector<FunctionInfo> &functionTable,
                                       const vector<ParameterInfo> &parameterTable);
};
