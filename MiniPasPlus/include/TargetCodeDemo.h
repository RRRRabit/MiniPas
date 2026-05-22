
#pragma once

#include <vector>
#include "CompileResult.h"

class TargetCodeDemo {
public:
    // 根据优化后的四元式生成目标代码表行。
    std::vector<TargetCodeItem> generate(const std::vector<Quadruple>& quads,
                                         const std::vector<BasicBlock>& blocks);
};



