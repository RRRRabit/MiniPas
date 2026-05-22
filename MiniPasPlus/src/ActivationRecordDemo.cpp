
#include "ActivationRecordDemo.h"

using std::string;
using std::vector;

// 根据符号表、函数表和参数表整理各作用域的活动记录。
vector<ActivationRecordItem> ActivationRecordDemo::build(
    const vector<Symbol>& symbols,
    const vector<FunctionInfo>& functionTable,
    const vector<ParameterInfo>& parameterTable)
{

    vector<ActivationRecordItem> records;

    // 给一个作用域补上固定的栈帧头部项目。
    auto addFrameHeader = [&](const string& scope, int& offset)
    {
        records.push_back({scope, "Old SP", offset++, 1});
        records.push_back({scope, "返回地址", offset++, 1});
        records.push_back({scope, "Display 表", offset++, 1});
    };

    int mainOffset = 0;
    addFrameHeader("主程序", mainOffset); // 添加当前作用域的栈帧头部
    for (const auto& symbol : symbols)
    {
        if (symbol.kind == "variable" || symbol.kind == "record variable" || symbol.kind == "array variable")
        {
            records.push_back({"主程序", symbol.name, mainOffset++, 1});
        }
    }

    for (const auto& function : functionTable)
    {
        int offset = 0;
        addFrameHeader(function.name, offset); // 添加当前作用域的栈帧头部

        for (const auto& parameter : parameterTable)
        {
            if (parameter.functionName == function.name)
            {
                records.push_back({function.name, "参数 " + parameter.name, offset++, 1});
            }
        }

        for (const auto& symbol : symbols)
        {
            if (symbol.kind.find("local variable of " + function.name) != string::npos)
            {
                records.push_back({function.name, symbol.name, offset++, 1});
            }
        }
    }

    return records;
}



