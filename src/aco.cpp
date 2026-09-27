// =============================================================================
// ACO para el TSP — 20 / 2.000 / 200.000 ciudades (C++17)
// Guía: 07_IA_2026_2.pdf (§14-§23). Compilar: make  |  Uso: ./aco [-n N ...]
// Ahorro: sin matriz de distancias, feromona dispersa n×k, m=n hormigas,
// doble feromona (local ξ + global Q/L), parada S sin mejora, jerarquía+OpenMP.
// =============================================================================
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <unistd.h>

// ---------------- 1. Configuración ----------------
struct Cfg {
    int n = 0, ants = -1 /*-1 => m=n*/, iters = -1, k = 8;
    uint64_t seed = 42;
    int chunk = 900, sinMejora = -1;  // S: parada tras S iters sin mejora (§22)
    float alpha = 1.0f, beta = 2.0f;  // §16: ejemplo α=1, β=2
    float rho = 0.1f;                 // §18: evaporación τ←(1−ρ)τ
    float qbase = 0.0f;               // Q de Q/L (§19); 0 => auto Q=n
    float tau0 = 1.0f;                // §11: T₀ uniforme = 1
    float xi = 0.1f;                  // 2ª feromona (local); 0 = off
    std::string dumpTour, dumpCoords;
};

struct City { float x, y; };
static inline float distCity(const City& a, const City& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ---------------- 2. Memoria (VmHWM no hereda pico del padre) ----------------
static long procKb(const char* key) {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256]; long v = -1; size_t kl = std::strlen(key);
    while (std::fgets(line, sizeof line, f))
        if (!std::strncmp(line, key, kl)) { std::sscanf(line + kl, "%ld", &v); break; }
    std::fclose(f);
    return v;
}
static long rssKb() { return procKb("VmRSS:"); }
static long peakKb() { return procKb("VmHWM:"); }
static double gbDensa(long n) { return (double)n * n * 4.0 / 1e9; }

// ---------------- 3. Instancia ----------------
static std::vector<City> genCities(int n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    std::vector<City> c; c.reserve(n);
    for (int i = 0; i < n; ++i) c.push_back({u(rng), u(rng)});
    return c;
}
static double tourLen(const std::vector<int>& t, const std::vector<City>& c) {
    double L = 0; int n = (int)t.size();
    for (int i = 0; i < n; ++i) {
        double dx = (double)c[t[i]].x - c[t[(i + 1) % n]].x;
        double dy = (double)c[t[i]].y - c[t[(i + 1) % n]].y;
        L += std::sqrt(dx * dx + dy * dy);
    }
    return L;
}
static bool validTour(const std::vector<int>& t, int n) {
    if ((int)t.size() != n) return false;
    std::vector<uint8_t> seen(n, 0);
    for (int v : t) seen[v] = 1;
    return std::all_of(seen.begin(), seen.end(), [](uint8_t v) { return v; });
}

// ---------------- 4. kNN exacto (buffer O(n), sin matriz n×n) ----------------
static void buildKnn(const std::vector<City>& c, int k,
                     std::vector<int>& idx, std::vector<float>& dst) {
    int n = (int)c.size(); k = std::min(k, n - 1);
    idx.assign((size_t)n * k, 0); dst.assign((size_t)n * k, 0);
    std::vector<float> d(n); std::vector<int> id(n), sel(k);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            float dx = c[i].x - c[j].x, dy = c[i].y - c[j].y;
            d[j] = dx * dx + dy * dy;
        }
        d[i] = 1e30f;
        std::iota(id.begin(), id.end(), 0);
        std::nth_element(id.begin(), id.begin() + k, id.end(),
                         [&](int a, int b) { return d[a] < d[b]; });
        for (int t = 0; t < k; ++t) sel[t] = id[t];
        std::sort(sel.begin(), sel.end(), [&](int a, int b) { return d[a] < d[b]; });
        for (int t = 0; t < k; ++t) {
            idx[(size_t)i * k + t] = sel[t];
            dst[(size_t)i * k + t] = std::sqrt(d[sel[t]]);
        }
    }
}

// ---------------- 5. Piezas ACO ----------------
using Rng = std::mt19937_64;

