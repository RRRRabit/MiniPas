#include "SymbolTable.h"

void SymbolTable::add(const SymbolEntry& entry) {
    // 符号表采用最简单的 vector 顺序存储。
    // 对课程设计规模来说，查找速度已经足够，代码也比哈希表更容易看懂。
    entries_.push_back(entry);
}

const SymbolEntry* SymbolTable::find(const std::string& name) const {
    // 按名字顺序查找符号。
    // 返回指针是为了避免复制 SymbolEntry；找不到就返回 nullptr。
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

SymbolEntry* SymbolTable::findMutable(const std::string& name) {
    // 可修改版本的查找。
    // Parser 在补充符号的类型、偏移、大小时，需要拿到非 const 指针。
    for (auto& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<SymbolEntry>& SymbolTable::entries() const {
    // 返回整张符号表，主要用于 GUI 表格展示和 CompileResult 汇总。
    return entries_;
}
