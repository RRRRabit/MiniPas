
#pragma once

#include <vector>
#include "CompileResult.h"

// 它每个作用域里会有哪些运行时单元。
class ActivationRecordDemo {
public:
    // 根据符号表、函数表和参数表整理活动记录。
    std::vector<ActivationRecordItem> build(const std::vector<Symbol>& symbols,
                                            const std::vector<FunctionInfo>& functionTable,
                                            const std::vector<ParameterInfo>& parameterTable);
};