// Ruleta §16-17 sobre candidatos no visitados. Devuelve -1 si no hay.
static int pickCandidate(const std::vector<int>& knn, const std::vector<float>& tau,
                         const std::vector<float>& eta, const std::vector<uint8_t>& vis,
                         int cur, int k, float alpha, Rng& rng,
                         std::uniform_real_distribution<float>& uni) {
    size_t base = (size_t)cur * k;
    int cand[32], nc = 0; float probs[32], sum = 0;  // k ≤ 32 siempre (19 máx)
    for (int t = 0; t < k; ++t) {
        int v = knn[base + t];
        if (!vis[v]) {
            float tp = (alpha == 1.0f) ? tau[base + t] : std::pow(tau[base + t], alpha);
            float p = tp * eta[base + t];
            cand[nc] = v; probs[nc] = p; sum += p; ++nc;
        }
    }
    if (nc == 0) return -1;
    if (!(sum > 0) || !std::isfinite(sum)) return cand[rng() % (uint64_t)nc];
    float r = uni(rng) * sum, acc = 0;
    for (int t = 0; t < nc; ++t) {
        acc += probs[t];
        if (r <= acc) return cand[t];
    }
    return cand[nc - 1];
}

// Vecino más cercano entre no visitados (muestreado si son >2000).
static int pickNearest(const std::vector<City>& c, const std::vector<uint8_t>& vis,
                       int cur, Rng& rng) {
    int n = (int)c.size();
    std::vector<int> unv; unv.reserve(n);
    for (int i = 0; i < n; ++i) if (!vis[i]) unv.push_back(i);
    const std::vector<int>* pool = &unv;
    std::vector<int> sample;
    if ((int)unv.size() > 2000) {  // muestreo con reemplazo (acota el peor caso)
        sample.reserve(2000);
        for (int s = 0; s < 2000; ++s) sample.push_back(unv[rng() % unv.size()]);
        pool = &sample;
    }
    auto d2 = [&](int v) {
        float dx = c[v].x - c[cur].x, dy = c[v].y - c[cur].y;
        return dx * dx + dy * dy;
    };
    float bd = 1e30f; int bn = (*pool)[0];
    for (int v : *pool) {
        float dd = d2(v);
        if (dd < bd) { bd = dd; bn = v; }
    }
    return bn;
}

// Suma 'q' a la arista (i→j) si está en la lista de candidatos.
static void addPhero(std::vector<float>& tau, const std::vector<int>& knn,
                     int k, int i, int j, float q) {
    size_t base = (size_t)i * k;
    for (int t = 0; t < k; ++t)
        if (knn[base + t] == j) { tau[base + t] += q; return; }
}
static void evaporate(std::vector<float>& tau, float decay) {
    for (float& v : tau) {
        v *= decay;
        if (v < 0.05f) v = 0.05f; else if (v > 5.0f) v = 5.0f;
    }
}

// ---------------- 6. ACO disperso con doble feromona ----------------
struct AcoResult { std::vector<int> tour; double len = 1e300; };

