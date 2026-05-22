
#pragma once

#include <vector>
#include "CompileResult.h"

// 这里不实现 VM，只给出运行结果表的数据形态。
class RuntimeResultDemo {
public:
    // 根据符号表整理运行结果表行。
    std::vector<RuntimeValue> makePlaceholder(const std::vector<Symbol>& symbols);
};



