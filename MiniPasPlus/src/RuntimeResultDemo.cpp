
#include "RuntimeResultDemo.h"

// 根据符号表整理运行结果表中的变量行。
std::vector<RuntimeValue> RuntimeResultDemo::makePlaceholder(const std::vector<Symbol>& symbols) {
    std::vector<RuntimeValue> values;

    for (const auto& symbol : symbols) {
        if (symbol.kind.find("variable") != std::string::npos) {
            values.push_back({symbol.name, ""});
        }
    }

    return values;
}



