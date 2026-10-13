#include "AI/ZetaAgentTools.h"

#include "MathUtils/RiemannZeta.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace Potato {
namespace AI {
namespace ZetaTools {

using namespace Potato::RZ;
using Params = std::unordered_map<std::string, std::string>;

// ---- 參數解析 helpers（與 RenderAgentTools 同風格）----

static double PD(const Params& p, const char* name,
                 double def = 0.0) {
    auto it = p.find(name);
    return it != p.end() ? std::atof(it->second.c_str()) : def;
}
static int PI(const Params& p, const char* name, int def = 0) {
    auto it = p.find(name);
    return it != p.end() ? std::atoi(it->second.c_str()) : def;
}

static ToolResult Ok(const std::string& out) {
    ToolResult r;
    r.success = true;
    r.output = out;
    return r;
}
static ToolResult Err(const std::string& msg) {
    ToolResult r;
    r.success = false;
    r.error = msg;
    return r;
}

static std::string CStr(const C& v) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%.12g%+.12gi",
                  v.real(), v.imag());
    return buf;
}
static std::string FStr(double v) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.12g", v);
    return buf;
}

static Tool MakeTool(const char* name, const char* desc,
                     std::initializer_list<
                         std::pair<const char*, const char*>>
                         params,
                     ToolFunction fn) {
    Tool t;
    t.name = name;
    t.description = desc;
    for (auto& pr : params)
        t.parameters[pr.first] = pr.second;
    t.function = std::move(fn);
    return t;
}

