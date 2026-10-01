"""Head-to-head ACO vs PSO con MISMO presupuesto e instancias.
Protocolo: misma instancia, mismo pop x iters, multi-semilla, exceso_% vs mejor
de ambos, trazas de convergencia.
Sin tocar archivos del ACO/PSO: solo orquesta los binarios y parsea su stdout.

Presupuestos iguales por celda de comparacion:
  n=20:     ACO(-a 20 -i 100) vs PSO(-S 20 -i 100)
  n=2000:   ACO(-a 20 -i 50)  vs PSO(-S 20 -i 50)
  n=200000: ACO(-a 8 -i 10)   vs PSO(-S 8 -i 10)   (por celda, como el companero)
Salidas nuevas: data/comp_*.csv, docs/img_comp/*.png
"""
import re, subprocess, csv, os
import numpy as np

ACO, PSO = "./bin/aco", "./bin/pso"
CONF = {20: {"aco": ["-a", "20", "-i", "100"], "pso": ["-S", "20", "-i", "100"]},
        2000: {"aco": ["-a", "20", "-i", "50"], "pso": ["-S", "20", "-i", "50"]},
        200000: {"aco": ["-a", "8", "-i", "10"], "pso": ["-S", "8", "-i", "10"]}}
SEEDS = [1, 2, 3]
os.makedirs("docs/img_comp", exist_ok=True)

def run(bin_, n, extra, seed):
    p = subprocess.run([bin_, "-n", str(n)] + extra + ["-s", str(seed)],
                       capture_output=True, text=True)
    assert p.returncode == 0, p.stderr[-500:]
    out = p.stdout
    g = lambda pat: re.search(pat, out).group(1)
    trace = [float(m.group(1)) for m in re.finditer(r"mejor-global=([\d.]+)", out)]
    return {"algo": "aco" if "aco" in bin_ else "pso", "n": n, "seed": seed,
            "length": float(g(r"longitud = ([\d.]+)")),
            "t_total": float(g(r"TOTAL=([\d.]+)s")),
            "peak_kb": int(re.search(r"peak (?:[A-Za-z]+ )?(\d+) -> (\d+) KB", out).group(2)),
            "trace": trace}

rows, traces = [], {}
for n, cf in CONF.items():
    for algo, bin_ in (("aco", ACO), ("pso", PSO)):
        for s in SEEDS:
            r = run(bin_, n, cf[algo], s)
            print(f"n={n} {algo} seed={s}: L={r['length']} t={r['t_total']}s peak={r['peak_kb']}KB "
                  f"iters_traza={len(r['trace'])}", flush=True)
            traces[(algo, n, s)] = r.pop("trace")
            rows.append(r)

