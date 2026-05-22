#ifndef MINIPASPLUS_SYMBOLTABLE_H
#define MINIPASPLUS_SYMBOLTABLE_H

#include <string>
#include <vector>

// 符号表项：记录程序里出现过的“名字”。
//
// 编译器不能只记住变量名，还必须知道这个名字代表什么。
// 例如同样叫 x，它可能是 integer 变量，也可能是函数参数。
struct SymbolEntry {
    std::string name;     // 名字本身，例如 a、sum、person
    std::string typeName; // 类型名，例如 integer、real、自定义 record 类型名
    std::string kind;     // 种类，例如 variable、function、array variable、temporary
    int address = -1;     // 相对地址/偏移。课程展示和临时变量分配会用到
    int size = 0;         // 这个符号占用多少字节
};

using Symbol = SymbolEntry;

// 符号表：集中保存所有已声明的标识符。
//
// Parser 做语义检查时会频繁查询它：
// - 变量有没有声明？
// - 变量是什么类型？
// - 函数是否存在？
// - record / array 变量占多大？
class SymbolTable {
public:
    // 添加一个符号。Parser 读到声明时会调用。
    void add(const SymbolEntry& entry);

    // 按名字查找符号。找不到返回 nullptr。
    // const 版本用于只读检查。
    const SymbolEntry* find(const std::string& name) const;

    // 可修改版本，目前保留给需要修改符号信息的场景。
    SymbolEntry* findMutable(const std::string& name);

    // 返回整个表，方便 GUI/CLI 展示。
    const std::vector<SymbolEntry>& entries() const;

private:
    // 用 vector 保存，代码简单直观。
    // 查找是线性查找；课程设计规模很小，性能足够。
    std::vector<SymbolEntry> entries_;
};

#endif
