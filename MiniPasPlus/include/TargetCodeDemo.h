
#pragma once

#include <vector>
#include "CompileResult.h"

using std::vector;

class TargetCodeDemo
{
public:
    // 根据优化后的四元式生成目标代码表行。
    vector<TargetCodeItem> generate(const vector<Quadruple> &quads,
                                    const vector<BasicBlock> &blocks);
};
