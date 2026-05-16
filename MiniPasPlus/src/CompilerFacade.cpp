#include "CompilerFacade.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "VirtualMachine.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace {

bool isNumberLiteral(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool isCommutativeOp(const std::string& op) {
    return op == "+" || op == "*";
}

bool isArithmeticOp(const std::string& op) {
    return op == "+" || op == "-" || op == "*" || op == "/";
}

bool isRelationalOp(const std::string& op) {
    return op == "<" || op == ">" || op == "=" || op == "!=" || op == "<=" || op == ">=";
}

bool isCalcOp(const std::string& op) {
    return isArithmeticOp(op) || isRelationalOp(op);
}

bool isTempLike(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') {
        return false;
    }
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

std::string formatNumber(double value) {
    std::string text = std::to_string(value);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

std::string circledNumber(int n) {
    static const char* table[] = {
        "",
        "①","②","③","④","⑤","⑥","⑦","⑧","⑨","⑩",
        "⑪","⑫","⑬","⑭","⑮","⑯","⑰","⑱","⑲","⑳",
        "㉑","㉒","㉓","㉔","㉕","㉖","㉗","㉘","㉙","㉚",
        "㉛","㉜","㉝","㉞","㉟",
        "㊱","㊲","㊳","㊴","㊵","㊶","㊷","㊸","㊹","㊺",
        "㊻","㊼","㊽","㊾","㊿"
    };
    if (n >= 1 && n <= 50) {
        return table[n];
    }
    return "[" + std::to_string(n) + "]";
}

std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads, const std::vector<BasicBlock>& blocks) {
    if (quads.empty()) {
        return {};
    }

    std::vector<Quadruple> optimized = quads;
    for (const auto& block : blocks) {
        if (block.start < 0 || block.end < block.start || block.end >= static_cast<int>(quads.size())) {
            continue;
        }

        std::unordered_map<std::string, std::string> alias;
        std::unordered_map<std::string, std::string> exprToValue;
        std::vector<Quadruple> blockOut;

        auto resolve = [&](const std::string& x) {
            if (x == "_" || x.empty()) {
                return x;
            }
            if (isNumberLiteral(x)) {
                return x;
            }
            auto it = alias.find(x);
            int guard = 0;
            std::string cur = x;
            while (it != alias.end() && it->second != cur && guard < 32) {
                cur = it->second;
                if (isNumberLiteral(cur)) {
                    break;
                }
                it = alias.find(cur);
                ++guard;
            }
            return cur;
        };

        auto killExprByVar = [&](const std::string& var) {
            for (auto it = exprToValue.begin(); it != exprToValue.end();) {
                const std::string& key = it->first;
                const std::string& value = it->second;
                bool hit = key.find("|" + var + "|") != std::string::npos
                    || (key.size() >= var.size() && key.compare(key.size() - var.size(), var.size(), var) == 0)
                    || value == var;
                if (hit) {
                    it = exprToValue.erase(it);
                } else {
                    ++it;
                }
            }
        };

        for (int i = block.start; i <= block.end; ++i) {
            const Quadruple& q = quads[i];

            if (q.op == ":=") {
                std::string src = resolve(q.arg1);
                killExprByVar(q.result);
                alias[q.result] = src;
                if (src != q.result) {
                    blockOut.push_back({":=", src, "_", q.result});
                }
                continue;
            }

            if (isArithmeticOp(q.op)) {
                std::string left = resolve(q.arg1);
                std::string right = resolve(q.arg2);

                if (isCommutativeOp(q.op) && left > right) {
                    std::swap(left, right);
                }

                if (q.op == "+" && right == "0") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "-" && right == "0") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "*" && (left == "0" || right == "0")) {
                    killExprByVar(q.result);
                    alias[q.result] = "0";
                    blockOut.push_back({":=", "0", "_", q.result});
                    continue;
                }
                if (q.op == "*" && right == "1") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }
                if (q.op == "/" && right == "1") {
                    killExprByVar(q.result);
                    alias[q.result] = left;
                    if (left != q.result) {
                        blockOut.push_back({":=", left, "_", q.result});
                    }
                    continue;
                }

                if (isNumberLiteral(left) && isNumberLiteral(right)) {
                    double lv = std::strtod(left.c_str(), nullptr);
                    double rv = std::strtod(right.c_str(), nullptr);
                    double fv = 0.0;
                    if (q.op == "+") {
                        fv = lv + rv;
                    } else if (q.op == "-") {
                        fv = lv - rv;
                    } else if (q.op == "*") {
                        fv = lv * rv;
                    } else if (q.op == "/" && rv != 0.0) {
                        fv = lv / rv;
                    } else {
                        blockOut.push_back({q.op, left, right, q.result});
                        killExprByVar(q.result);
                        alias[q.result] = q.result;
                        continue;
                    }
                    std::string folded = formatNumber(fv);
                    killExprByVar(q.result);
                    alias[q.result] = folded;
                    blockOut.push_back({":=", folded, "_", q.result});
                    continue;
                }

                std::string key = q.op + "|" + left + "|" + right;
                auto found = exprToValue.find(key);
                if (found != exprToValue.end()) {
                    killExprByVar(q.result);
                    alias[q.result] = found->second;
                    if (found->second != q.result) {
                        blockOut.push_back({":=", found->second, "_", q.result});
                    }
                    continue;
                }

                killExprByVar(q.result);
                alias[q.result] = q.result;
                blockOut.push_back({q.op, left, right, q.result});
                exprToValue[key] = q.result;
                continue;
            }

            // 其余控制流四元式直接保留。
            blockOut.push_back(q);
        }

        // 第8章思想：块内逆序活跃分析，删除无用赋值（尤其临时变量）。
        std::vector<bool> keep(blockOut.size(), true);
        std::unordered_set<std::string> live;
        for (int i = static_cast<int>(blockOut.size()) - 1; i >= 0; --i) {
            const Quadruple& q = blockOut[i];
            bool isAssign = (q.op == ":=");
            bool isCalc = isCalcOp(q.op);

            auto useVar = [&](const std::string& x) {
                if (!x.empty() && x != "_" && !isNumberLiteral(x)
                    && !(x.size() >= 2 && x.front() == '\'' && x.back() == '\'')) {
                    live.insert(x);
                }
            };

            if (isAssign || isCalc) {
                bool resultTemp = isTempLike(q.result);
                bool resultLive = (!q.result.empty() && live.find(q.result) != live.end());
                if (resultTemp && !resultLive) {
                    keep[i] = false;
                    continue;
                }
                if (!q.result.empty()) {
                    live.erase(q.result);
                }
                useVar(q.arg1);
                useVar(q.arg2);
            } else {
                useVar(q.arg1);
                useVar(q.arg2);
                useVar(q.result);
            }
        }

        std::vector<Quadruple> filtered;
        filtered.reserve(blockOut.size());
        for (int i = 0; i < static_cast<int>(blockOut.size()); ++i) {
            if (keep[i]) {
                filtered.push_back(blockOut[i]);
            }
        }

        int outIndex = 0;
        for (int i = block.start; i <= block.end; ++i) {
            if (outIndex < static_cast<int>(filtered.size())) {
                optimized[i] = filtered[outIndex++];
            } else {
                optimized[i] = {"nop", "_", "_", "_"};
            }
        }
    }

    std::vector<Quadruple> compacted;
    compacted.reserve(optimized.size());
    for (const auto& q : optimized) {
        if (q.op != "nop") {
            compacted.push_back(q);
        }
    }
    return compacted;
}

