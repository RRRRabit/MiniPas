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

    // 判断一个字符串是不是数字常量。
    // 这里用 strtod，而不是自己逐字符判断，是因为它同时支持整数和小数：
    // 例如 "12"、"3.14" 都会被认为是数字。
    bool isNumberLiteral(const std::string& s)
    {
        if (s.empty()) return false;
        char* end = nullptr;
        std::strtod(s.c_str(), &end);
        return end != nullptr && *end == '\0';
    }

    // 后端优化只处理几类简单运算。
    // 加法和乘法满足交换律：a + b 和 b + a 结果一样，所以可以统一表达式形式。
    bool isCommutativeOp(const std::string& op)
    { return op == "+" || op == "*"; }
    bool isArithmeticOp(const std::string& op)
    { return op == "+" || op == "-" || op == "*" || op == "/"; }
    bool isRelationalOp(const std::string& op)
    { return op == "<" || op == ">" || op == "=" || op == "!=" || op == "<=" || op == ">="; }
    bool isCalcOp(const std::string& op)
    { return isArithmeticOp(op) || isRelationalOp(op); }

    // 临时变量由 Parser 生成，一般长这样：t1、t2、t3。
    // 优化器可以安全删除“不再被使用的临时变量赋值”，但不能随便删除普通变量赋值。
    // 例如 x := 1 是用户变量，删除会改变最终结果；t1 := 1 如果没人用，就可以删除。
    bool isTempLike(const std::string& s)
    {
        if (s.size() < 2 || s[0] != 't') return false;
        for (std::size_t i = 1; i < s.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        }
        return true;
    }

    // 常量折叠后要把 double 转回字符串。
    // std::to_string(3.0) 会得到 "3.000000"，这里把多余的 0 去掉，方便展示和后续比较。
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

        // optimized 先复制原四元式，之后逐个基本块替换成优化后的版本。
        // 基本块之间不做激进优化，原因是跨越 if/while 后变量值可能来自不同路径，新手项目里保持保守更安全。
        std::vector<Quadruple> optimized = quads;
        for (const auto& block : blocks)
        {
            if (block.start < 0 || block.end < block.start || block.end >= static_cast<int>(quads.size())) continue;

            // alias：记录“变量现在等价于谁”。
            // 例如 t1 := a 之后，可以记 alias["t1"] = "a"；
            // 后面用 t1 时，就可以直接替换成 a，这叫复制传播。
            std::unordered_map<std::string, std::string> alias;

            // exprToValue：记录某个表达式已经算过，结果存在哪个变量里。
            // 例如 t1 := a + b 后，记录 "+|a|b" -> "t1"；
            // 后面如果又出现 t2 := a + b，就可以直接改成 t2 := t1。
            // 这叫公共子表达式消除。
            std::unordered_map<std::string, std::string> exprToValue;
            std::vector<Quadruple> blockOut;

            // resolve 用来追踪 alias 链。
            // 例如 t2 := t1，t1 := a，那么 resolve("t2") 最终希望得到 "a"。
            // guard 是保护措施，避免极端情况下 alias 形成环导致死循环。
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

            // 如果某个变量被重新赋值，所有依赖这个变量的旧表达式缓存都不再可靠。
            // 例如之前有 t1 := a + b；后来 a := 10；
            // 此时再看到 a + b，不能复用旧的 t1，因为 a 的值已经变了。
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
                    // 赋值语句的主要优化是复制传播：
                    // 如果右侧是 t1，而 t1 已知等价于 a，就把右侧改成 a。
                    std::string src = resolve(q.arg1);
                    killExprByVar(q.result);
                    alias[q.result] = src;
                    if (src != q.result) blockOut.push_back({":=", src, "_", q.result});
                    continue;
                }

                if (isArithmeticOp(q.op))
                {
                    // 先把左右操作数替换成已经知道的等价值。
                    std::string left = resolve(q.arg1);
                    std::string right = resolve(q.arg2);

                    // 对加法/乘法，把 a+b 和 b+a 统一为同一种 key，便于公共子表达式消除。
                    if (isCommutativeOp(q.op) && left > right) std::swap(left, right);

                    // 代数恒等式简化：
                    // x + 0 = x，x - 0 = x。
                    if ((q.op == "+" || q.op == "-") && right == "0")
                    {
                        killExprByVar(q.result);
                        alias[q.result] = left;
                        if (left != q.result) blockOut.push_back({":=", left, "_", q.result});
                        continue;
                    }

                    // x * 0 = 0。
                    if (q.op == "*" && (left == "0" || right == "0"))
                    {
                        killExprByVar(q.result);
                        alias[q.result] = "0";
                        blockOut.push_back({":=", "0", "_", q.result});
                        continue;
                    }

                    // x * 1 = x，x / 1 = x。
                    if ((q.op == "*" && right == "1") || (q.op == "/" && right == "1"))
                    {
                        killExprByVar(q.result);
                        alias[q.result] = left;
                        if (left != q.result) blockOut.push_back({":=", left, "_", q.result});
                        continue;
                    }

                    // 常量折叠：
                    // 如果左右两边都是数字，编译阶段就能算出结果，不必等运行时再算。
                    // 例如 2 + 3 直接变成 5。
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
                            // 除数为 0 时不在优化阶段处理，保留给虚拟机运行时报错。
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

                    // 公共子表达式消除：
                    // 同一个基本块内，如果同一个表达式已经算过，就复用之前的结果。
                    std::string key = q.op + "|" + left + "|" + right;
                    auto found = exprToValue.find(key);
                    if (found != exprToValue.end())
                    {
                        killExprByVar(q.result);
                        alias[q.result] = found->second;
                        if (found->second != q.result) blockOut.push_back({":=", found->second, "_", q.result});
                        continue;
                    }

                    // 不能继续简化时，保留这条计算，并把它记录到 exprToValue。
                    killExprByVar(q.result);
                    alias[q.result] = q.result;
                    blockOut.push_back({q.op, left, right, q.result});
                    exprToValue[key] = q.result;
                    continue;
                }
                blockOut.push_back(q);
            }

            // 死临时变量删除。
            // 思路：从后往前看，如果 t1 被赋值后再也没有被使用，那么这条 t1 := ... 可以删除。
            // 注意这里只删临时变量 t1/t2，不删用户变量 x/y，避免改变程序最终输出。
            std::vector<bool> keep(blockOut.size(), true);
            std::unordered_set<std::string> live;
            for (int i = static_cast<int>(blockOut.size()) - 1; i >= 0; --i)
            {
                const Quadruple& q = blockOut[i];
                bool isAssign = (q.op == ":=");
                bool isCalc = isCalcOp(q.op);
                auto useVar = [&](const std::string& x)
                {
                    // 数字、空占位符、字符常量都不算变量。
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
                    // 给临时变量赋值，但后面没人读它，说明这条语句没有实际作用。
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
                    // 控制流、函数调用等语句不能简单删除，只记录它们使用了哪些操作数。
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
                // 一个基本块优化后可能变短。
                // 为了不立刻破坏其他基本块的下标，先用 nop 占位，最后统一删除 nop。
                if (outIndex < static_cast<int>(filtered.size())) optimized[i] = filtered[outIndex++];
                else optimized[i] = {"nop", "_", "_", "_"};
            }
        }

        // 删除所有占位 nop，得到最终优化后的四元式序列。
        std::vector<Quadruple> compacted;
        compacted.reserve(optimized.size());
        for (const auto& q : optimized) if (q.op != "nop") compacted.push_back(q);
        return compacted;
    }

    std::vector<BasicBlock> buildBasicBlocksFromQuads(const std::vector<Quadruple>& quads)
    {
        std::vector<BasicBlock> blocks;
        if (quads.empty()) return blocks;

        // 基本块划分的目标：
        // 1. 找到所有“入口语句”（leader）；
        // 2. 两个入口之间就是一个基本块。
        //
        // 这里的四元式用结构化标记表示控制流：
        // if ... el ... ie 表示 if/else/end if；
        // wh ... do ... we 表示 while/do/while end。
        // 所以先要配对这些控制流标记，才能知道跳转目标在哪里。
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
            // 第一遍：用栈配对 if/else/end 和 while/do/end。
            // 栈的作用是支持嵌套结构，例如 if 里面还有 if。
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
                // if 后面的第一条语句是 true 分支入口。
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
                // do 后面的第一条语句是 while 循环体入口。
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
                auto f = doToWe.find(i);
                if (f != doToWe.end())
                {
                    // 条件为 false 时，跳到循环结束后的第一条语句。
                    int falseTarget = f->second + 1;
                    if (falseTarget < static_cast<int>(quads.size())) leaders.insert(falseTarget);
                }
            }
            else if (q.op == "el")
            {
                // true 分支执行完以后，会跳到 ie 位置，所以 ie 也是一个入口。
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
                // while 结束处会跳回循环条件，循环条件位置也是入口。
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
            // 第 i 个入口到下一个入口前一条，就是一个基本块。
            int nextStart = (i + 1 < static_cast<int>(starts.size())) ? starts[i + 1] : static_cast<int>(quads.size());
            blocks.push_back({i, starts[i], nextStart - 1});
        }
        return blocks;
    }

} // namespace backend



