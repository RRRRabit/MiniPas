
#include "ActivationRecordDemo.h"

using std::string;
using std::vector;

// 根据符号表、函数表和参数表整理各作用域的活动记录。
vector<ActivationRecordItem> ActivationRecordDemo::build(
    const vector<Symbol>& symbols,
    const vector<FunctionInfo>& functionTable,
    const vector<ParameterInfo>& parameterTable)
{
    // 这个函数的目标：
    // 根据“符号表 + 函数表 + 参数表”，构造活动记录（栈帧）展示数据。
    // 输出结果中的每一项都包含：
    // - scope: 作用域（主程序/函数名）
    // - name: 该栈帧单元的名字
    // - offset: 在栈帧中的偏移位置
    // - size: 占用单元数
    vector<ActivationRecordItem> records;

    // 计算“占用单元数”。
    // 规则：
    // 1) 如果语义阶段已经给出 size（>0），直接使用；
    // 2) 否则按基础类型给默认值，避免全部写成 1。
    auto inferSizeByType = [](const string& typeName) -> int
    {
        // 这里只做“展示层”的近似大小估计，不参与真实 VM 执行。
        if (typeName == "real")
        {
            return 2;
        }
        if (typeName == "integer")
        {
            return 1;
        }
        if (typeName == "char")
        {
            return 1;
        }
        if (typeName == "boolean")
        {
            return 1;
        }
        // 其他类型（如未展开的自定义类型）给保守默认值。
        return 1;
    };

    auto symbolOccupy = [&](const Symbol& symbol) -> int
    {
        // 如果前面语义阶段已经计算出精确 size，就优先使用。
        if (symbol.size > 0)
        {
            return symbol.size;
        }
        // 否则按类型给一个默认占用。
        return inferSizeByType(symbol.typeName);
    };

    auto parameterOccupy = [&](const ParameterInfo& parameter) -> int
    {
        // vn: 变参（引用传递）按“地址+元信息”展示为 2 单元；
        // vf: 值参按类型大小展示。
        if (parameter.passMode == "vn")
        {
            return 2;
        }
        // 值参按类型占用。
        return inferSizeByType(parameter.type);
    };

    // 给一个作用域补上固定的栈头部项目。
    auto addFrameHeader = [&](const string& scope, int& offset)
    {
        records.push_back({scope, "Old SP", offset, 1});
        offset += 1;
        records.push_back({scope, "返回地址", offset, 1});
        offset += 1;
        records.push_back({scope, "Display 表", offset, 1});
        offset += 1;
    };

    int mainOffset = 0;
    addFrameHeader("主程序", mainOffset); // 添加当前作用域的栈帧头部
    for (const auto& symbol : symbols)
    {
        // 主程序作用域里，只放全局变量类条目。
        if (symbol.kind == "variable" || symbol.kind == "record variable" || symbol.kind == "array variable")
        {
            int occupy = symbolOccupy(symbol);
            records.push_back({"主程序", symbol.name, mainOffset, occupy});
            mainOffset += occupy;
        }
    }

    for (const auto& function : functionTable)
    {
        // 每个函数独立一张“展示栈帧”，offset 从 0 重新开始。
        int offset = 0;
        addFrameHeader(function.name, offset); // 添加当前作用域的栈帧头部

        for (const auto& parameter : parameterTable)
        {
            // 只收集当前函数自己的参数。
            if (parameter.functionName == function.name)
            {
                int occupy = parameterOccupy(parameter);
                records.push_back({function.name, "参数 " + parameter.name, offset, occupy});
                offset += occupy;
            }
        }

        for (const auto& symbol : symbols)
        {
            // 通过 kind 文本判断“是否属于当前函数局部变量”。
            if (symbol.kind.find("local variable of " + function.name) != string::npos)
            {
                int occupy = symbolOccupy(symbol);
                records.push_back({function.name, symbol.name, offset, occupy});
                offset += occupy;
            }
        }
    }

    return records;
}
