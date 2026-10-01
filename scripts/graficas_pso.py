"""Gráficas PSO-TSP -> docs/img_pso/ (no toca nada del ACO)"""
import csv, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

os.makedirs("docs/img_pso", exist_ok=True)
rows = list(csv.DictReader(open("data/pso_resultados.csv")))
n = np.array([int(r["n"]) for r in rows], float)
t = np.array([float(r["t_total"]) for r in rows], float)
tpso = np.array([float(r["t_pso"]) for r in rows], float)
peak = np.array([int(r["peak_kb"]) for r in rows], float) / 1000.0
L = np.array([float(r["longitud"]) for r in rows], float)
ev = np.array([int(r["evals"]) for r in rows], float)
densa_mb = n**2 * 4 / 1e6
plt.rcParams.update({"font.size": 10})

def tour_L(nn):
    for r in rows:
        if int(r["n"]) == nn:
            return float(r["longitud"])
    return float("nan")

fig, ax = plt.subplots(figsize=(7, 4.5))
ax.loglog(n, t, "o-", label="TOTAL medido")
ax.loglog(n, tpso, "s--", alpha=0.6, label="fase PSO/jerárquica")
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("tiempo (s)")
ax.set_title("PSO: tiempo vs tamaño (log-log)")
ax.grid(True, which="both", alpha=0.3); ax.legend(); fig.tight_layout()
fig.savefig("docs/img_pso/fig_pso_tiempo.png", dpi=120)

fig, ax = plt.subplots(figsize=(7, 4.5))
ax.loglog(n, peak, "o-", color="darkorange", label="pico medido (VmHWM)")
ax.loglog(n, densa_mb, "r--", label="teórica matriz densa float32")
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("memoria (MB)")
ax.set_title("PSO memoria: medida vs densa teórica (log-log)")
ax.grid(True, which="both", alpha=0.3); ax.legend(); fig.tight_layout()
fig.savefig("docs/img_pso/fig_pso_memoria.png", dpi=120)

fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
ax[0].loglog(n, L, "o-", color="green")
ax[0].set_xlabel("n"); ax[0].set_ylabel("longitud del tour")
ax[0].set_title("PSO longitud total vs n"); ax[0].grid(True, which="both", alpha=0.3)
ax[1].semilogx(n, L / n, "o-", color="teal")
ax[1].set_xlabel("n"); ax[1].set_ylabel("longitud / arista")
ax[1].set_title("PSO costo medio por arista vs n"); ax[1].grid(True, which="both", alpha=0.3)
fig.tight_layout(); fig.savefig("docs/img_pso/fig_pso_calidad.png", dpi=120)

fig, ax = plt.subplots(figsize=(7, 4.2))
ax.loglog(n, ev, "o-", color="purple")
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("evaluaciones f(x) totales")
ax.set_title("PSO: evaluaciones = S × T (población × iteraciones)")
ax.grid(True, which="both", alpha=0.3); fig.tight_layout()
fig.savefig("docs/img_pso/fig_pso_evals.png", dpi=120)

def load(cfile, tfile):
    return np.loadtxt(cfile, delimiter=","), np.loadtxt(tfile, dtype=int)

# Mismo formato que fig_tours del ACO: 2 arriba + panel 200k grande con TOUR COMPLETO
fig = plt.figure(figsize=(11, 9.5))
gs = fig.add_gridspec(2, 2, height_ratios=[1, 1.25])

ax0 = fig.add_subplot(gs[0, 0])
ax1 = fig.add_subplot(gs[0, 1])
ax2 = fig.add_subplot(gs[1, :])

c20, t20 = load("data/pso_coords20.csv", "data/pso_tour20.csv")
o = np.append(t20, t20[0])
ax0.scatter(c20[:, 0], c20[:, 1], s=45, zorder=3, c="red")
for i, (x, y) in enumerate(c20): ax0.text(x, y, f" {i}", fontsize=7.5)
ax0.plot(c20[o, 0], c20[o, 1], lw=1.2, color="royalblue")
ax0.set_title(f"n = 20, PSO random-keys (L = {tour_L(20):.2f})", fontsize=11, fontweight="bold")
ax0.set_aspect("equal")
ax0.grid(True, linestyle=":", alpha=0.5)

c2k, t2k = load("data/pso_coords2000.csv", "data/pso_tour2000.csv")
o = np.append(t2k, t2k[0])
ax1.scatter(c2k[:, 0], c2k[:, 1], s=0.8, alpha=0.4, color="navy")
ax1.plot(c2k[o, 0], c2k[o, 1], lw=0.35, alpha=0.75, color="royalblue")
ax1.set_title(f"n = 2.000, PSO random-keys + 2-opt (L = {tour_L(2000):.2f})", fontsize=11, fontweight="bold")
ax1.set_aspect("equal")
ax1.grid(True, linestyle=":", alpha=0.5)

ck, tk = load("data/pso_coords200k.csv", "data/pso_tour200k.csv")
ok = np.append(tk, tk[0])
ax2.scatter(ck[::4, 0], ck[::4, 1], s=0.1, alpha=0.15, color="navy")
ax2.plot(ck[ok, 0], ck[ok, 1], lw=0.12, alpha=0.05, color="darkorange")
ax2.set_title(f"n = 200.000, PSO jerárquico celular (recorrido completo de 200.000 ciudades, L = {tour_L(200000):.2f})",
              fontsize=12, fontweight="bold")
ax2.set_aspect("equal")
ax2.grid(True, linestyle=":", alpha=0.4)
ax2.set_xlim(-0.02, 1.02)
ax2.set_ylim(-0.02, 1.02)

fig.tight_layout(); fig.savefig("docs/img_pso/fig_pso_tours.png", dpi=180)
print("figs listas")
