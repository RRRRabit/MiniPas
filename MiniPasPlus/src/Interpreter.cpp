#include "Interpreter.h"
#include <cstdlib>

void Interpreter::execute(const std::vector<Quadruple>& quadruples) {
    values_.clear();

    for (const auto& quad : quadruples) {
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
    return values_;
}

double Interpreter::readValue(const std::string& text) const {
    auto it = values_.find(text);
    if (it != values_.end()) {
        return it->second;
    }
    return std::strtod(text.c_str(), nullptr);
}

