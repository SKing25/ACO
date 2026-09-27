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

fig, ax = plt.subplots(1, 3, figsize=(15, 4.5))
for a in ax: a.title.set_fontsize(10)
c20, t20 = load("coords20.csv", "tour20.csv")
o = np.append(t20, t20[0])
ax[0].scatter(c20[:, 0], c20[:, 1], s=50, zorder=3, c="red")
for i, (x, y) in enumerate(c20): ax[0].text(x, y, f" {i}", fontsize=7)
ax[0].plot(c20[o, 0], c20[o, 1], lw=1.2)
ax[0].set_title("n=20, ACO denso (L=3.65)"); ax[0].set_aspect("equal")

c2k, t2k = load("coords2000.csv", "tour2000.csv")
o = np.append(t2k, t2k[0])
ax[1].scatter(c2k[:, 0], c2k[:, 1], s=1, alpha=0.5)
ax[1].plot(c2k[o, 0], c2k[o, 1], lw=0.3, alpha=0.7)
ax[1].set_title("n=2000, ACO disperso k=8 + 2-opt (L=41.53)"); ax[1].set_aspect("equal")

ck, tk = load("coords200k.csv", "tour200k.csv")
rng = np.random.default_rng(0)
m = rng.choice(len(ck), size=5000, replace=False)
ax[2].scatter(ck[m, 0], ck[m, 1], s=1, alpha=0.4)
d = tk[::200]
ax[2].plot(ck[d, 0], ck[d, 1], lw=0.5, alpha=0.7, color="darkorange")
ax[2].set_title("n=200000, jerárquico (muestra 5k + tour diezmado, L=479.3)")
ax[2].set_aspect("equal")
fig.tight_layout(); fig.savefig("fig_tours.png", dpi=120)
print("figs listas")
