
#pragma once

#include <vector>
#include "CompileResult.h"

// BackendOptimization 是后端优化阶段。
// 输入四元式和基本块，输出优化后的四元式。
class BackendOptimization {
public:
    // 按基本块执行局部优化，返回优化后的四元式序列。
    std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads,
                                                 const std::vector<BasicBlock>& blocks);
};



