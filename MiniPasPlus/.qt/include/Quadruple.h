#ifndef MINIPASPLUS_QUADRUPLE_H
#define MINIPASPLUS_QUADRUPLE_H

#include <iostream>
#include <string>
#include <vector>

// 四元式：编译器的中间代码。
//
// 高级语言语句通常比较复杂，例如：
//     c := a + b * 4
//
// 编译器会把它拆成几条简单的四元式：
//     (*, b, 4, t1)
//     (+, a, t1, t2)
//     (:=, t2, _, c)
//
// 每条四元式由 4 个字段组成：
// op     ：操作符，比如 +、*、:=、if、call。
// arg1   ：第一个操作数。
// arg2   ：第二个操作数；没有时用 "_"。
// result ：结果放在哪里；例如临时变量 t1 或目标变量 c。
struct Quadruple {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

// 打印四元式序列，格式为：(op, arg1, arg2, result)。
// 这是调试/展示辅助函数，不影响编译逻辑。
inline void printQuadruples(const std::vector<Quadruple>& quadruples, std::ostream& out = std::cout) {
    for (std::size_t i = 0; i < quadruples.size(); ++i) {
        const auto& q = quadruples[i];
        out << i << ": (" << q.op << ", " << q.arg1
            << ", " << q.arg2 << ", " << q.result << ")\n";
    }
}

#endif