static AcoResult aco(const std::vector<City>& c, const std::vector<int>& knn,
                     const std::vector<float>& knnDist, int k,
                     int m, int iters, const Cfg& P, uint64_t seed, bool verbose) {
    int n = (int)c.size();
    Rng rng(seed);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    std::vector<float> tau((size_t)n * k, P.tau0), eta((size_t)n * k);
    for (size_t i = 0; i < eta.size(); ++i)
        eta[i] = std::pow(1.0f / (knnDist[i] + 1e-9f), P.beta);

    float Qb = (P.qbase > 0) ? P.qbase : (float)n;
    float decay = 1.0f - P.rho;
    std::vector<int> tour(n), best;
    std::vector<uint8_t> vis(n);
    double bestLen = 1e300; int lastImp = -1;

    for (int it = 0; it < iters; ++it) {
        double itBest = 1e300;
        for (int a = 0; a < m; ++a) {
            std::fill(vis.begin(), vis.end(), 0);
            int cur = (int)(rng() % (uint64_t)n);
            tour[0] = cur; vis[cur] = 1;
            for (int step = 1; step < n; ++step) {
                int nxt = pickCandidate(knn, tau, eta, vis, cur, k, P.alpha, rng, uni);
                if (nxt < 0) nxt = pickNearest(c, vis, cur, rng);
                if (P.xi > 0) {  // feromona LOCAL (pequeña): enfría la arista
                    size_t b = (size_t)cur * k;
                    for (int t = 0; t < k; ++t)
                        if (knn[b + t] == nxt) {
                            tau[b + t] = (1 - P.xi) * tau[b + t] + P.xi * P.tau0;
                            break;
                        }
                }
                tour[step] = nxt; vis[nxt] = 1; cur = nxt;
            }
            double L = tourLen(tour, c);
            itBest = std::min(itBest, L);
            if (L < bestLen) {  // refuerzo inmediato del mejor (grande)
                bestLen = L; best = tour; lastImp = it;
                float Q = Qb / (float)L;
                for (int s = 0; s < n; ++s)
                    addPhero(tau, knn, k, tour[s], tour[(s + 1) % n], Q);
            }
        }
        evaporate(tau, decay);
        float Qg = Qb / (float)bestLen * P.rho * 2.0f;  // elitista global
        for (int s = 0; s < n; ++s)
            addPhero(tau, knn, k, best[s], best[(s + 1) % n], Qg);
        if (verbose)
            std::printf("  it %2d/%d  mejor-global=%.2f  mejor-it=%.2f\n", it + 1, iters, bestLen, itBest);
        if (P.sinMejora > 0 && it - lastImp + 1 >= P.sinMejora) {
            if (verbose) std::printf("  parada temprana en it %d (S=%d sin mejora)\n", it + 1, P.sinMejora);
            break;
        }
    }
    return {best, bestLen};
}

// ---------------- 7. NN + 2-opt con ventana ----------------
static std::vector<int> nnTour(const std::vector<City>& c, int start = 0) {
    int n = (int)c.size();
    std::vector<int> tour(n); std::vector<uint8_t> vis(n, 0);
    tour[0] = start; vis[start] = 1;
    Rng rng(0);  // pickNearest solo muestrea si >2000; semilla fija = determinista
    for (int s = 1, cur = start; s < n; ++s) {
        cur = pickNearest(c, vis, cur, rng);
        tour[s] = cur; vis[cur] = 1;
    }
    return tour;
}
static double twoOpt(std::vector<int>& t, const std::vector<City>& c, int pasadas, int ventana) {
    int n = (int)t.size();
    double best = tourLen(t, c);
    for (int p = 0; p < pasadas; ++p) {
        bool mejoro = false;
        for (int i = 0; i < n - 2; ++i) {
            int jmax = ventana <= 0 ? n : std::min(n, i + ventana);
            for (int j = i + 2; j < jmax; ++j) {
                if (i == 0 && j == n - 1) continue;
                double antes = distCity(c[t[i]], c[t[(i + 1) % n]]) + distCity(c[t[j]], c[t[(j + 1) % n]]);
                double desp = distCity(c[t[i]], c[t[j]]) + distCity(c[t[(i + 1) % n]], c[t[(j + 1) % n]]);
                if (desp < antes - 1e-12) {
                    std::reverse(t.begin() + i + 1, t.begin() + j + 1);
                    best -= (antes - desp); mejoro = true;
                }
            }
        }
        if (!mejoro) break;
    }
    return best;
}

// ---------------- 8. Jerárquico (n masiva) ----------------
static std::vector<int> solveCells(const std::vector<City>& c, const std::vector<int>& order,
                                   const std::vector<std::pair<int,int>>& ranges, const Cfg& P,
                                   int mCfg, int iters, uint64_t seed,
                                   std::vector<City>& centros, bool verbose) {
    int nc = (int)ranges.size();
    std::vector<std::vector<int>> subs(nc);
    centros.resize(nc);
    int done = 0;
    #pragma omp parallel for schedule(dynamic) shared(done)
    for (int ci = 0; ci < nc; ++ci) {
        std::vector<int> gidx(order.begin() + ranges[ci].first, order.begin() + ranges[ci].second);
        std::vector<City> sub(gidx.size());
        double sx = 0, sy = 0;
        for (size_t t = 0; t < gidx.size(); ++t) { sub[t] = c[gidx[t]]; sx += sub[t].x; sy += sub[t].y; }
        centros[ci] = {(float)(sx / sub.size()), (float)(sy / sub.size())};
        std::vector<int> local;
        if ((int)sub.size() <= std::max(30, P.k + 1)) {
            local = nnTour(sub);
        } else {
            int kk = std::min(P.k, (int)sub.size() - 1);
            std::vector<int> ki; std::vector<float> kd;
            buildKnn(sub, kk, ki, kd);
            int m = (mCfg < 0) ? (int)sub.size() : mCfg;  // m = |celda|
            local = aco(sub, ki, kd, kk, m, iters, P, seed + ci, false).tour;
            if (local.empty()) local = nnTour(sub);
        }
        subs[ci].resize(local.size());
        for (size_t t = 0; t < local.size(); ++t) subs[ci][t] = gidx[local[t]];
        int d = __sync_add_and_fetch(&done, 1);
        if (verbose && d % 50 == 0) {
            #pragma omp critical
            std::printf("  %d/%d celdas...\n", d, nc);
        }
    }
    // Costura: NN sobre centroides + concatenar
    std::vector<int> global;
    global.reserve(c.size());
    for (int id : nnTour(centros)) global.insert(global.end(), subs[id].begin(), subs[id].end());
    return global;
}

