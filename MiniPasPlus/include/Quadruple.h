#ifndef MINIPASPLUS_QUADRUPLE_H
#define MINIPASPLUS_QUADRUPLE_H

#include <iostream>
#include <string>
#include <vector>

// 四元式：op 表示操作，arg1/arg2 表示操作数，result 表示结果位置。
struct Quadruple {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

// 打印四元式序列，格式为：(op, arg1, arg2, result)。
inline void printQuadruples(const std::vector<Quadruple>& quadruples, std::ostream& out = std::cout) {
    for (std::size_t i = 0; i < quadruples.size(); ++i) {
        const auto& q = quadruples[i];
        out << i << ": (" << q.op << ", " << q.arg1
            << ", " << q.arg2 << ", " << q.result << ")\n";
    }
}

#endif