bool isTempName(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') {
        return false;
    }
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

bool isVariableOperand(const std::string& s) {
    if (s.empty() || s == "_") {
        return false;
    }
    if (isNumberLiteral(s)) {
        return false;
    }
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
        return false;
    }
    return true;
}

std::string opToTarget(const std::string& op) {
    if (op == "+") return "ADD";
    if (op == "-") return "SUB";
    if (op == "*") return "MUL";
    if (op == "/") return "DIV";
    if (op == "<") return "LT";
    if (op == ">") return "GT";
    if (op == "=") return "EQ";
    if (op == "!=") return "NE";
    if (op == "<=") return "LE";
    if (op == ">=") return "GE";
    return "";
}

std::vector<BasicBlock> buildBasicBlocksFromQuads(const std::vector<Quadruple>& quads) {
    std::vector<BasicBlock> blocks;
    if (quads.empty()) {
        return blocks;
    }

    struct IfCtx {
        int ifIndex = -1;
        int elIndex = -1;
    };
    struct DoCtx {
        int doIndex = -1;
        int whIndex = -1;
    };
    std::set<int> leaders;
    leaders.insert(0);
    std::map<int, int> ifToIe;
    std::map<int, int> ifToEl;
    std::vector<IfCtx> ifStack;
    std::vector<int> whStack;
    std::vector<DoCtx> doStack;
    std::map<int, int> doToWe;
    std::map<int, int> weToWh;

    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        const Quadruple& q = quads[i];
        if (q.op == "if") {
            ifStack.push_back({i, -1});
        } else if (q.op == "el") {
            if (!ifStack.empty()) {
                ifStack.back().elIndex = i;
            }
        } else if (q.op == "ie") {
            if (!ifStack.empty()) {
                ifToIe[ifStack.back().ifIndex] = i;
                ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                ifStack.pop_back();
            }
        } else if (q.op == "wh") {
            whStack.push_back(i);
        } else if (q.op == "do") {
            int whIndex = whStack.empty() ? -1 : whStack.back();
            doStack.push_back({i, whIndex});
        } else if (q.op == "we") {
            if (!doStack.empty()) {
                DoCtx ctx = doStack.back();
                doStack.pop_back();
                doToWe[ctx.doIndex] = i;
                if (ctx.whIndex >= 0) {
                    weToWh[i] = ctx.whIndex;
                }
            }
            if (!whStack.empty()) {
                whStack.pop_back();
            }
        }
    }

    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        const Quadruple& q = quads[i];
        if (q.op == "if") {
            if (i + 1 < static_cast<int>(quads.size())) {
                leaders.insert(i + 1);
            }
            int falseTarget = -1;
            auto e = ifToEl.find(i);
            if (e != ifToEl.end() && e->second >= 0) {
                falseTarget = e->second + 1;
            } else {
                auto f = ifToIe.find(i);
                if (f != ifToIe.end()) {
                    falseTarget = f->second;
                }
            }
            if (falseTarget >= 0 && falseTarget < static_cast<int>(quads.size())) {
                leaders.insert(falseTarget);
            }
        } else if (q.op == "do") {
            if (i + 1 < static_cast<int>(quads.size())) {
                leaders.insert(i + 1);
            }
            auto f = doToWe.find(i);
            if (f != doToWe.end()) {
                int falseTarget = f->second + 1;
                if (falseTarget < static_cast<int>(quads.size())) {
                    leaders.insert(falseTarget);
                }
            }
        } else if (q.op == "el") {
            for (const auto& pair : ifToEl) {
                if (pair.second == i) {
                    auto ie = ifToIe.find(pair.first);
                    if (ie != ifToIe.end()) {
                        leaders.insert(ie->second);
                    }
                    break;
                }
            }
        } else if (q.op == "we") {
            auto w = weToWh.find(i);
            if (w != weToWh.end()) {
                int condEntry = w->second + 1;
                if (condEntry < static_cast<int>(quads.size())) {
                    leaders.insert(condEntry);
                }
            }
            if (i + 1 < static_cast<int>(quads.size())) {
                leaders.insert(i + 1);
            }
        }
    }

    std::vector<int> starts(leaders.begin(), leaders.end());
    std::sort(starts.begin(), starts.end());
    for (int i = 0; i < static_cast<int>(starts.size()); ++i) {
        int nextStart = (i + 1 < static_cast<int>(starts.size())) ? starts[i + 1] : static_cast<int>(quads.size());
        blocks.push_back({i, starts[i], nextStart - 1});
    }
    return blocks;
}

