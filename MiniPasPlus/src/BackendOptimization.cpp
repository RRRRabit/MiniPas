#include "CompilerBackendStages.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

    bool isNumberLiteral(const std::string& s)
    {
        if (s.empty()) return false;
        char* end = nullptr;
        std::strtod(s.c_str(), &end);
        return end != nullptr && *end == '\0';
    }

    bool isCommutativeOp(const std::string& op)
    { return op == "+" || op == "*"; }
    bool isArithmeticOp(const std::string& op)
    { return op == "+" || op == "-" || op == "*" || op == "/"; }
    bool isRelationalOp(const std::string& op)
    { return op == "<" || op == ">" || op == "=" || op == "!=" || op == "<=" || op == ">="; }
    bool isCalcOp(const std::string& op)
    { return isArithmeticOp(op) || isRelationalOp(op); }

    bool isTempLike(const std::string& s)
    {
        if (s.size() < 2 || s[0] != 't') return false;
        for (std::size_t i = 1; i < s.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        }
        return true;
    }

    std::string formatNumber(double value)
    {
        std::string text = std::to_string(value);
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text.empty() ? "0" : text;
    }

} // namespace

namespace backend
{

    std::vector<Quadruple> optimizeByBasicBlocks(const std::vector<Quadruple>& quads,
    const std::vector<BasicBlock>& blocks)
    {
        if (quads.empty()) return {};

        std::vector<Quadruple> optimized = quads;
        for (const auto& block : blocks)
        {
            if (block.start < 0 || block.end < block.start || block.end >= static_cast<int>(quads.size())) continue;

            std::unordered_map<std::string, std::string> alias;
            std::unordered_map<std::string, std::string> exprToValue;
            std::vector<Quadruple> blockOut;

            auto resolve = [&](const std::string& x)
            {
                if (x == "_" || x.empty() || isNumberLiteral(x)) return x;
                auto it = alias.find(x);
                int guard = 0;
                std::string cur = x;
                while (it != alias.end() && it->second != cur && guard < 32)
                {
                    cur = it->second;
                    if (isNumberLiteral(cur)) break;
                    it = alias.find(cur);
                    ++guard;
                }
                return cur;
            };

            auto killExprByVar = [&](const std::string& var)
            {
                for (auto it = exprToValue.begin(); it != exprToValue.end();)
                {
                    const std::string& key = it->first;
                    const std::string& value = it->second;
                    bool hit = key.find("|" + var + "|") != std::string::npos
                    || (key.size() >= var.size() && key.compare(key.size() - var.size(), var.size(), var) == 0)
                    || value == var;
                    if (hit) it = exprToValue.erase(it);
                    else ++it;
                }
            };

            for (int i = block.start; i <= block.end; ++i)
            {
                const Quadruple& q = quads[i];
                if (q.op == ":=")
                {
                    std::string src = resolve(q.arg1);
                    killExprByVar(q.result);
                    alias[q.result] = src;
                    if (src != q.result) blockOut.push_back({":=", src, "_", q.result});
                    continue;
                }

                if (isArithmeticOp(q.op))
                {
                    std::string left = resolve(q.arg1);
                    std::string right = resolve(q.arg2);
                    if (isCommutativeOp(q.op) && left > right) std::swap(left, right);

                    if ((q.op == "+" || q.op == "-") && right == "0")
                    {
                        killExprByVar(q.result);
                        alias[q.result] = left;
                        if (left != q.result) blockOut.push_back({":=", left, "_", q.result});
                        continue;
                    }
                    if (q.op == "*" && (left == "0" || right == "0"))
                    {
                        killExprByVar(q.result);
                        alias[q.result] = "0";
                        blockOut.push_back({":=", "0", "_", q.result});
                        continue;
                    }
                    if ((q.op == "*" && right == "1") || (q.op == "/" && right == "1"))
                    {
                        killExprByVar(q.result);
                        alias[q.result] = left;
                        if (left != q.result) blockOut.push_back({":=", left, "_", q.result});
                        continue;
                    }

                    if (isNumberLiteral(left) && isNumberLiteral(right))
                    {
                        double lv = std::strtod(left.c_str(), nullptr);
                        double rv = std::strtod(right.c_str(), nullptr);
                        double fv = 0.0;
                        if (q.op == "+") fv = lv + rv;
                        else if (q.op == "-") fv = lv - rv;
                        else if (q.op == "*") fv = lv * rv;
                        else if (q.op == "/" && rv != 0.0) fv = lv / rv;
                        else
                        {
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
                    if (found != exprToValue.end())
                    {
                        killExprByVar(q.result);
                        alias[q.result] = found->second;
                        if (found->second != q.result) blockOut.push_back({":=", found->second, "_", q.result});
                        continue;
                    }

                    killExprByVar(q.result);
                    alias[q.result] = q.result;
                    blockOut.push_back({q.op, left, right, q.result});
                    exprToValue[key] = q.result;
                    continue;
                }
                blockOut.push_back(q);
            }

            std::vector<bool> keep(blockOut.size(), true);
            std::unordered_set<std::string> live;
            for (int i = static_cast<int>(blockOut.size()) - 1; i >= 0; --i)
            {
                const Quadruple& q = blockOut[i];
                bool isAssign = (q.op == ":=");
                bool isCalc = isCalcOp(q.op);
                auto useVar = [&](const std::string& x)
                {
                    if (!x.empty() && x != "_" && !isNumberLiteral(x)
                    && !(x.size() >= 2 && x.front() == '\'' && x.back() == '\''))
                    {
                        live.insert(x);
                    }
                };
                if (isAssign || isCalc)
                {
                    bool resultTemp = isTempLike(q.result);
                    bool resultLive = (!q.result.empty() && live.find(q.result) != live.end());
                    if (resultTemp && !resultLive)
                    {
                        keep[i] = false;
                        continue;
                    }
                    if (!q.result.empty()) live.erase(q.result);
                    useVar(q.arg1);
                    useVar(q.arg2);
                }
                else
                {
                    useVar(q.arg1); useVar(q.arg2); useVar(q.result);
                }
            }

            std::vector<Quadruple> filtered;
            filtered.reserve(blockOut.size());
            for (int i = 0; i < static_cast<int>(blockOut.size()); ++i)
            {
                if (keep[i]) filtered.push_back(blockOut[i]);
            }

            int outIndex = 0;
            for (int i = block.start; i <= block.end; ++i)
            {
                if (outIndex < static_cast<int>(filtered.size())) optimized[i] = filtered[outIndex++];
                else optimized[i] = {"nop", "_", "_", "_"};
            }
        }

        std::vector<Quadruple> compacted;
        compacted.reserve(optimized.size());
        for (const auto& q : optimized) if (q.op != "nop") compacted.push_back(q);
        return compacted;
    }

    std::vector<BasicBlock> buildBasicBlocksFromQuads(const std::vector<Quadruple>& quads)
    {
        std::vector<BasicBlock> blocks;
        if (quads.empty()) return blocks;

        struct IfCtx { int ifIndex = -1; int elIndex = -1; };
        struct DoCtx { int doIndex = -1; int whIndex = -1; };
        std::set<int> leaders;
        leaders.insert(0);
        std::map<int, int> ifToIe;
        std::map<int, int> ifToEl;
        std::vector<IfCtx> ifStack;
        std::vector<int> whStack;
        std::vector<DoCtx> doStack;
        std::map<int, int> doToWe;
        std::map<int, int> weToWh;

        for (int i = 0; i < static_cast<int>(quads.size()); ++i)
        {
            const Quadruple& q = quads[i];
            if (q.op == "if") ifStack.push_back({i, -1});
            else if (q.op == "el")
            { if (!ifStack.empty()) ifStack.back().elIndex = i; }
            else if (q.op == "ie")
            {
                if (!ifStack.empty())
                {
                    ifToIe[ifStack.back().ifIndex] = i;
                    ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                    ifStack.pop_back();
                }
            }
            else if (q.op == "wh") whStack.push_back(i);
            else if (q.op == "do")
            {
                int whIndex = whStack.empty() ? -1 : whStack.back();
                doStack.push_back({i, whIndex});
            }
            else if (q.op == "we")
            {
                if (!doStack.empty())
                {
                    DoCtx ctx = doStack.back();
                    doStack.pop_back();
                    doToWe[ctx.doIndex] = i;
                    if (ctx.whIndex >= 0) weToWh[i] = ctx.whIndex;
                }
                if (!whStack.empty()) whStack.pop_back();
            }
        }

        for (int i = 0; i < static_cast<int>(quads.size()); ++i)
        {
            const Quadruple& q = quads[i];
            if (q.op == "if")
            {
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
                int falseTarget = -1;
                auto e = ifToEl.find(i);
                if (e != ifToEl.end() && e->second >= 0) falseTarget = e->second + 1;
                else
                {
                    auto f = ifToIe.find(i);
                    if (f != ifToIe.end()) falseTarget = f->second;
                }
                if (falseTarget >= 0 && falseTarget < static_cast<int>(quads.size())) leaders.insert(falseTarget);
            }
            else if (q.op == "do")
            {
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
                auto f = doToWe.find(i);
                if (f != doToWe.end())
                {
                    int falseTarget = f->second + 1;
                    if (falseTarget < static_cast<int>(quads.size())) leaders.insert(falseTarget);
                }
            }
            else if (q.op == "el")
            {
                for (const auto& pair : ifToEl)
                {
                    if (pair.second == i)
                    {
                        auto ie = ifToIe.find(pair.first);
                        if (ie != ifToIe.end()) leaders.insert(ie->second);
                        break;
                    }
                }
            }
            else if (q.op == "we")
            {
                auto w = weToWh.find(i);
                if (w != weToWh.end())
                {
                    int condEntry = w->second + 1;
                    if (condEntry < static_cast<int>(quads.size())) leaders.insert(condEntry);
                }
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
            }
        }

        std::vector<int> starts(leaders.begin(), leaders.end());
        std::sort(starts.begin(), starts.end());
        for (int i = 0; i < static_cast<int>(starts.size()); ++i)
        {
            int nextStart = (i + 1 < static_cast<int>(starts.size())) ? starts[i + 1] : static_cast<int>(quads.size());
            blocks.push_back({i, starts[i], nextStart - 1});
        }
        return blocks;
    }

} // namespace backend



