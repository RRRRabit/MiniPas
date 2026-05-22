#ifndef MINIPASPLUS_TYPETABLE_H
#define MINIPASPLUS_TYPETABLE_H

#include <iosfwd>
#include <string>
#include <vector>

// record 字段信息：对应课程资料中结构表 RINFL 的一个“域”。
//
// 例如：
//     type person = record
//         age: integer;
//         score: real;
//     end;
//
// 会产生两个 FieldInfo：
//     age   : integer
//     score : real
struct FieldInfo {
    std::string name;
    std::string type;

    // 字段偏移量：该字段在 record 变量内部的起始位置。
    // 例如 age 的 offset=0，score 的 offset=4，表示 score 从该记录值区第 4 个字节开始存放。
    int offset = 0;
    int size = 0;
};

// record 类型信息：保存字段列表以及整个 record 占用的总字节数。
//
// record 本身不是变量，它是“类型模板”。
// 真正的变量声明例如 p: person; 会在符号表里引用这个类型名。
struct RecordType {
    std::string name;
    std::vector<FieldInfo> fields;
    int totalSize = 0;
};

// 数组类型信息：保存下界、上界、成分类型和总长度。
//
// 例如：
//     type arr = array[1..5] of integer;
//
// low=1, high=5, elementType="integer"。
struct ArrayType {
    std::string name;
    int low = 0;
    int high = 0;
    std::string elementType;
    int elementSize = 0;
    int totalSize = 0;
};

// 类型表：保存用户通过 type 声明的 record/array 类型。
//
// 基本类型 integer/real/char/boolean 不需要单独存到 vector，
// getTypeSize 会直接按名字返回固定长度。
//
// Parser 什么时候用 TypeTable？
// - 声明 record/array 类型时，把类型加入 TypeTable。
// - 声明变量时，检查类型名是否存在。
// - 使用 p.age 时，检查 age 是否真是 record 字段。
// - 使用 a[i] 时，检查 a 是否真是 array，并取出元素类型。
class TypeTable {
public:
    // 添加 record 类型，并自动计算每个字段的 offset 和 size。
    void addRecordType(const std::string& name, const std::vector<FieldInfo>& fields);

    // 查询 record 类型；找不到时返回 nullptr。
    const RecordType* findRecordType(const std::string& name) const;

    // 添加数组类型，并自动计算数组总长度。
    void addArrayType(const std::string& name, int low, int high, const std::string& elementType);

    // 查询数组类型；找不到时返回 nullptr。
    const ArrayType* findArrayType(const std::string& name) const;

    // 查询 record 字段；找不到 record 或字段时返回 nullptr。
    const FieldInfo* findField(const std::string& recordName, const std::string& fieldName) const;

    // 返回类型长度。基本类型直接返回固定长度，record 类型返回 totalSize，未知类型返回 -1。
    int getTypeSize(const std::string& typeName) const;

    // 打印 record 类型表，便于控制台调试和课程演示。
    void printTypeTable(std::ostream& out) const;

    const std::vector<RecordType>& recordTypes() const;
    const std::vector<ArrayType>& arrayTypes() const;

private:
    std::vector<RecordType> recordTypes_;
    std::vector<ArrayType> arrayTypes_;
};

#endif
