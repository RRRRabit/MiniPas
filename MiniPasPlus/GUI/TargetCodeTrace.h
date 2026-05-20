#ifndef MINIPASPLUS_TARGET_CODE_TRACE_H
#define MINIPASPLUS_TARGET_CODE_TRACE_H

#include "CompileResult.h"
#include <string>
#include <vector>

struct TargetTraceItem {
    // 当前四元式属于哪个基本块。
    std::string basicBlock;

    // 原始四元式文本。
    std::string quadruple;

    // 生成出的目标代码文本。
    std::string targetCode;

    // RDL/SEM 是课程设计里用于展示目标代码生成过程的辅助列。
    // 它们只用于 GUI 展示，不影响真实 VM 执行。
    std::string rdl;
    std::string sem;
};

struct BackendGenerationResult {
    // 给 GUI 展示的目标代码字符串。
    std::vector<std::string> codes;

    // 真正可以交给 VirtualMachine 执行的 VM 指令。
    std::vector<VMInstruction> instructions;

    // 每条四元式生成目标代码时的展示轨迹。
    std::vector<TargetTraceItem> trace;
};

namespace backend {

// 生成 GUI 后端展示需要的所有材料：
// - codes：目标代码文本；
// - instructions：VM 指令；
// - trace：每个基本块、四元式和目标代码的对应关系。
BackendGenerationResult generateTargetArtifacts(const std::vector<Quadruple>& quads,
                                                const std::vector<BasicBlock>& blocks,
                                                const std::vector<Symbol>& symbols);

} // namespace backend

#endif