static std::vector<int> hierarchical(const std::vector<City>& c, const Cfg& P,
                                     int iters, uint64_t seed, bool verbose, int& nCeldas) {
    int n = (int)c.size();
    int G = std::max(1, (int)std::ceil(std::sqrt((double)n / P.chunk)));
    std::vector<int> cell(n);
    for (int i = 0; i < n; ++i)
        cell[i] = std::min(G - 1, (int)(c[i].x * G)) * G + std::min(G - 1, (int)(c[i].y * G));
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return cell[a] < cell[b]; });
    std::vector<std::pair<int,int>> ranges;
    for (int a = 0; a < n;) {
        int b = a;
        while (b < n && cell[order[b]] == cell[order[a]]) ++b;
        ranges.emplace_back(a, b); a = b;
    }
    nCeldas = (int)ranges.size();
    if (verbose) std::printf("Grilla %dx%d -> %d celdas\n", G, G, nCeldas);
    std::vector<City> centros;
    return solveCells(c, order, ranges, P, P.ants, iters, seed, centros, verbose);
}

// ---------------- 9. Experimento + CLI ----------------
static double now() {
    using C = std::chrono::steady_clock;
    return std::chrono::duration<double>(C::now().time_since_epoch()).count();
}
static void saveVec(const std::string& f, const std::vector<int>& v) {
    if (f.empty()) return;
    std::ofstream o(f);
    for (int x : v) o << x << "\n";
}

static void experimento(Cfg cfg) {
    int m = (cfg.ants < 0) ? cfg.n : cfg.ants;  // clásico: m = n
    std::printf("\n===== n = %d =====\nParams: m=%d iters=%d k=%d α=%.2f β=%.2f ρ=%.2f Q=%s ξ=%.2f S=%d\n",
                cfg.n, m, cfg.iters, cfg.k, cfg.alpha, cfg.beta, cfg.rho,
                cfg.qbase > 0 ? std::to_string(cfg.qbase).c_str() : "auto(n)", cfg.xi, cfg.sinMejora);
    std::printf("Densa teórica: %.3f GB | dispersa n*%d: %.2f MB\n",
                gbDensa(cfg.n), cfg.k, (double)cfg.n * cfg.k * 4 / 1e6);
    long r0 = rssKb(), p0 = peakKb();
    double t0 = now(), tKnn = 0, tAco = 0, tOpt = 0;
    std::vector<City> c = genCities(cfg.n, cfg.seed);
    std::vector<int> tour; double len = 0;

    if (cfg.n > 5000) {
        double t1 = now();
        int nc = 0;
        tour = hierarchical(c, cfg, cfg.iters, cfg.seed, true, nc);
        tAco = now() - t1; len = tourLen(tour, c);
        std::printf("Recorridos-hormiga = %lld (n x iters)\n", (long long)cfg.n * cfg.iters);
    } else {
        std::vector<int> ki; std::vector<float> kd;
        double t1 = now();
        int kk = std::min(cfg.k, cfg.n - 1);
        buildKnn(c, kk, ki, kd); tKnn = now() - t1;
        double t2 = now();
        tour = aco(c, ki, kd, kk, m, cfg.iters, cfg, cfg.seed + 1, true).tour;
        len = tourLen(tour, c); tAco = now() - t2;
        std::printf("Recorridos-hormiga = %lld\n", (long long)m * cfg.iters);
        if (cfg.n >= 200 && cfg.n <= 5000) {
            double t3 = now();
            len = twoOpt(tour, c, 1, 40); tOpt = now() - t3;
            len = tourLen(tour, c);
        }
        if (cfg.n <= 2000)
            std::printf("Referencia NN greedy: %.2f\n", tourLen(nnTour(c), c));
    }
    long r1 = rssKb(), p1 = peakKb();
    std::printf("Tour válido: %s | longitud = %.2f\n", validTour(tour, cfg.n) ? "SI" : "NO", len);
    std::printf("Tiempos: kNN=%.3fs ACO/jer=%.3fs 2opt=%.3fs TOTAL=%.3fs\n", tKnn, tAco, tOpt, now() - t0);
    std::printf("Memoria: RSS actual %ld -> %ld KB (+%.1f MB) | peak %ld -> %ld KB\n",
                r0, r1, (r1 - r0) / 1000.0, p0, p1);
    saveVec(cfg.dumpTour, tour);
    if (!cfg.dumpCoords.empty()) {
        std::ofstream o(cfg.dumpCoords);
        for (auto& p : c) o << p.x << "," << p.y << "\n";
    }
}

