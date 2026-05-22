#ifndef MINIPASPLUS_COMPILERBACKENDSTAGES_H
#define MINIPASPLUS_COMPILERBACKENDSTAGES_H

#include "CompileResult.h"
#include <vector>

namespace backend {

    // 对四元式按基本块做局部优化。
    //
    // 这里的优化是“保守优化”，只在一个基本块内部做：
    // - 常量折叠：2 + 3 直接变成 5；
    // - 复制传播：t1 := a 后，后面用 t1 的地方尽量改成 a；
    // - 公共子表达式消除：重复的 a + b 只算一次；
    // - 删除无用临时变量：没人再用的 t1 赋值可以删。
    std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks);

    // 根据 if/while 等控制流标记，把四元式划分成基本块。
    // 基本块的特点是：进入后会顺序执行，中间不会从别处跳入，也不会从中间跳出。
    std::vector<BasicBlock> buildBasicBlocksFromQuads(const std::vector<Quadruple>& quads);

} // namespace backend

#endif