struct QuadLiveInfo {
    bool arg1Live = false;
    bool arg2Live = false;
    bool resultLive = false;
};

std::vector<QuadLiveInfo> buildLiveness(const std::vector<Quadruple>& quads,
                                        const std::vector<BasicBlock>& blocks,
                                        const std::vector<Symbol>& symbols) {
    std::unordered_set<std::string> temporaryNames;
    for (const auto& s : symbols) {
        if (s.kind == "temporary") {
            temporaryNames.insert(s.name);
        }
    }
    auto isTemp = [&](const std::string& v) {
        return temporaryNames.find(v) != temporaryNames.end() || isTempName(v);
    };

    std::vector<QuadLiveInfo> live(quads.size());
    for (const auto& block : blocks) {
        if (block.start < 0 || block.end >= static_cast<int>(quads.size()) || block.end < block.start) {
            continue;
        }

        std::unordered_map<std::string, bool> symLive;
        for (int i = block.start; i <= block.end; ++i) {
            const Quadruple& q = quads[i];
            if (isVariableOperand(q.arg1) && symLive.find(q.arg1) == symLive.end()) {
                symLive[q.arg1] = !isTemp(q.arg1);
            }
            if (isVariableOperand(q.arg2) && symLive.find(q.arg2) == symLive.end()) {
                symLive[q.arg2] = !isTemp(q.arg2);
            }
            if (isVariableOperand(q.result) && symLive.find(q.result) == symLive.end()) {
                symLive[q.result] = !isTemp(q.result);
            }
        }

        for (int i = block.end; i >= block.start; --i) {
            const Quadruple& q = quads[i];
            if (isVariableOperand(q.result)) {
                live[i].resultLive = symLive[q.result];
                symLive[q.result] = false;
            }
            if (isVariableOperand(q.arg1)) {
                live[i].arg1Live = symLive[q.arg1];
                symLive[q.arg1] = true;
            }
            if (isVariableOperand(q.arg2)) {
                live[i].arg2Live = symLive[q.arg2];
                symLive[q.arg2] = true;
            }
        }
    }
    return live;
}