int main(int argc, char** argv) {
    Cfg cli; bool soloN = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto val = [&](int& i_) -> std::string {
            if (i_ + 1 >= argc) { std::fprintf(stderr, "Falta valor para %s\n", a.c_str()); std::exit(1); }
            return argv[++i_];
        };
        if (a == "-n") { cli.n = std::stoi(val(i)); soloN = true; }
        else if (a == "-a") cli.ants = std::stoi(val(i));
        else if (a == "-i") cli.iters = std::stoi(val(i));
        else if (a == "-k") cli.k = std::stoi(val(i));
        else if (a == "-s") cli.seed = (uint64_t)std::stoul(val(i));
        else if (a == "-A") cli.alpha = std::stof(val(i));
        else if (a == "-B") cli.beta = std::stof(val(i));
        else if (a == "-R") cli.rho = std::stof(val(i));
        else if (a == "--Q") cli.qbase = std::stof(val(i));
        else if (a == "--xi") cli.xi = std::stof(val(i));
        else if (a == "--sin-mejora") cli.sinMejora = std::stoi(val(i));
        else if (a == "--chunk") cli.chunk = std::stoi(val(i));
        else if (a == "--dump-tour") cli.dumpTour = val(i);
        else if (a == "--dump-coords") cli.dumpCoords = val(i);
        else { std::printf("Uso: %s [-n N] [-a m] [-i it] [-k k] [-s seed] [-A α] [-B β] [-R ρ]\n"
                           "     [--Q q] [--xi x] [--sin-mejora S] [--chunk C] [--dump-tour F] [--dump-coords F]\n", argv[0]); return 0; }
    }
    auto preset = [&](int n, int iters, int k, uint64_t seed, int S) {
        Cfg d; d.n = n; d.iters = iters; d.k = k; d.seed = seed;
        d.alpha = cli.alpha; d.beta = cli.beta; d.rho = cli.rho;
        d.qbase = cli.qbase; d.xi = cli.xi;
        d.sinMejora = (cli.sinMejora != -1) ? cli.sinMejora : S;
        return d;
    };
    std::printf("ACO-TSP | CPUs: %ld | RSS: %ld KB\n", sysconf(_SC_NPROCESSORS_ONLN), rssKb());
    if (!soloN) {
        experimento(preset(20, 50, 19, 7, 15));
        experimento(preset(2000, 10, 8, 11, 4));
        experimento(preset(200000, 2, 8, 21, 0));
    } else {
        Cfg d = cli.n <= 100 ? preset(cli.n, 50, cli.n - 1, 7, 15)
              : cli.n <= 5000 ? preset(cli.n, 10, 8, 11, 4)
                              : preset(cli.n, 2, 8, 21, 0);
        if (cli.ants != -1) d.ants = cli.ants;
        if (cli.iters != -1) d.iters = cli.iters;
        if (cli.k != 8) d.k = cli.k;
        if (cli.seed != 42) d.seed = cli.seed; else d.seed = (uint64_t)d.n;
        d.dumpTour = cli.dumpTour; d.dumpCoords = cli.dumpCoords;
        experimento(d);
    }
}
