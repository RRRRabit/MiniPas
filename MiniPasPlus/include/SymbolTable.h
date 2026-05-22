// 文件说明：定义符号表结构与查询接口，服务语义检查与中间代码生成。

#pragma once

#include <string>
#include <vector>

struct Symbol {
    std::string name;      // 名字：x、arr、person、add
    std::string typeName;  // 类型：integer、real、自定义 record/array 类型名
    std::string kind;      // 类别：program、variable、function、parameter、temporary
    int address = 0;       // 符号的相对地址
    int size = 0;
};

// SymbolTable：符号表模块，管理变量/函数/参数/临时变量等符号信息。
// 在编译流程中的作用：词法与语法之后建立，语义检查和中间代码生成阶段反复查询。
class SymbolTable {
public:
    // 向符号表追加一个符号项。
    // 调用时机：变量声明、函数声明、参数声明、临时变量生成。
    void add(const Symbol& symbol)
    {
        symbols_.push_back(symbol);
    }

    // 按名字查找符号。
    // 返回值：找到则返回指针；找不到返回 nullptr。
    // 说明：使用线性查找，逻辑最直观。
    const Symbol* find(const std::string& name) const
    {
        for (const auto& symbol : symbols_)
        {
            if (symbol.name == name)
            {
                return &symbol;
            }
        }
        return nullptr;
    }

    // 判断某个名字是否已经在符号表中出现。
    bool exists(const std::string& name) const
    {
        return find(name) != nullptr;
    }

    // 返回符号表中的全部符号项。
    const std::vector<Symbol>& all() const
    {
        return symbols_;
    }

private:
    std::vector<Symbol> symbols_;
};