std::vector<std::string> generateTargetCodes(const std::vector<Quadruple>& quads,
                                             const std::vector<BasicBlock>& blocks,
                                             const std::vector<Symbol>& symbols) {
    return {};
}

struct TargetGenResult {
    std::vector<std::string> codes;
    std::vector<VMInstruction> instructions;
    std::vector<TargetTraceItem> trace;
};

TargetGenResult generateTargetTrace(const std::vector<Quadruple>& quads,
                                    const std::vector<BasicBlock>& blocks,
                                    const std::vector<Symbol>& symbols) {
    struct Inst {
        std::string op;
        std::string a;
        std::string b;
        std::string c;
        int target = -1; // for FJ/JMP label address
    };

    std::vector<Inst> insts;
    TargetGenResult out;
    std::vector<QuadLiveInfo> live = buildLiveness(quads, blocks, symbols);
    std::set<int> blockEnds;
    for (const auto& b : blocks) {
        blockEnds.insert(b.end);
    }
    std::vector<int> quadToBlock(quads.size(), -1);
    for (const auto& b : blocks) {
        for (int i = b.start; i <= b.end && i < static_cast<int>(quadToBlock.size()); ++i) {
            if (i >= 0) {
                quadToBlock[i] = b.id;
            }
        }
    }

    std::string rdl; // R中当前变量名，空表示空闲
    std::vector<int> sem;
    int pendingWhMark = -1;
    int nextArgSlot = 0;

    auto emit = [&](const std::string& op, const std::string& a, const std::string& b, const std::string& c, int target = -1) {
        insts.push_back({op, a, b, c, target});
        return static_cast<int>(insts.size()) - 1;
    };
    auto quadText = [&](int i, const Quadruple& q) {
        return std::to_string(i) + ": (" + q.op + ", " + q.arg1 + ", " + q.arg2 + ", " + q.result + ")";
    };
    auto instTextRange = [&](int begin, int end) {
        if (begin >= static_cast<int>(insts.size())) {
            return std::string("-");
        }
        std::string s;
        int realEnd = std::min(end, static_cast<int>(insts.size()) - 1);
        for (int i = begin; i <= realEnd; ++i) {
            const Inst& in = insts[i];
            if (!s.empty()) s += " ; ";
            std::string showA = in.a;
            if ((in.op == "FJ" || in.op == "TJ") && in.target >= 0) {
                showA = in.a;
            }
            std::string showB = in.b;
            if ((in.op == "FJ" || in.op == "TJ" || in.op == "JMP") && in.target >= 0) {
                showB = "L" + std::to_string(in.target);
            }
            s += in.op + " " + showA + ", " + showB + ", " + in.c;
        }
        return s;
    };
    auto storeReg = [&]() {
        if (!rdl.empty() && isVariableOperand(rdl)) {
            emit("ST", "R0", rdl, "_");
        }
        rdl.clear();
    };
    auto loadToReg = [&](const std::string& x) {
        if (rdl == x) {
            return;
        }
        emit("LD", "R0", x, "_");
        rdl = x;
    };

    std::map<int, int> ifToIe;
    std::map<int, int> ifToEl;
    struct IfCtx {
        int ifIndex = -1;
        int elIndex = -1;
    };
    std::vector<IfCtx> ifStack;
    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        if (quads[i].op == "if") {
            ifStack.push_back({i, -1});
        } else if (quads[i].op == "el") {
            if (!ifStack.empty()) {
                ifStack.back().elIndex = i;
            }
        } else if (quads[i].op == "ie") {
            if (!ifStack.empty()) {
                ifToIe[ifStack.back().ifIndex] = i;
                ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                ifStack.pop_back();
            }
        }
    }

    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        const Quadruple& q = quads[i];
        std::string targetOp = opToTarget(q.op);
        int instBegin = static_cast<int>(insts.size());
        int poppedAddr = -1;
        int poppedAddr2 = -1;
        int pushedAddr = -1;

        if (pendingWhMark >= 0 && q.op != "wh" && instBegin >= pendingWhMark) {
            sem.push_back(instBegin);
            pushedAddr = instBegin;
            pendingWhMark = -1;
        }

        if (!targetOp.empty()) {
            loadToReg(q.arg1);
            emit(targetOp, "R0", "R0", q.arg2);
            rdl = q.result;
        } else if (q.op == ":=") {
            loadToReg(q.arg1);
            rdl = q.result;
        } else if (q.op == "if" || q.op == "do") {
            loadToReg(q.arg1);
            int p = emit("FJ", "R0", "_", "_", -1);
            sem.push_back(p);
            pushedAddr = p;
            rdl.clear();
        } else if (q.op == "el") {
            storeReg();
            int fjPos = -1;
            if (!sem.empty()) {
                fjPos = sem.back();
                sem.pop_back();
                poppedAddr = fjPos;
            }
            int jmpPos = emit("JMP", "_", "_", "_", -1);
            if (fjPos >= 0) {
                insts[fjPos].target = jmpPos + 1;
            }
            sem.push_back(jmpPos);
            pushedAddr = jmpPos;
        } else if (q.op == "ie") {
            int ieEntry = instBegin;
            storeReg();
            if (!sem.empty()) {
                int p = sem.back();
                sem.pop_back();
                poppedAddr = p;
                insts[p].target = ieEntry;
            }
        } else if (q.op == "wh") {
            pendingWhMark = instBegin;
        } else if (q.op == "we") {
            storeReg();
            int doPos = -1;
            int whPos = -1;
            if (!sem.empty()) {
                doPos = sem.back();
                sem.pop_back();
                poppedAddr = doPos;
            }
            if (!sem.empty()) {
                whPos = sem.back();
                sem.pop_back();
                poppedAddr2 = whPos;
            }
            int j = emit("JMP", "_", "_", "_", whPos);
            if (doPos >= 0) {
                insts[doPos].target = j + 1;
            }
        } else if (q.op == "param") {
            emit("ARG", q.arg1, "_", "_");
            ++nextArgSlot;
        } else if (q.op == "call") {
            emit("CALL", q.arg1, q.arg2, "_");
            if (q.result != "_" && !q.result.empty()) {
                emit("MOVRET", "R0", "_", "_");
                emit("ST", "R0", q.result, "_");
                rdl.clear();
            }
            nextArgSlot = 0;
        } else if (q.op == "ret") {
            emit("RET", q.arg1, "_", "_");
            rdl.clear();
        }

        if (blockEnds.find(i) != blockEnds.end()) {
            storeReg();
        }

        TargetTraceItem item;
        item.basicBlock = (i < static_cast<int>(quadToBlock.size()) && quadToBlock[i] >= 0)
            ? ("B" + std::to_string(quadToBlock[i] + 1))
            : "_";
        item.quadruple = quadText(i, q);
        int instEnd = static_cast<int>(insts.size()) - 1;
        item.targetCode = std::to_string(instBegin) + ":" + std::to_string(instEnd); // 临时占位，后续统一格式化
        item.rdl = rdl.empty() ? "0" : rdl;
        if (poppedAddr >= 0 && poppedAddr2 >= 0) {
            item.sem = circledNumber(poppedAddr + 1) + "✓ " + circledNumber(poppedAddr2 + 1) + "✓";
        } else if (poppedAddr >= 0 && pushedAddr >= 0) {
            item.sem = circledNumber(poppedAddr + 1) + "✓ " + circledNumber(pushedAddr + 1);
        } else if (poppedAddr >= 0) {
            item.sem = circledNumber(poppedAddr + 1) + "✓";
        } else if (poppedAddr2 >= 0) {
            item.sem = circledNumber(poppedAddr2 + 1) + "✓";
        } else if (pushedAddr >= 0) {
            item.sem = circledNumber(pushedAddr + 1);
        } else {
            item.sem.clear();
        }
        out.trace.push_back(item);
    }

    // 用最终回填结果重写每行目标代码，避免出现 '?'
    for (auto& item : out.trace) {
        std::size_t sep = item.targetCode.find(':');
        if (sep == std::string::npos) {
            continue;
        }
        int begin = std::stoi(item.targetCode.substr(0, sep));
        int end = std::stoi(item.targetCode.substr(sep + 1));
        item.targetCode = instTextRange(begin, end);
    }

    out.codes.reserve(insts.size());
    out.instructions.reserve(insts.size());
    for (int i = 0; i < static_cast<int>(insts.size()); ++i) {
        const Inst& in = insts[i];
        std::string showB = in.b;
        if ((in.op == "FJ" || in.op == "TJ" || in.op == "JMP") && in.target >= 0) {
            showB = "L" + std::to_string(in.target);
        }
        std::string line = in.op + " " + in.a + ", " + showB + ", " + in.c;
        out.codes.push_back(line);
        out.instructions.push_back({in.op, in.a, showB, in.c});
    }
    out.codes.push_back("HALT _, _, _");
    out.instructions.push_back({"HALT", "_", "_", "_"});
    return out;
}

