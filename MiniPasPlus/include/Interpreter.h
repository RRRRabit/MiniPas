#ifndef MINIPASPLUS_INTERPRETER_H
#define MINIPASPLUS_INTERPRETER_H

#include "Quadruple.h"
#include <map>
#include <string>
#include <vector>

// 解释器：执行四元式，并保存最终变量值。
class Interpreter {
public:
    // 执行四元式序列。
    void execute(const std::vector<Quadruple>& quadruples);

    // 返回变量最终值表。
    const std::map<std::string, double>& values() const;

private:
    std::map<std::string, double> values_;

    double readValue(const std::string& text) const;
};

#endif

