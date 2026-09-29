"""Gráficas del informe ACO-TSP (matplotlib, sin dependencias extra)"""
import csv
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

rows = list(csv.DictReader(open("resultados.csv")))
n = np.array([int(r["n"]) for r in rows], float)
t = np.array([float(r["t_total"]) for r in rows], float)
taco = np.array([float(r["t_aco"]) for r in rows], float)
peak = np.array([int(r["peak_kb"]) for r in rows], float) / 1000.0  # MB
L = np.array([float(r["longitud"]) for r in rows], float)
tours = np.array([int(r["ant_tours"]) for r in rows], float)
densa_mb = n**2 * 4 / 1e6  # teórica densa float32 en MB

plt.rcParams.update({"font.size": 10})

# 1. Tiempo vs n (log-log) ------------------------------------------------------
fig, ax = plt.subplots(figsize=(7, 4.5))
ax.loglog(n, t, "o-", label="TOTAL medido")
ax.loglog(n, taco, "s--", alpha=0.6, label="fase ACO/jerárquica")
ax.axvspan(15, 3000, color="gray", alpha=0.08)
ax.text(25, max(t)*0.6, "ACO serie\nm = n", fontsize=9)
ax.text(30000, max(t)*0.6, "Jerárquico\n(OpenMP)", fontsize=9)
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("tiempo (s)")
ax.set_title("Tiempo de cómputo vs tamaño (log-log)")
ax.grid(True, which="both", alpha=0.3); ax.legend(); fig.tight_layout()
fig.savefig("fig_tiempo.png", dpi=120)

# 2. Memoria vs n ----------------------------------------------------------------
fig, ax = plt.subplots(figsize=(7, 4.5))
ax.loglog(n, peak, "o-", color="darkorange", label="pico medido (VmHWM)")
ax.loglog(n, densa_mb, "r--", label="teórica matriz densa float32")
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("memoria (MB)")
ax.set_title("Memoria: medida vs densa teórica (log-log)")
ax.grid(True, which="both", alpha=0.3); ax.legend(); fig.tight_layout()
fig.savefig("fig_memoria.png", dpi=120)

# 3. Calidad ----------------------------------------------------------------------
fig, ax = plt.subplots(1, 2, figsize=(11, 4.2))
ax[0].loglog(n, L, "o-", color="green")
ax[0].set_xlabel("n"); ax[0].set_ylabel("longitud del tour")
ax[0].set_title("Longitud total vs n"); ax[0].grid(True, which="both", alpha=0.3)
ax[1].semilogx(n, L / n, "o-", color="teal")
ax[1].set_xlabel("n"); ax[1].set_ylabel("longitud / arista")
ax[1].set_title("Costo medio por arista vs n"); ax[1].grid(True, which="both", alpha=0.3)
fig.tight_layout(); fig.savefig("fig_calidad.png", dpi=120)

# 4. Hormigas ----------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(7, 4.2))
ax.loglog(n, tours, "o-", color="purple")
ax.set_xlabel("n (ciudades)"); ax.set_ylabel("recorridos-hormiga totales")
ax.set_title("Hormigas: recorridos totales (m = n, teoría clásica)")
ax.grid(True, which="both", alpha=0.3); fig.tight_layout()
fig.savefig("fig_hormigas.png", dpi=120)

# 5. Tours --------------------------------------------------------------------------
def load(cfile, tfile):
    c = np.loadtxt(cfile, delimiter=",")
    t = np.loadtxt(tfile, dtype=int)
    return c, t

fig = plt.figure(figsize=(11, 9.5))
gs = fig.add_gridspec(2, 2, height_ratios=[1, 1.25])

ax0 = fig.add_subplot(gs[0, 0])
ax1 = fig.add_subplot(gs[0, 1])
ax2 = fig.add_subplot(gs[1, :])

c20, t20 = load("coords20.csv", "tour20.csv")
o = np.append(t20, t20[0])
ax0.scatter(c20[:, 0], c20[:, 1], s=45, zorder=3, c="red")
for i, (x, y) in enumerate(c20): ax0.text(x, y, f" {i}", fontsize=7.5)
ax0.plot(c20[o, 0], c20[o, 1], lw=1.2, color="royalblue")
ax0.set_title("n = 20, ACO denso (L = 4.08)", fontsize=11, fontweight="bold")
ax0.set_aspect("equal")
ax0.grid(True, linestyle=":", alpha=0.5)

c2k, t2k = load("coords2000.csv", "tour2000.csv")
o = np.append(t2k, t2k[0])
ax1.scatter(c2k[:, 0], c2k[:, 1], s=0.8, alpha=0.4, color="navy")
ax1.plot(c2k[o, 0], c2k[o, 1], lw=0.35, alpha=0.75, color="royalblue")
ax1.set_title("n = 2.000, ACO disperso k=8 + 2-opt (L = 41.72)", fontsize=11, fontweight="bold")
ax1.set_aspect("equal")
ax1.grid(True, linestyle=":", alpha=0.5)

ck, tk = load("coords200k.csv", "tour200k.csv")
ok = np.append(tk, tk[0])
ax2.scatter(ck[::4, 0], ck[::4, 1], s=0.1, alpha=0.15, color="navy")
ax2.plot(ck[ok, 0], ck[ok, 1], lw=0.035, alpha=0.5, color="darkorange")
ax2.set_title("n = 200.000, ACO jerárquico celular (recorrido completo de 200.000 ciudades, L = 479.25)", fontsize=12, fontweight="bold")
ax2.set_aspect("equal")
ax2.grid(True, linestyle=":", alpha=0.4)
ax2.set_xlim(-0.02, 1.02)
ax2.set_ylim(-0.02, 1.02)

fig.tight_layout(); fig.savefig("fig_tours.png", dpi=180)
print("figs listas")
