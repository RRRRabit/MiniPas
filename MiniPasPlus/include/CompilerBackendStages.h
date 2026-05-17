#ifndef MINIPASPLUS_COMPILERBACKENDSTAGES_H
#define MINIPASPLUS_COMPILERBACKENDSTAGES_H

#include "CompileResult.h"
#include <string>
#include <vector>

struct BackendGenerationResult {
    std::vector<std::string> codes;
    std::vector<VMInstruction> instructions;
    std::vector<TargetTraceItem> trace;
};

namespace backend {

    std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks);

    std::vector<BasicBlock> buildBasicBlocksFromQuads(const std::vector<Quadruple>& quads);

    BackendGenerationResult generateTargetArtifacts(const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks,
    const std::vector<Symbol>& symbols);

} // namespace backend

#endif
