#ifndef MINIPASPLUS_VIRTUALMACHINE_H
#define MINIPASPLUS_VIRTUALMACHINE_H

#include "CompileResult.h"
#include <map>
#include <string>
#include <vector>

// 简单栈式 VM：执行结构化 VMInstruction 指令，并输出最终变量值。
class VirtualMachine {
public:
    void execute(const std::vector<VMInstruction>& program);
    const std::map<std::string, double>& values() const;

private:
    struct Frame {
        std::string returnDest;
        int returnPc = -1;
        std::map<std::string, double> locals;
    };

    std::map<std::string, double> globals_;
    std::vector<Frame> frames_;
    std::vector<double> argBuffer_;
    double retVal_ = 0.0;

    double readValue(const std::string& operand) const;
    void writeValue(const std::string& operand, double value);
    bool isNumber(const std::string& text) const;
    bool isTrue(double v) const;
    int parseLabel(const std::string& text) const;
};

#endif
