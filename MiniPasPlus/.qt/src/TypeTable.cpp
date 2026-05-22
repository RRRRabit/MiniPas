#include "TypeTable.h"
#include <iostream>

void TypeTable::addRecordType(const std::string& name, const std::vector<FieldInfo>& fields) {
    // 新建一条记录类型描述。
    // 例如：
    // type Point = record x: integer; y: integer; end;
    // 这里会生成 RecordType{name="Point", fields=[x,y], totalSize=8}。
    RecordType record;
    record.name = name;

    int currentOffset = 0;
    for (auto field : fields) {
        // 每个字段在记录中的偏移量从 0 开始累加。
        // 如果 x 大小是 4，y 就从 offset=4 的位置开始。
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
    // 根据类型名查找记录类型。找不到返回 nullptr。
    for (const auto& record : recordTypes_) {
        if (record.name == name) {
            return &record;
        }
    }
    return nullptr;
}

void TypeTable::addArrayType(const std::string& name, int low, int high, const std::string& elementType) {
    // 新建数组类型描述。
    // 例如 array[1..10] of integer：
    // low=1, high=10, elementSize=4, totalSize=10*4=40。
    ArrayType array;
    array.name = name;
    array.low = low;
    array.high = high;
    array.elementType = elementType;
    array.elementSize = getTypeSize(elementType);
    array.totalSize = (high - low + 1) * array.elementSize;
    arrayTypes_.push_back(array);
}

const ArrayType* TypeTable::findArrayType(const std::string& name) const {
    // 根据类型名查找数组类型。找不到返回 nullptr。
    for (const auto& array : arrayTypes_) {
        if (array.name == name) {
            return &array;
        }
    }
    return nullptr;
}

const FieldInfo* TypeTable::findField(const std::string& recordName, const std::string& fieldName) const {
    // 先找到记录类型，再在该记录的字段列表里找字段名。
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
    // 基本类型大小采用常见约定：
    // integer=4 字节，real=8 字节，char/boolean=1 字节。
    // 这些大小主要用于活动记录展示和符号表展示。
    if (typeName == "integer") {
        return 4;
    }
    if (typeName == "real") {
        return 8;
    }
    if (typeName == "char") {
        return 1;
    }
    if (typeName == "boolean") {
        return 1;
    }

    // 自定义 record 的大小就是所有字段大小之和。
    const RecordType* record = findRecordType(typeName);
    if (record != nullptr) {
        return record->totalSize;
    }

    // 自定义 array 的大小就是元素个数 * 元素大小。
    const ArrayType* array = findArrayType(typeName);
    if (array != nullptr) {
        return array->totalSize;
    }

    // -1 表示未知类型，调用方可据此报错。
    return -1;
}

void TypeTable::printTypeTable(std::ostream& out) const {
    // 调试输出：把类型表以文本形式打印出来。
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
    // 给 GUI/CompileResult 读取记录类型表。
    return recordTypes_;
}

const std::vector<ArrayType>& TypeTable::arrayTypes() const {
    // 给 GUI/CompileResult 读取数组类型表。
    return arrayTypes_;
}