with open("data/comp_pso_vs_aco.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["algo", "n", "seed", "length", "t_total", "peak_kb"])
    w.writeheader(); w.writerows(rows)

mx = max(len(t) for t in traces.values())
with open("data/comp_traces.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["algo", "n", "seed"] + [f"it{i+1}" for i in range(mx)])
    for (a, n, s), t in traces.items():
        w.writerow([a, n, s] + t + [""] * (mx - len(t)))

# Resumen con exceso_% (media vs mejor tour de AMBOS, como el companero)
res = {}
for n in CONF:
    for algo in ("aco", "pso"):
        sel = [r for r in rows if r["n"] == n and r["algo"] == algo]
        L = [r["length"] for r in sel]; T = [r["t_total"] for r in sel]
        P = [r["peak_kb"] for r in sel]
        res[(n, algo)] = {"corridas": len(sel), "media": float(np.mean(L)),
                          "desv": float(np.std(L, ddof=1)) if len(L) > 1 else 0.0,
                          "mejor": float(min(L)), "t": float(np.mean(T)),
                          "t_desv": float(np.std(T, ddof=1)) if len(T) > 1 else 0.0,
                          "mem": float(np.mean(P) / 1024),
                          "mem_desv": float(np.std(P, ddof=1) / 1024) if len(P) > 1 else 0.0}
for n in CONF:
    b = min(res[(n, "aco")]["mejor"], res[(n, "pso")]["mejor"])
    for algo in ("aco", "pso"):
        res[(n, algo)]["exceso"] = 100 * (res[(n, algo)]["media"] / b - 1)
        res[(n, algo)]["exc_desv"] = 100 * res[(n, algo)]["desv"] / b

with open("data/comp_resumen.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["n", "algo", "corridas", "media", "desv", "mejor",
                                      "t", "t_desv", "mem", "mem_desv", "exceso", "exc_desv"])
    w.writeheader()
    for (n, a), d in res.items(): w.writerow({"n": n, "algo": a, **d})
print("data/comp_*.csv listos")

# Figuras
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
COL = {"aco": "#c0392b", "pso": "#2471a3"}
ns = sorted(CONF)

fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
w = 0.38
for k, algo in enumerate(["aco", "pso"]):
    xs = [i + (k - 0.5) * w for i in range(len(ns))]
    ev = [res[(n, algo)]["exceso"] for n in ns]
    ee = [res[(n, algo)]["exc_desv"] for n in ns]
    lo = [max(v - e, v * 0.2) for v, e in zip(ev, ee)]  # el exceso no baja de 0
    ax[0].bar(xs, ev, w, yerr=[[v - l for v, l in zip(ev, lo)],
                               [e for e in ee]],
              color=COL[algo], label=algo.upper(), capsize=4)
    for x, v, e in zip(xs, ev, ee):  # etiqueta: la barra roja sí existe, solo era invisible
        lab = f"{v:.0f}%" if v >= 10 else f"{v:.2g}%"
        ax[0].text(x, (v + e) * 1.25, lab, ha="center", va="bottom", fontsize=8)
    ax[1].bar(xs, [res[(n, algo)]["t"] for n in ns], w,
              yerr=[res[(n, algo)]["t_desv"] for n in ns], color=COL[algo], label=algo.upper(), capsize=4)
    ax[2].bar(xs, [res[(n, algo)]["mem"] for n in ns], w,
              yerr=[res[(n, algo)]["mem_desv"] for n in ns],
              color=COL[algo], label=algo.upper(), capsize=4)
ax[0].set_title("Exceso % sobre el mejor tour (media, log)")
ax[0].set_yscale("log")
ax[1].set_title("Tiempo medio (s, log)")
ax[2].set_title("Memoria pico media (MB)")
for a_ in ax:
    a_.set_xticks(range(len(ns))); a_.set_xticklabels([f"n={n}" for n in ns])
    a_.legend(); a_.grid(axis="y", alpha=0.3)
ax[1].set_yscale("log")
fig.tight_layout(); fig.savefig("docs/img_comp/fig_comp_barras.png", dpi=120)

fig, ax = plt.subplots(1, 2, figsize=(13, 4.5))
for i, n in enumerate([20, 2000]):
    for algo in ("aco", "pso"):
        t = traces[(algo, n, 1)]
        ax[i].plot(range(1, len(t) + 1), t, color=COL[algo], lw=2, label=f"{algo.upper()} mejor")
    ax[i].set_title(f"Convergencia, n={n} (semilla 1, mismo presupuesto)")
    ax[i].set_xlabel("iteracion"); ax[i].set_ylabel("longitud")
    ax[i].grid(alpha=0.3); ax[i].legend()
fig.tight_layout(); fig.savefig("docs/img_comp/fig_comp_convergencia.png", dpi=120)
print("docs/img_comp/*.png listas")
for (n, a), d in sorted(res.items()):
    print(f"n={n} {a}: media={d['media']:.2f} desv={d['desv']:.2f} mejor={d['mejor']:.2f} "
          f"t={d['t']:.3f}s mem={d['mem']:.2f}MB exceso={d['exceso']:.2f}%")
