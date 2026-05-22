// 文件说明：定义类型表结构（record/array）与查找接口，服务类型声明和语义检查。

#pragma once

#include <string>
#include <vector>

struct FieldInfo {
    std::string name;   // 字段名
    std::string type;   // 字段类型
    int offset = 0;     // 字段偏移
};

struct RecordType {
    std::string name;                 // 记录类型名
    std::vector<FieldInfo> fields;    // 该记录的全部字段
};

struct ArrayType {
    std::string name;         // 数组类型名
    int low = 0;              // 下界
    int high = 0;             // 上界
    std::string elementType;  // 元素类型
};

class TypeTable {
public:
    // 注册一个 record 类型定义。
    void addRecord(const std::string& name, const std::vector<FieldInfo>& fields) {
        records_.push_back({name, fields});
    }

    // 注册一个 array 类型定义。
    void addArray(const std::string& name, int low, int high, const std::string& elementType) {
        arrays_.push_back({name, low, high, elementType});
    }

    // 按类型名查找 record 定义。
    const RecordType* findRecord(const std::string& name) const {
        for (const auto& record : records_) {
            if (record.name == name) return &record;
        }
        return nullptr;
    }

    // 按类型名查找 array 定义。
    const ArrayType* findArray(const std::string& name) const {
        for (const auto& array : arrays_) {
            if (array.name == name) return &array;
        }
        return nullptr;
    }

    // 查找 record 的某个字段定义。
    // 如果 record 不存在或字段不存在，返回 nullptr。
    const FieldInfo* findField(const std::string& recordName, const std::string& fieldName) const {
        const RecordType* record = findRecord(recordName);
        if (record == nullptr) return nullptr;

        for (const auto& field : record->fields) {
            if (field.name == fieldName) return &field;
        }
        return nullptr;
    }

    // 返回全部 record 类型定义。
    const std::vector<RecordType>& records() const { return records_; }

    // 返回全部 array 类型定义。
    const std::vector<ArrayType>& arrays() const { return arrays_; }

private:
    std::vector<RecordType> records_;
    std::vector<ArrayType> arrays_;
};



