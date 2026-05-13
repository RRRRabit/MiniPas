#include "SymbolTable.h"

void SymbolTable::add(const SymbolEntry& entry) {
    entries_.push_back(entry);
}

const SymbolEntry* SymbolTable::find(const std::string& name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

SymbolEntry* SymbolTable::findMutable(const std::string& name) {
    for (auto& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<SymbolEntry>& SymbolTable::entries() const {
    return entries_;
}