std::vector<VMInstruction> generateVmInstructions(const std::vector<Quadruple>& quads,
                                                  const std::vector<FunctionInfo>& functions) {
    struct PendingInst {
        VMInstruction inst;
        int jumpTargetQuad = -1;
        std::string callTargetFunc;
    };

    std::map<int, int> ifToIe;
    std::map<int, int> ifToEl;
    std::vector<std::pair<int, int>> ifStack;
    std::vector<int> whStack;
    std::vector<std::pair<int, int>> doStack;
    std::map<int, int> doToWe;
    std::map<int, int> weToWh;

    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        const Quadruple& q = quads[i];
        if (q.op == "if") {
            ifStack.push_back({i, -1});
        } else if (q.op == "el") {
            if (!ifStack.empty()) {
                ifStack.back().second = i;
            }
        } else if (q.op == "ie") {
            if (!ifStack.empty()) {
                ifToIe[ifStack.back().first] = i;
                ifToEl[ifStack.back().first] = ifStack.back().second;
                ifStack.pop_back();
            }
        } else if (q.op == "wh") {
            whStack.push_back(i);
        } else if (q.op == "do") {
            int wh = whStack.empty() ? -1 : whStack.back();
            doStack.push_back({i, wh});
        } else if (q.op == "we") {
            if (!doStack.empty()) {
                auto [doIdx, whIdx] = doStack.back();
                doStack.pop_back();
                doToWe[doIdx] = i;
                if (whIdx >= 0) {
                    weToWh[i] = whIdx;
                }
            }
            if (!whStack.empty()) {
                whStack.pop_back();
            }
        }
    }

    std::vector<PendingInst> pending;
    std::vector<int> quadToInst(quads.size(), -1);

    auto emit = [&](const VMInstruction& inst, int jumpQuad = -1, const std::string& callFunc = "") {
        pending.push_back({inst, jumpQuad, callFunc});
    };

    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        quadToInst[i] = static_cast<int>(pending.size());
        const Quadruple& q = quads[i];
        std::string vmOp = opToTarget(q.op);
        if (!vmOp.empty()) {
            emit({vmOp, q.result, q.arg1, q.arg2});
        } else if (q.op == ":=") {
            emit({"MOV", q.result, q.arg1, "_"});
        } else if (q.op == "if") {
            int falseTarget = -1;
            auto el = ifToEl.find(i);
            if (el != ifToEl.end() && el->second >= 0) {
                falseTarget = el->second + 1;
            } else {
                auto ie = ifToIe.find(i);
                if (ie != ifToIe.end()) {
                    falseTarget = ie->second;
                }
            }
            emit({"FJ", q.arg1, "_", "_"}, falseTarget);
        } else if (q.op == "el") {
            int ieTarget = -1;
            for (const auto& [ifIdx, elIdx] : ifToEl) {
                if (elIdx == i) {
                    auto itIe = ifToIe.find(ifIdx);
                    if (itIe != ifToIe.end()) {
                        ieTarget = itIe->second;
                    }
                    break;
                }
            }
            emit({"JMP", "_", "_", "_"}, ieTarget);
        } else if (q.op == "do") {
            int falseTarget = -1;
            auto we = doToWe.find(i);
            if (we != doToWe.end()) {
                falseTarget = we->second + 1;
            }
            emit({"FJ", q.arg1, "_", "_"}, falseTarget);
        } else if (q.op == "we") {
            auto wh = weToWh.find(i);
            int condEntry = (wh == weToWh.end()) ? -1 : (wh->second + 1);
            emit({"JMP", "_", "_", "_"}, condEntry);
        } else if (q.op == "param") {
            emit({"ARG", q.arg1, "_", "_"});
        } else if (q.op == "call") {
            emit({"CALL", "_", q.arg2, "_"}, -1, q.arg1);
            if (q.result != "_" && !q.result.empty()) {
                emit({"MOVRET", q.result, "_", "_"});
            }
        } else if (q.op == "ret") {
            emit({"RET", q.arg1, "_", "_"});
        }
    }

    std::unordered_map<std::string, int> funcEntryInst;
    for (const auto& f : functions) {
        if (f.entryQuad >= 0 && f.entryQuad < static_cast<int>(quadToInst.size())) {
            int instIdx = quadToInst[f.entryQuad];
            if (instIdx >= 0) {
                funcEntryInst[f.name] = instIdx;
            }
        }
    }

    int funcCount = static_cast<int>(functions.size());
    int seenRet = 0;
    int mainEntryQuad = 0;
    for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
        if (quads[i].op == "ret") {
            ++seenRet;
            if (seenRet == funcCount) {
                mainEntryQuad = i + 1;
                break;
            }
        }
    }
    int mainEntryInst = (mainEntryQuad >= 0 && mainEntryQuad < static_cast<int>(quadToInst.size()))
        ? quadToInst[mainEntryQuad]
        : 0;

    std::vector<VMInstruction> result;
    result.reserve(pending.size() + 2);
    result.push_back({"JMP", "L" + std::to_string(mainEntryInst + 1), "_", "_"});
    for (const auto& p : pending) {
        VMInstruction inst = p.inst;
        if (p.jumpTargetQuad >= 0) {
            int targetLabel = -1;
            if (p.jumpTargetQuad < static_cast<int>(quadToInst.size())) {
                int t = quadToInst[p.jumpTargetQuad];
                if (t >= 0) {
                    targetLabel = t + 1; // +1: result[0] 是入口跳转
                }
            } else if (p.jumpTargetQuad == static_cast<int>(quadToInst.size())) {
                // 跳转到“最后一条四元式之后”，对应 VM 末尾 HALT。
                targetLabel = static_cast<int>(pending.size()) + 1;
            }
            if (targetLabel >= 0) {
                if (inst.op == "JMP") {
                    inst.a = "L" + std::to_string(targetLabel);
                } else {
                    inst.b = "L" + std::to_string(targetLabel);
                }
            }
        }
        if (inst.op == "CALL") {
            auto it = funcEntryInst.find(p.callTargetFunc);
            if (it != funcEntryInst.end()) {
                inst.a = "L" + std::to_string(it->second + 1);
            } else {
                inst.a = "L0";
            }
        }
        result.push_back(inst);
    }
    result.push_back({"HALT", "_", "_", "_"});
    return result;
}

} // namespace

