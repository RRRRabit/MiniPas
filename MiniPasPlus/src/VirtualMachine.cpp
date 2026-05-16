#include "VirtualMachine.h"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

void VirtualMachine::execute(const std::vector<VMInstruction>& program) {
    globals_.clear();
    frames_.clear();
    argBuffer_.clear();
    retVal_ = 0.0;

    frames_.push_back(Frame{});
    int pc = 0;
    int guard = 0;
    const int maxSteps = 2000000;

    while (pc >= 0 && pc < static_cast<int>(program.size())) {
        if (++guard > maxSteps) {
            throw std::runtime_error("VM 执行步数超限，可能存在死循环");
        }
        const VMInstruction& ins = program[pc];

        if (ins.op == "HALT") {
            break;
        } else if (ins.op == "LD") {
            writeValue(ins.a, readValue(ins.b));
            ++pc;
        } else if (ins.op == "ST") {
            writeValue(ins.b, readValue(ins.a));
            ++pc;
        } else if (ins.op == "MOV") {
            writeValue(ins.a, readValue(ins.b));
            ++pc;
        } else if (ins.op == "ADD" || ins.op == "SUB" || ins.op == "MUL" || ins.op == "DIV"
                   || ins.op == "LT" || ins.op == "GT" || ins.op == "EQ" || ins.op == "LE"
                   || ins.op == "GE" || ins.op == "NE" || ins.op == "AND" || ins.op == "OR") {
            double b = readValue(ins.b);
            double c = readValue(ins.c);
            double out = 0.0;
            if (ins.op == "ADD") out = b + c;
            else if (ins.op == "SUB") out = b - c;
            else if (ins.op == "MUL") out = b * c;
            else if (ins.op == "DIV") {
                if (c == 0.0) {
                    throw std::runtime_error("VM 运行时错误：除数为 0");
                }
                out = b / c;
            } else if (ins.op == "LT") out = (b < c) ? 1.0 : 0.0;
            else if (ins.op == "GT") out = (b > c) ? 1.0 : 0.0;
            else if (ins.op == "EQ") out = (b == c) ? 1.0 : 0.0;
            else if (ins.op == "LE") out = (b <= c) ? 1.0 : 0.0;
            else if (ins.op == "GE") out = (b >= c) ? 1.0 : 0.0;
            else if (ins.op == "NE") out = (b != c) ? 1.0 : 0.0;
            else if (ins.op == "AND") out = (isTrue(b) && isTrue(c)) ? 1.0 : 0.0;
            else if (ins.op == "OR") out = (isTrue(b) || isTrue(c)) ? 1.0 : 0.0;
            writeValue(ins.a, out);
            ++pc;
        } else if (ins.op == "NO") {
            writeValue(ins.a, isTrue(readValue(ins.b)) ? 0.0 : 1.0);
            ++pc;
        } else if (ins.op == "FJ") {
            if (!isTrue(readValue(ins.a))) {
                pc = parseLabel(ins.b);
            } else {
                ++pc;
            }
        } else if (ins.op == "TJ") {
            if (isTrue(readValue(ins.a))) {
                pc = parseLabel(ins.b);
            } else {
                ++pc;
            }
        } else if (ins.op == "JMP") {
            pc = parseLabel(ins.a);
        } else if (ins.op == "ARG") {
            argBuffer_.push_back(readValue(ins.a));
            ++pc;
        } else if (ins.op == "CALL") {
            int argc = static_cast<int>(readValue(ins.b));
            if (argc < 0 || argc > static_cast<int>(argBuffer_.size())) {
                throw std::runtime_error("VM 运行时错误：CALL 参数数量非法");
            }
            Frame frame;
            frame.returnDest = "_";
            frame.returnPc = pc + 1;
            for (int i = 0; i < argc; ++i) {
                frame.locals["ARG" + std::to_string(i)] = argBuffer_[argBuffer_.size() - argc + i];
            }
            argBuffer_.resize(argBuffer_.size() - argc);
            frames_.push_back(std::move(frame));
            pc = parseLabel(ins.a);
        } else if (ins.op == "RET") {
            retVal_ = readValue(ins.a);
            if (frames_.size() <= 1) {
                break;
            }
            int retPc = frames_.back().returnPc;
            frames_.pop_back();
            pc = retPc;
        } else if (ins.op == "MOVRET") {
            writeValue(ins.a, retVal_);
            ++pc;
        } else {
            throw std::runtime_error("VM 运行时错误：未知指令 " + ins.op);
        }
    }
}

const std::map<std::string, double>& VirtualMachine::values() const {
    return globals_;
}

double VirtualMachine::readValue(const std::string& operand) const {
    if (operand.empty() || operand == "_") {
        return 0.0;
    }
    if (operand.size() > 1 && operand[0] == 'L' && std::isdigit(static_cast<unsigned char>(operand[1]))) {
        return std::strtod(operand.substr(1).c_str(), nullptr);
    }
    if (isNumber(operand)) {
        return std::strtod(operand.c_str(), nullptr);
    }
    if (!frames_.empty()) {
        auto it = frames_.back().locals.find(operand);
        if (it != frames_.back().locals.end()) {
            return it->second;
        }
    }
    auto g = globals_.find(operand);
    return g == globals_.end() ? 0.0 : g->second;
}

void VirtualMachine::writeValue(const std::string& operand, double value) {
    if (operand.empty() || operand == "_") {
        return;
    }
    if (!frames_.empty() && operand.rfind("ARG", 0) == 0) {
        frames_.back().locals[operand] = value;
        return;
    }
    if (!frames_.empty() && operand.size() >= 1 && operand[0] == 'R') {
        frames_.back().locals[operand] = value;
        return;
    }
    globals_[operand] = value;
}

bool VirtualMachine::isNumber(const std::string& text) const {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    std::strtod(text.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool VirtualMachine::isTrue(double v) const {
    return v != 0.0;
}

int VirtualMachine::parseLabel(const std::string& text) const {
    if (text.size() > 1 && text[0] == 'L') {
        return std::atoi(text.substr(1).c_str());
    }
    throw std::runtime_error("VM 运行时错误：非法跳转标签 " + text);
}

