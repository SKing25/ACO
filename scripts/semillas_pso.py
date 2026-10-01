"""Buena práctica experimental PSO (guía §23): varias semillas por tamaño."""
import re, subprocess, csv
import numpy as np
import os
os.makedirs("docs/img_pso", exist_ok=True)

BIN = "./bin/pso"
CASOS = {20: [1, 2, 3, 4, 5], 2000: [11, 12, 13], 200000: [21, 22]}

def run(n, s):
    p = subprocess.run([BIN, "-n", str(n), "-s", str(s)],
                       capture_output=True, text=True)
    assert p.returncode == 0, p.stderr[-500:]
    g = lambda pat: re.search(pat, p.stdout).group(1)
    return (float(g(r"TOTAL=([\d.]+)s")), float(g(r"longitud = ([\d.]+)")),
            int(re.search(r"peak \w+ \d+ -> (\d+) KB", p.stdout).group(1)))

res = {}
for n, seeds in CASOS.items():
    Ls, Ts, Ps = [], [], []
    for s in seeds:
        t, L, pk = run(n, s)
        Ls.append(L); Ts.append(t); Ps.append(pk)
        print(f"n={n} seed={s}: L={L} t={t}s peak={pk}KB", flush=True)
    res[n] = {"L_mean": float(np.mean(Ls)), "L_std": float(np.std(Ls, ddof=1)) if len(Ls) > 1 else 0.0,
              "L_best": float(min(Ls)), "t_mean": float(np.mean(Ts)),
              "t_std": float(np.std(Ts, ddof=1)) if len(Ts) > 1 else 0.0,
              "peak_max": int(max(Ps)), "seeds": len(seeds)}

with open("data/pso_resultados_semillas.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["n"] + list(next(iter(res.values())).keys()))
    w.writeheader()
    for n, d in res.items(): w.writerow({"n": n, **d})

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
ns = sorted(res)
ax[0].errorbar(ns, [res[x]["L_mean"] for x in ns], yerr=[res[x]["L_std"] for x in ns],
               fmt="o-", capsize=4, color="green")
ax[0].set_xscale("log"); ax[0].set_xlabel("n"); ax[0].set_ylabel("longitud (media ± std)")
ax[0].set_title("PSO calidad: media ± std entre semillas"); ax[0].grid(True, which="both", alpha=0.3)
ax[1].errorbar(ns, [res[x]["t_mean"] for x in ns], yerr=[res[x]["t_std"] for x in ns],
               fmt="o-", capsize=4, color="steelblue")
ax[1].set_xscale("log"); ax[1].set_yscale("log")
ax[1].set_xlabel("n"); ax[1].set_ylabel("tiempo (s, media ± std)")
ax[1].set_title("PSO tiempo: media ± std entre semillas"); ax[1].grid(True, which="both", alpha=0.3)
fig.tight_layout(); fig.savefig("docs/img_pso/fig_pso_semillas.png", dpi=120)
print("listo")