CompileResult CompilerFacade::compileAndRun(const std::string& sourceCode) {
    CompileResult finalResult;

    try {
        // 1. 词法分析：生成 Token 序列、标识符表和常数表。
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();

        finalResult.tokens = tokens;
        finalResult.keywordTable = lexer.getKeywordTable();
        finalResult.delimiterTable = lexer.getDelimiterTable();
        finalResult.identifierTable = lexer.getIdentifierTable();
        finalResult.constantTable = lexer.getConstantTable();
        finalResult.constantEntries = lexer.getConstantEntries();

        for (const auto& token : tokens) {
            if (token.type == TokenType::ERROR) {
                finalResult.success = false;
                finalResult.errorMessage = "词法错误: line " + std::to_string(token.line)
                    + ", column " + std::to_string(token.column)
                    + " 非法字符 '" + token.lexeme + "'";
                return finalResult;
            }
        }

        // 2. 语法分析：递归下降分析，同时填写表并生成四元式。
        Parser parser(tokens);
        CompileResult parseResult = parser.parse();

        finalResult.symbols = parseResult.symbols;
        finalResult.recordTypes = parseResult.recordTypes;
        finalResult.arrayTypes = parseResult.arrayTypes;
        finalResult.functionTable = parseResult.functionTable;
        finalResult.parameterTable = parseResult.parameterTable;
        finalResult.activationRecords = parseResult.activationRecords;
        finalResult.quadruples = parseResult.quadruples;
        finalResult.basicBlocks = parseResult.basicBlocks;
        finalResult.optimizedQuadruples = optimizeByBasicBlocks(finalResult.quadruples, finalResult.basicBlocks);
        std::vector<BasicBlock> optimizedBlocks = buildBasicBlocksFromQuads(finalResult.optimizedQuadruples);
        TargetGenResult tg = generateTargetTrace(finalResult.optimizedQuadruples, optimizedBlocks, finalResult.symbols);
        finalResult.targetCodes = std::move(tg.codes);
        finalResult.vmInstructions = generateVmInstructions(finalResult.quadruples, finalResult.functionTable);
        finalResult.targetTrace = std::move(tg.trace);

        // 3. 运行层执行：优先使用 VM，失败时回退到四元式解释器。
        try {
            auto vmStart = std::chrono::steady_clock::now();
            VirtualMachine vm;
            vm.execute(finalResult.vmInstructions);
            auto vmEnd = std::chrono::steady_clock::now();
            finalResult.vmRuntimeMs = std::chrono::duration<double, std::milli>(vmEnd - vmStart).count();
            finalResult.vmUsed = true;
            finalResult.runtimeValues = vm.values();
            finalResult.vmErrorMessage.clear();
        } catch (const std::exception& vmEx) {
            finalResult.vmUsed = false;
            finalResult.vmFallbackUsed = true;
            finalResult.vmErrorMessage = vmEx.what();
            Interpreter interpreter;
            interpreter.execute(finalResult.quadruples);
            finalResult.runtimeValues = interpreter.values();
        }

        finalResult.success = true;
    } catch (const std::exception& ex) {
        finalResult.success = false;
        finalResult.errorMessage = ex.what();
    }

    return finalResult;
}
