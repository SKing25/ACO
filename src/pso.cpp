// =============================================================================
// PSO para el TSP — 20 / 2.000 / 200.000 ciudades (C++17)
// Guía: docs/09_IA_2026_2.pdf (§12–§29). Compilar:
//   g++ -O3 -march=native -fopenmp -std=c++17 -Wall -Wextra src/pso.cpp -o bin/pso
// Uso: ./bin/pso  |  ./bin/pso -n 2000  |  ./bin/pso -n 20 --dump-tour F ...
// -----------------------------------------------------------------------------
// Diseño (el PSO de la guía es continuo §7 y el TSP es discreto):
//  - Random keys: posición x ∈ [0,1]^n; tour = argsort(x). Velocidad v init 0 (§14).
//  - v ← w·v + c1·r1·(p−x) + c2·r2·(g−x); x ← x+v (§17–§18); r1,r2 por partícula (§21).
//  - Recorte a [0,1] + |v| ≤ vmax (§20). w decreciente (§25): w0→w1 lineal.
//  - pbest/gbest por minimización (§15). Parada: máx iters + S sin mejora (§23).
// Ahorro (igual filosofía que el ACO): float/int32, sin matriz de distancias
//  (cálculo al vuelo), decodificación con buffer reutilizado O(n), jerarquía en
//  grilla + OpenMP para 200k (celdas ~900), 2-opt con ventana solo en n≤5000.
// NOTA: archivo autocontenido; no comparte código con src/aco.cpp.
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
    int n = 0, pop = -1 /*-1 => S=n*/, iters = -1;
    uint64_t seed = 42;
    int chunk = 900, sinMejora = -1;  // S: parada tras S iters sin mejora (§23)
    float w0 = 0.9f, w1 = 0.4f;       // inercia decreciente (§25); ej. §19: w=0.7
    float c1 = 1.4f, c2 = 1.6f;       // cognitivo / social (§19: 1.4 / 1.6)
    float vmax = 0.2f;                // cota de velocidad (§20, rango [0,1])
    std::string dumpTour, dumpCoords;
};

struct City { float x, y; };
static inline float distCity(const City& a, const City& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ---------------- 2. Memoria ----------------
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

// ---------------- 3. Instancia (mismo generador que el ACO: instancias iguales) --
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

// ---------------- 4. Decodificación random-keys: tour = argsort(x) ---------------
static void decode(const float* x, std::vector<int>& idx /*buffer reutilizado*/) {
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return x[a] < x[b]; });
}

// ---------------- 5. PSO (§13, §21) ----------------
using Rng = std::mt19937_64;
struct PsoResult { std::vector<float> gbest; double len = 1e300; };

static PsoResult pso(const std::vector<City>& c, int S, int T, const Cfg& P,
                     uint64_t seed, bool verbose) {
    int n = (int)c.size();
    Rng rng(seed);
    std::uniform_real_distribution<float> uni01(0.0f, 1.0f);
    std::uniform_real_distribution<float> unir(0.0f, 1.0f);  // r1, r2 ∈ [0,1) (§16)

    // Enjambre: X, V, P (pbest) + valores; gbest global (§12, §15)
    std::vector<float> X((size_t)S * n), V((size_t)S * n, 0.0f), PB((size_t)S * n);
    std::vector<double> pbVal(S, 1e300);
    std::vector<float> gb(n, 0.0f);
    double gbVal = 1e300;
    std::vector<int> idx(n), tour(n);

    for (int s = 0; s < S; ++s) {  // init: x∼U[0,1], v=0, pbest=x (§14)
        for (int d = 0; d < n; ++d) {
            float x = uni01(rng);
            X[(size_t)s * n + d] = x;
            PB[(size_t)s * n + d] = x;
        }
        decode(&X[(size_t)s * n], idx);
        double L = tourLen(idx, c);
        pbVal[s] = L;
        if (L < gbVal) { gbVal = L; gb = std::vector<float>(&X[(size_t)s * n], &X[(size_t)s * n] + n); }
    }

    int lastImp = -1;
    for (int t = 0; t < T; ++t) {
        float w = P.w0 - (P.w0 - P.w1) * (T == 1 ? 0.0f : (float)t / (T - 1));  // §25
        double itBest = 1e300;
        for (int s = 0; s < S; ++s) {
            float r1 = unir(rng), r2 = unir(rng);  // por partícula (§21)
            size_t b = (size_t)s * n;
            for (int d = 0; d < n; ++d) {  // §17–§18 + recorte/vmax (§20)
                float v = w * V[b + d]
                        + P.c1 * r1 * (PB[b + d] - X[b + d])
                        + P.c2 * r2 * (gb[d] - X[b + d]);
                if (v > P.vmax) v = P.vmax; else if (v < -P.vmax) v = -P.vmax;
                float x = X[b + d] + v;
                if (x > 1.0f) x = 1.0f; else if (x < 0.0f) x = 0.0f;
                V[b + d] = v; X[b + d] = x;
            }
            decode(&X[b], idx);
            double L = tourLen(idx, c);
            itBest = std::min(itBest, L);
            if (L < pbVal[s]) {  // memoria personal (§15)
                pbVal[s] = L;
                std::copy(&X[b], &X[b] + n, &PB[b]);
            }
        }
        for (int s = 0; s < S; ++s)  // memoria global (§15, §21)
            if (pbVal[s] < gbVal) {
                gbVal = pbVal[s]; lastImp = t;
                std::copy(&PB[(size_t)s * n], &PB[(size_t)s * n] + n, gb.begin());
            }
        if (verbose)
            std::printf("  it %3d/%d  mejor-global=%.2f  mejor-it=%.2f  w=%.3f\n", t + 1, T, gbVal, itBest, w);
        if (P.sinMejora > 0 && t - lastImp + 1 >= P.sinMejora) {
            if (verbose) std::printf("  parada temprana en it %d (S=%d sin mejora)\n", t + 1, P.sinMejora);
            break;
        }
    }
    return {gb, gbVal};
}

