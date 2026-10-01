"""Barrido PSO-TSP: ejecuta ./bin/pso -n N y extrae metricas a data/pso_resultados.csv"""
import re, subprocess, csv, os

BIN = "./bin/pso"
NS = [20, 100, 500, 1000, 2000, 20000, 50000, 100000, 200000]
rows = []
for n in NS:
    print(f"=== n={n} ===", flush=True)
    p = subprocess.run([BIN, "-n", str(n)], capture_output=True, text=True)
    out = p.stdout
    if p.returncode != 0:
        print("ERROR:", p.stderr[-2000:]); break
    def grab(pat):
        m = re.search(pat, out)
        return m.group(1) if m else ""
    total = float(grab(r"TOTAL=([\d.]+)s"))
    tpso = float(grab(r"PSO/jer=([\d.]+)s") or 0)
    topt = float(grab(r"2opt=([\d.]+)s") or 0)
    length = float(grab(r"longitud = ([\d.]+)"))
    peak = int(grab(r"peak \w+ \d+ -> (\d+) KB"))
    rss = re.search(r"RSS actual \d+ -> \d+ KB \(\+([\d.]+) MB\)", out)
    S = grab(r"S=(\d+) part")
    T = grab(r"T=(\d+)")
    ev = grab(r"totales = (\d+)")
    rows.append({"n": n, "pop": S, "iters": T, "evals": ev,
                 "t_pso": tpso, "t_2opt": topt, "t_total": total,
                 "longitud": length, "peak_kb": peak,
                 "rss_delta_mb": float(rss.group(1)) if rss else ""})
    print(f"  total={total}s peak={peak}KB L={length}", flush=True)

os.makedirs("data", exist_ok=True)
with open("data/pso_resultados.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    w.writeheader(); w.writerows(rows)
print("data/pso_resultados.csv listo")
