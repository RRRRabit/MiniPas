// 文件说明：实现基本块局部优化流程，包括常量折叠、传播、代数化简等。

#include "BackendOptimization.h"
#include <map>
#include <set>

// 以基本块为单位执行局部优化，并返回新的四元式序列。
// 当前实现：复制传播、代数化简、公共子表达式复用。
std::vector<Quadruple> BackendOptimization::optimizeByBasicBlocks(
    const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks) {

    std::vector<Quadruple> output;

    for (const auto& block : blocks) {
        // alias 表示“谁可以替换成谁”。
        std::map<std::string, std::string> alias;

        // exprValue 记录已经算过的表达式。
        std::map<std::string, std::string> exprValue;

        for (int i = block.start; i <= block.end && i < static_cast<int>(quads.size()); ++i) {
            Quadruple q = quads[i];

            if (q.op == ":=") {
                if (alias.count(q.arg1)) q.arg1 = alias[q.arg1];
                alias[q.result] = q.arg1;
                output.push_back(q);
                continue;
            }

            if (q.op == "+" || q.op == "-" || q.op == "*" || q.op == "/") {
                if (alias.count(q.arg1)) q.arg1 = alias[q.arg1];
                if (alias.count(q.arg2)) q.arg2 = alias[q.arg2];

                // 代数化简：x + 0 = x，x * 1 = x。
                if ((q.op == "+" && q.arg2 == "0") || (q.op == "*" && q.arg2 == "1")) {
                    output.push_back({":=", q.arg1, "_", q.result});
                    continue;
                }

                // 公共子表达式：相同 op/arg1/arg2 只算一次。
                std::string key = q.op + "|" + q.arg1 + "|" + q.arg2;
                if (exprValue.count(key)) {
                    output.push_back({":=", exprValue[key], "_", q.result});
                    continue;
                }

                exprValue[key] = q.result;
                output.push_back(q);
                continue;
            }

            output.push_back(q);
        }
    }

    return output;
}