// ---------------- 6. NN + 2-opt (costura y pulido, igual que en el ACO) ----------
static std::vector<int> nnTour(const std::vector<City>& c, int start = 0) {
    int n = (int)c.size();
    std::vector<int> tour(n); std::vector<uint8_t> vis(n, 0);
    tour[0] = start; vis[start] = 1; int cur = start;
    for (int s = 1; s < n; ++s) {
        float bd = 1e30f; int bn = -1;
        for (int v = 0; v < n; ++v) {
            if (vis[v]) continue;
            float dx = c[v].x - c[cur].x, dy = c[v].y - c[cur].y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bd) { bd = d2; bn = v; }
        }
        cur = bn; tour[s] = cur; vis[cur] = 1;
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

// ---------------- 7. Jerárquico (n masiva): misma grilla que el ACO --------------
static std::vector<int> hierarchical(const std::vector<City>& c, const Cfg& P,
                                     int S, int T, uint64_t seed, bool verbose, int& nCeldas) {
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
    std::vector<std::vector<int>> subs(nCeldas);
    std::vector<City> centros(nCeldas);
    int done = 0;
    #pragma omp parallel for schedule(dynamic) shared(done)
    for (int ci = 0; ci < nCeldas; ++ci) {
        std::vector<int> gidx(order.begin() + ranges[ci].first, order.begin() + ranges[ci].second);
        std::vector<City> sub(gidx.size());
        double sx = 0, sy = 0;
        for (size_t t = 0; t < gidx.size(); ++t) { sub[t] = c[gidx[t]]; sx += sub[t].x; sy += sub[t].y; }
        centros[ci] = {(float)(sx / sub.size()), (float)(sy / sub.size())};
        std::vector<int> local;
        if ((int)sub.size() <= 30) {
            local = nnTour(sub);
        } else {
            PsoResult r = pso(sub, S, T, P, seed + ci, false);
            local.resize(sub.size());
            std::vector<int> idx(sub.size());
            decode(r.gbest.data(), idx);
            local = idx;
        }
        subs[ci].resize(local.size());
        for (size_t t = 0; t < local.size(); ++t) subs[ci][t] = gidx[local[t]];
        int d = __sync_add_and_fetch(&done, 1);
        if (verbose && d % 50 == 0) {
            #pragma omp critical
            std::printf("  %d/%d celdas...\n", d, nCeldas);
        }
    }
    std::vector<int> global;
    global.reserve(n);
    for (int id : nnTour(centros)) global.insert(global.end(), subs[id].begin(), subs[id].end());
    return global;
}

// ---------------- 8. Experimento + CLI ----------------
static double now() {
    using C = std::chrono::steady_clock;
    return std::chrono::duration<double>(C::now().time_since_epoch()).count();
}

static void experimento(Cfg cfg, int S, int T) {
    std::printf("\n===== n = %d =====\nParams: S=%d part. T=%d w=[%.2f,%.2f] c1=%.2f c2=%.2f vmax=%.2f S_stop=%d\n",
                cfg.n, S, T, cfg.w0, cfg.w1, cfg.c1, cfg.c2, cfg.vmax, cfg.sinMejora);
    std::printf("Densa teórica: %.3f GB | enjambre: %.2f MB\n",
                gbDensa(cfg.n), (double)S * cfg.n * 4 * 3 / 1e6);
    long r0 = rssKb(), p0 = peakKb();
    double t0 = now(), tPso = 0, tOpt = 0;
    std::vector<City> c = genCities(cfg.n, cfg.seed);
    std::vector<int> tour; double len = 0;

    if (cfg.n > 5000) {
        double t1 = now();
        int nc = 0;
        tour = hierarchical(c, cfg, S, T, cfg.seed, true, nc);
        tPso = now() - t1; len = tourLen(tour, c);
        std::printf("Evaluaciones totales = %lld (n_celdas x S x T)\n", (long long)nc * S * T);
    } else {
        double t1 = now();
        PsoResult r = pso(c, S, T, cfg, cfg.seed + 1, true);
        tPso = now() - t1;
        tour.resize(cfg.n);
        decode(r.gbest.data(), tour);
        len = tourLen(tour, c);
        std::printf("Evaluaciones totales = %lld (S x T)\n", (long long)S * T);
        if (cfg.n >= 200 && cfg.n <= 5000) {
            double t2 = now();
            len = twoOpt(tour, c, 1, 40); tOpt = now() - t2;
            len = tourLen(tour, c);
        }
        if (cfg.n <= 2000)
            std::printf("Referencia NN greedy: %.2f\n", tourLen(nnTour(c), c));
    }
    long r1 = rssKb(), p1 = peakKb();
    std::printf("Tour válido: %s | longitud = %.2f\n", validTour(tour, cfg.n) ? "SI" : "NO", len);
    std::printf("Tiempos: PSO/jer=%.3fs 2opt=%.3fs TOTAL=%.3fs\n", tPso, tOpt, now() - t0);
    std::printf("Memoria: RSS actual %ld -> %ld KB (+%.1f MB) | peak VmHWM %ld -> %ld KB\n",
                r0, r1, (r1 - r0) / 1000.0, p0, p1);
    if (!cfg.dumpTour.empty()) {
        std::ofstream o(cfg.dumpTour);
        for (int v : tour) o << v << "\n";
    }
    if (!cfg.dumpCoords.empty()) {
        std::ofstream o(cfg.dumpCoords);
        for (auto& p : c) o << p.x << "," << p.y << "\n";
    }
}

int main(int argc, char** argv) {
    Cfg cli; bool soloN = false;
    int cliS = -1, cliT = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto val = [&](int& i_) -> std::string {
            if (i_ + 1 >= argc) { std::fprintf(stderr, "Falta valor para %s\n", a.c_str()); std::exit(1); }
            return argv[++i_];
        };
        if (a == "-n") { cli.n = std::stoi(val(i)); soloN = true; }
        else if (a == "-S") cliS = std::stoi(val(i));
        else if (a == "-i") cliT = std::stoi(val(i));
        else if (a == "-s") cli.seed = (uint64_t)std::stoul(val(i));
        else if (a == "--w0") cli.w0 = std::stof(val(i));
        else if (a == "--w1") cli.w1 = std::stof(val(i));
        else if (a == "-c1") cli.c1 = std::stof(val(i));
        else if (a == "-c2") cli.c2 = std::stof(val(i));
        else if (a == "--vmax") cli.vmax = std::stof(val(i));
        else if (a == "--sin-mejora") cli.sinMejora = std::stoi(val(i));
        else if (a == "--chunk") cli.chunk = std::stoi(val(i));
        else if (a == "--dump-tour") cli.dumpTour = val(i);
        else if (a == "--dump-coords") cli.dumpCoords = val(i);
        else { std::printf("Uso: %s [-n N] [-S part.] [-i iters] [-s seed] [--w0 w] [--w1 w]\n"
                           "     [-c1 c] [-c2 c] [--vmax v] [--sin-mejora S] [--chunk C]\n"
                           "     [--dump-tour F] [--dump-coords F]\n", argv[0]); return 0; }
    }
    // Presets por régimen: S=n (simetría con ACO m=n) salvo celdas (S=32, cota)
    auto reg = [&](int n) -> std::pair<int,int> {
        if (n <= 100) return {30, 300};
        if (n <= 5000) return {n, 100};
        return {32, 40};
    };
    auto stopS = [&](int n) { return n <= 100 ? 50 : (n <= 5000 ? 15 : 0); };
    std::printf("PSO-TSP | CPUs: %ld | RSS: %ld KB\n", sysconf(_SC_NPROCESSORS_ONLN), rssKb());
    if (!soloN) {
        for (int n : {20, 2000, 200000}) {
            Cfg d = cli; d.n = n;
            auto [S, T] = reg(n);
            if (cliS > 0) S = cliS;
            if (cliT > 0) T = cliT;
            if (cli.sinMejora == -1) d.sinMejora = stopS(n);
            if (n == 20) { d.seed = 20; } else if (n == 2000) { d.seed = 2000; } else { d.seed = 200000; }
            if (cli.seed != 42) d.seed = cli.seed;
            experimento(d, S, T);
        }
    } else {
        Cfg d = cli;
        auto [S, T] = reg(cli.n);
        if (cliS > 0) S = cliS;
        if (cliT > 0) T = cliT;
        if (cli.sinMejora == -1) d.sinMejora = stopS(cli.n);
        if (cli.seed == 42) d.seed = (uint64_t)cli.n;  // misma instancia que el ACO
        experimento(d, S, T);
    }
}
