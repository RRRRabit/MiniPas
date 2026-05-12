#ifndef MINIPASPLUS_TYPETABLE_H
#define MINIPASPLUS_TYPETABLE_H

#include <iosfwd>
#include <string>
#include <vector>

// record 字段信息：对应课程资料中结构表 RINFL 的一个“域”。
struct FieldInfo {
    std::string name;
    std::string type;

    // 字段偏移量：该字段在 record 变量内部的起始位置。
    // 例如 age 的 offset=0，score 的 offset=4，表示 score 从该记录值区第 4 个字节开始存放。
    int offset = 0;
    int size = 0;
};

// record 类型信息：保存字段列表以及整个 record 占用的总字节数。
struct RecordType {
    std::string name;
    std::vector<FieldInfo> fields;
    int totalSize = 0;
};

// 类型表：保存基本类型长度和用户通过 type 声明的 record 类型。
class TypeTable {
public:
    // 添加 record 类型，并自动计算每个字段的 offset 和 size。
    void addRecordType(const std::string& name, const std::vector<FieldInfo>& fields);

    // 查询 record 类型；找不到时返回 nullptr。
    const RecordType* findRecordType(const std::string& name) const;

    // 查询 record 字段；找不到 record 或字段时返回 nullptr。
    const FieldInfo* findField(const std::string& recordName, const std::string& fieldName) const;

    // 返回类型长度。基本类型直接返回固定长度，record 类型返回 totalSize，未知类型返回 -1。
    int getTypeSize(const std::string& typeName) const;

    // 打印 record 类型表，便于控制台调试和课程演示。
    void printTypeTable(std::ostream& out) const;

    const std::vector<RecordType>& recordTypes() const;

private:
    std::vector<RecordType> recordTypes_;
};

#endif
