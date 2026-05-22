#ifndef MINIPASPLUS_INTERPRETER_H
#define MINIPASPLUS_INTERPRETER_H

#include "Quadruple.h"
#include <map>
#include <string>
#include <vector>

// 老版四元式解释器。
//
// 当前核心运行路径主要使用 VirtualMachine：
// 四元式 -> VM 指令 -> VM 执行。
//
// 这个 Interpreter 保留为简单备用版本，它只能处理最基础的赋值和四则运算，
// 不支持 if/while/function，所以答辩重点不要放在这里。
class Interpreter {
public:
    // 顺序执行四元式序列。
    // 注意：这里只适合没有控制流的简单四元式。
    void execute(const std::vector<Quadruple>& quadruples);

    // 返回变量最终值表。
    const std::map<std::string, double>& values() const;

private:
    // 变量名 -> 当前数值。
    std::map<std::string, double> values_;

    // 如果 text 是变量名，就从 values_ 取值；
    // 如果 text 是数字文本，就转成 double。
    double readValue(const std::string& text) const;
};

#endif
