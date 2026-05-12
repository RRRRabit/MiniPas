#include "TypeTable.h"
#include <iostream>

void TypeTable::addRecordType(const std::string& name, const std::vector<FieldInfo>& fields) {
    RecordType record;
    record.name = name;

    int currentOffset = 0;
    for (auto field : fields) {
        field.offset = currentOffset;
        field.size = getTypeSize(field.type);

        // offset 是字段相对于 record 起始地址的距离，下一个字段紧跟当前字段之后。
        currentOffset += field.size;
        record.fields.push_back(field);
    }

    record.totalSize = currentOffset;
    recordTypes_.push_back(record);
}

const RecordType* TypeTable::findRecordType(const std::string& name) const {
    for (const auto& record : recordTypes_) {
        if (record.name == name) {
            return &record;
        }
    }
    return nullptr;
}

const FieldInfo* TypeTable::findField(const std::string& recordName, const std::string& fieldName) const {
    const RecordType* record = findRecordType(recordName);
    if (record == nullptr) {
        return nullptr;
    }

    for (const auto& field : record->fields) {
        if (field.name == fieldName) {
            return &field;
        }
    }
    return nullptr;
}

int TypeTable::getTypeSize(const std::string& typeName) const {
    if (typeName == "integer") {
        return 4;
    }
    if (typeName == "real") {
        return 8;
    }
    if (typeName == "char") {
        return 1;
    }

    const RecordType* record = findRecordType(typeName);
    if (record != nullptr) {
        return record->totalSize;
    }

    return -1;
}

void TypeTable::printTypeTable(std::ostream& out) const {
    for (const auto& record : recordTypes_) {
        out << record.name << " totalSize=" << record.totalSize << '\n';
        for (const auto& field : record.fields) {
            out << field.name << ' ' << field.type
                << " offset=" << field.offset
                << " size=" << field.size << '\n';
        }
    }
}

const std::vector<RecordType>& TypeTable::recordTypes() const {
    return recordTypes_;
}