size_t RegisterZetaTools(ToolRegistry& registry) {
    size_t n = 0;
    auto add = [&](Tool t) {
        registry.RegisterTool(std::move(t));
        ++n;
    };

    // ---- ζ/L 求值 ----
    add(MakeTool(
        "zeta_eval",
        "ζ(σ+it) Euler-Maclaurin 求值（n=截斷項數,預設200;"
        "高 t 需 n≳4t）",
        {{"sigma", "實部"}, {"t", "虛部"}, {"n", "截斷項數"}},
        [](const Params& p) -> ToolResult {
            const C s(PD(p, "sigma", 0.5), PD(p, "t"));
            const C v = ZetaEMN(s, PI(p, "n", 200));
            return Ok(CStr(v) + " |zeta|=" + FStr(std::abs(v)));
        }));

    add(MakeTool(
        "zeta_eta_eval",
        "η(s)=Σ(−1)^{n−1}n^{−s} Euler 變換求值——"
        "獨立路徑,可與 (1−2^{1−s})ζ 交叉驗證",
        {{"sigma", "實部"}, {"t", "虛部"}},
        [](const Params& p) -> ToolResult {
            const C s(PD(p, "sigma", 0.5), PD(p, "t"));
            const C v = EtaEuler(s);
            return Ok(CStr(v) + " |eta|=" + FStr(std::abs(v)));
        }));

    add(MakeTool(
        "zeta_xi_eval",
        "ξ(s)=½s(s−1)π^{−s/2}Γ(s/2)ζ(s) 整函數——"
        "線上實值；零點=非平凡零點",
        {{"sigma", "實部"}, {"t", "虛部"}, {"n", "截斷項數"}},
        [](const Params& p) -> ToolResult {
            const C s(PD(p, "sigma", 0.5), PD(p, "t"));
            const C v = Xi(s, PI(p, "n", 200));
            return Ok(CStr(v) + " |xi|=" + FStr(std::abs(v)));
        }));

    add(MakeTool(
        "zeta_beta_eval",
        "Dirichlet β(s)=L(s,χ₄)=Σ(−1)^n(2n+1)^{−s}"
        "（Euler 變換）",
        {{"sigma", "實部"}, {"t", "虛部"}},
        [](const Params& p) -> ToolResult {
            const C s(PD(p, "sigma", 0.5), PD(p, "t"));
            const C v = BetaDirichlet(s);
            return Ok(CStr(v) + " |beta|=" + FStr(std::abs(v)));
        }));

    add(MakeTool(
        "zeta_chi5_eval",
        "L(s,χ₅) mod 5 實特徵（Hurwitz 分解）——"
        "Dedekind ζ_{Q(√5)}=ζ·L(χ₅)",
        {{"sigma", "實部"}, {"t", "虛部"}},
        [](const Params& p) -> ToolResult {
            const C s(PD(p, "sigma", 0.5), PD(p, "t"));
            // s=1 是 Hurwitz 單極點：Σχ₅(r)=0 極點相消,
            // 走極點扣除式 −Σχ₅(r)ψ(r/5)/5
            if (std::abs(s - C(1.0, 0.0)) < 1e-9) {
                const double v = Chi5LAt1();
                return Ok(FStr(v) + " |L|=" +
                          FStr(std::fabs(v)));
            }
            const C v = Chi5L(s);
            return Ok(CStr(v) + " |L|=" + FStr(std::abs(v)));
        }));

    // ---- 零點/計數 ----
    add(MakeTool(
        "zeta_zeros_scan",
        "臨界線零點掃描：真 Z(t)=e^{iθ}ζ(½+it) 變號→"
        "二分求精。回傳 count 與 roots 清單（截斷 t_hi≤1000）",
        {{"t_lo", "下界"}, {"t_hi", "上界≤1000"},
         {"step", "掃描步距(預設0.02)"}},
        [](const Params& p) -> ToolResult {
            const double lo = PD(p, "t_lo", 10.0);
            const double hi = PD(p, "t_hi", 100.0);
            const double step = PD(p, "step", 0.02);
            if (hi <= lo || hi > 1000.0 || step <= 0.0)
                return Err("need 0<=t_lo<t_hi<=1000, step>0");
            // N≳4t 抗 EM 漸近發散
            const int n =
                std::max(200, static_cast<int>(4.2 * hi));
            auto zhi = [n](double t) { return RotZetaHi(t, n); };
            const std::vector<double> roots =
                ScanRoots(zhi, lo, hi, step);
            const long expected = std::lround(
                RSTheta(hi) / kPi + 1.0 -
                (RSTheta(lo) / kPi + 1.0));
            std::string out =
                "count=" + std::to_string(roots.size()) +
                " expected~=" + std::to_string(expected) +
                " roots=";
            char buf[32];
            const size_t cap =
                roots.size() < 80 ? roots.size() : 80;
            for (size_t i = 0; i < cap; ++i) {
                if (i) out += ",";
                std::snprintf(buf, sizeof(buf), "%.5f",
                              roots[i]);
                out += buf;
            }
            if (roots.size() > cap) out += ",...";
            return Ok(out);
        }));

    add(MakeTool(
        "zeta_winding",
        "輻角原理：矩形 σ∈[s_lo,s_hi]×t∈[t_lo,t_hi] 內"
        " ζ 零點繞數（線外零點偵測）",
        {{"s_lo", ""}, {"s_hi", ""}, {"t_lo", ""},
         {"t_hi", ""}, {"step", "取樣步距"}},
        [](const Params& p) -> ToolResult {
            const int w = ArgPrincipleCount(
                [](const C& s) { return ZetaEM(s); },
                PD(p, "s_lo", 0.4), PD(p, "s_hi", 0.6),
                PD(p, "t_lo", 1.0), PD(p, "t_hi", 15.0),
                PD(p, "step", 0.01));
            return Ok("winding=" + std::to_string(w));
        }));

    // ---- RH 等價改寫 ----
    add(MakeTool(
        "zeta_li_lambda",
        "Li 判據 λ̃_n 截斷下界：ρ=½+iγ 每對共軛零點貢獻"
        " 2(1−cos nφ)≥0——全正 ⟸ RH 必要條件",
        {{"n", "階數 1..20"}, {"t_max", "零點截斷(預設130)"}},
        [](const Params& p) -> ToolResult {
            const int ord = PI(p, "n", 1);
            if (ord < 1 || ord > 20)
                return Err("n must be 1..20");
            const double tmax = PD(p, "t_max", 130.0);
            const int n4 =
                std::max(200, static_cast<int>(4.2 * tmax));
            auto zhi = [n4](double t) {
                return RotZetaHi(t, n4);
            };
            const std::vector<double> roots =
                ScanRoots(zhi, 10.0, tmax, 0.02);
            double lam = 0.0;
            for (const double g : roots) {
                const C rho(0.5, g);
                const double phi =
                    std::arg((rho - C(1, 0)) / rho);
                lam += 2.0 * (1.0 - std::cos(ord * phi));
            }
            return Ok("lambda~" + FStr(lam) + " zeros=" +
                      std::to_string(roots.size()) +
                      " (true lambda_1~=0.0231, 截斷為下界)");
        }));

    add(MakeTool(
        "zeta_debruijn",
        "de Bruijn–Newman H_t(x)=∫e^{tu²}Φ(u)cos(xu)du"
        "（梯形譜精度）。H_0 零點=2γ；RH⇔Λ≤0,"
        "Polymath15 證 Λ≤0.22",
        {{"x", "頻率(零點在 2γ)"}, {"t", "流參數(預設0)"}},
        [](const Params& p) -> ToolResult {
            const double x = PD(p, "x", 0.0);
            const double t = PD(p, "t", 0.0);
            if (std::fabs(x) > 85.0)
                return Err(
                    "|x|<=85（H_0 振幅~e^{-pi x/8} 噪底）");
            const double v = HtFlow(x, t);
            return Ok("H_" + FStr(t) + "(" + FStr(x) + ")=" +
                      FStr(v));
        }));

    add(MakeTool(
        "zeta_debruijn_scan",
        "H_t 實零點掃描 x∈[x_lo,x_hi≤85]：t≥0 時"
        "零點應全實（t=0.2 仍可證偽 Λ 上界）",
        {{"x_lo", ""}, {"x_hi", "≤85"}, {"t", "流參數"},
         {"step", "預設0.05"}},
        [](const Params& p) -> ToolResult {
            const double lo = PD(p, "x_lo", 4.0);
            const double hi = PD(p, "x_hi", 85.0);
            const double t = PD(p, "t", 0.0);
            const double step = PD(p, "step", 0.05);
            if (hi <= lo || hi > 85.0)
                return Err("need 0<x_lo<x_hi<=85");
            auto h = [t](double x) { return HtFlow(x, t); };
            const std::vector<double> r =
                ScanRoots(h, lo, hi, step);
            std::string out =
                "real_zeros=" + std::to_string(r.size()) +
                " roots=";
            char buf[32];
            for (size_t i = 0;
                 i < r.size() && i < 40; ++i) {
                if (i) out += ",";
                std::snprintf(buf, sizeof(buf), "%.4f",
                              r[i]);
                out += buf;
            }
            return Ok(out);
        }));

    // ---- 算術判據 ----
    add(MakeTool(
        "zeta_mertens",
        "Mertens M(x)=Σμ(n) 篩到 x_max：max|M|/√x<1"
        " 為 RH 等價增長尺度的弱形式（~e^{1.6e40} 才破）",
        {{"x_max", "篩上界≤10^7（預設10^6）"}},
        [](const Params& p) -> ToolResult {
            const int N = PI(p, "x_max", 1000000);
            if (N < 10 || N > 10000000)
                return Err("x_max in [10, 10^7]");
            std::vector<int> mu((size_t)N + 1, 1);
            std::vector<char> comp((size_t)N + 1, 0);
            std::vector<int> primes;
            for (int i = 2; i <= N; ++i) {
                if (!comp[(size_t)i]) {
                    primes.push_back(i);
                    mu[(size_t)i] = -1;
                }
                for (const int pr : primes) {
                    const long long m = 1LL * i * pr;
                    if (m > N) break;
                    comp[(size_t)m] = 1;
                    mu[(size_t)m] = (i % pr == 0)
                                        ? 0
                                        : -mu[(size_t)i];
                    if (i % pr == 0) break;
                }
            }
            double worst = 0.0;
            long long argmax = 0, ms = mu[1];
            for (int x = 2; x <= N; ++x) {
                ms += mu[(size_t)x];
                const double r =
                    std::fabs((double)ms) /
                    std::sqrt((double)x);
                if (r > worst) { worst = r; argmax = x; }
            }
            return Ok("max|M|/sqrt(x)=" + FStr(worst) +
                      " at x=" + std::to_string(argmax) +
                      " bound_ok=" +
                      (worst < 1.0 ? "1" : "0"));
        }));

    add(MakeTool(
        "zeta_robin",
        "Robin 判據 σ(n)<e^γ n ln ln n（n>5040）——"
        "RH 唯一純算術等價形式。回傳最大比值與 argmax",
        {{"n_max", "上界≤3×10^6（預設10^6）"}},
        [](const Params& p) -> ToolResult {
            const int N = PI(p, "n_max", 1000000);
            if (N < 5041 || N > 3000000)
                return Err("n_max in [5041, 3e6]");
            std::vector<uint32_t> sig((size_t)N + 1, 0);
            for (int d = 1; d <= N; ++d)
                for (int m = d; m <= N; m += d)
                    sig[(size_t)m] += (uint32_t)d;
            const double eG = std::exp(0.5772156649015329);
            double worst = 0.0;
            int argmax = 0;
            for (int x = 5041; x <= N; ++x) {
                const double r =
                    sig[(size_t)x] /
                    (eG * x *
                     std::log(std::log((double)x)));
                if (r > worst) { worst = r; argmax = x; }
            }
            return Ok("max ratio=" + FStr(worst) + " at n=" +
                      std::to_string(argmax) +
                      " bound_ok=" +
                      (worst < 1.0 ? "1" : "0"));
        }));

    add(MakeTool(
        "zeta_psi",
        "Chebyshev ψ(x)=Σ_{n≤x}Λ(n) 篩法——顯式公式"
        " ψ≈x−Σx^ρ/ρ 的比對目標（質數階梯）",
        {{"x", "上界≤10^7"}},
        [](const Params& p) -> ToolResult {
            const double x = PD(p, "x", 100.0);
            if (x < 2.0 || x > 1e7)
                return Err("x in [2, 10^7]");
            const double psi = PsiChebyshev(x);
            return Ok("psi(" + FStr(x) + ")=" + FStr(psi) +
                      " x-psi=" + FStr(x - psi));
        }));

    return n;
}

} // namespace ZetaTools
} // namespace AI
} // namespace Potato
