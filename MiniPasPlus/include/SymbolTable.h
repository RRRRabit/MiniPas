#ifndef MINIPASPLUS_SYMBOLTABLE_H
#define MINIPASPLUS_SYMBOLTABLE_H

#include <string>
#include <vector>

// 符号表项：记录变量名、类型名和种类。
struct SymbolEntry {
    std::string name;
    std::string typeName;
    std::string kind;
    int address = -1;
    int size = 0;
};

using Symbol = SymbolEntry;

// 符号表：集中保存普通变量和 record 变量。
class SymbolTable {
public:
    void add(const SymbolEntry& entry);
    const SymbolEntry* find(const std::string& name) const;
    const std::vector<SymbolEntry>& entries() const;

private:
    std::vector<SymbolEntry> entries_;
};

#endif
