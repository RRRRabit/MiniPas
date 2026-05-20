#include "Interpreter.h"
#include <cstdlib>

void Interpreter::execute(const std::vector<Quadruple>& quadruples) {
    // 每次执行前清空旧变量值，避免上一次运行影响这一次。
    values_.clear();

    for (const auto& quad : quadruples) {
        // 老版解释器只是顺序执行，不理解 if/while 跳转。
        // 因此这里只处理赋值和四则运算。
        if (quad.op == ":=") {
            values_[quad.result] = readValue(quad.arg1);
        } else if (quad.op == "+") {
            values_[quad.result] = readValue(quad.arg1) + readValue(quad.arg2);
        } else if (quad.op == "-") {
            values_[quad.result] = readValue(quad.arg1) - readValue(quad.arg2);
        } else if (quad.op == "*") {
            values_[quad.result] = readValue(quad.arg1) * readValue(quad.arg2);
        } else if (quad.op == "/") {
            values_[quad.result] = readValue(quad.arg1) / readValue(quad.arg2);
        }
    }
}

const std::map<std::string, double>& Interpreter::values() const {
    // 返回最终变量表。
    return values_;
}

double Interpreter::readValue(const std::string& text) const {
    // 如果 text 是变量名，直接返回变量当前值。
    auto it = values_.find(text);
    if (it != values_.end()) {
        return it->second;
    }

    // 否则把 text 当成数字常量。
    return std::strtod(text.c_str(), nullptr);
}
