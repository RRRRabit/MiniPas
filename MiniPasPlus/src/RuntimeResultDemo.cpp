
#include "RuntimeResultDemo.h"

using std::string;
using std::vector;

// 根据符号表整理运行结果表中的变量行。
vector<RuntimeValue> RuntimeResultDemo::makePlaceholder(const vector<Symbol> &symbols)
{
    vector<RuntimeValue> values;

    for (const auto &symbol : symbols)
    {
        if (symbol.kind.find("variable") != string::npos)
        {
            values.push_back({symbol.name, ""});
        }
    }

    return values;
}
