
#pragma once

#include <vector>
#include "CompileResult.h"

using std::vector;

// BackendOptimization 是后端优化阶段。
// 输入四元式和基本块，输出优化后的四元式。
class BackendOptimization
{
public:
    // 按基本块执行局部优化，返回优化后的四元式序列。
    vector<Quadruple> optimizeByBasicBlocks(const vector<Quadruple> &quads,
                                            const vector<BasicBlock> &blocks);
};
