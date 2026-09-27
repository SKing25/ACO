"""Barrido de tamaños: ejecuta aco -n N y extrae métricas a data/resultados.csv"""
import os, sys, re, subprocess, csv

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(ROOT_DIR, "data")
BIN_DIR = os.path.join(ROOT_DIR, "bin")
os.makedirs(DATA_DIR, exist_ok=True)

ACO_BIN = os.path.join(BIN_DIR, "aco")
if not os.path.exists(ACO_BIN):
    ACO_BIN = os.path.join(ROOT_DIR, "aco")

NS = [20, 100, 500, 1000, 2000, 20000, 50000, 100000, 200000]
rows = []
for n in NS:
    print(f"=== n={n} ===", flush=True)
    p = subprocess.run([ACO_BIN, "-n", str(n)], capture_output=True, text=True)
    out = p.stdout
    if p.returncode != 0:
        print("ERROR:", p.stderr[-2000:]); break
    def grab(pat):
        m = re.search(pat, out)
        return m.group(1) if m else ""
    total = float(grab(r"TOTAL=([\d.]+)s"))
    tknn = float(grab(r"kNN=([\d.]+)s") or 0)
    taco = float(grab(r"ACO/jer=([\d.]+)s") or 0)
    topt = float(grab(r"2opt=([\d.]+)s") or 0)
    length = float(grab(r"longitud = ([\d.]+)"))
    peak = int(grab(r"peak \w+ \d+ -> (\d+) KB"))
    rss = re.search(r"RSS actual \d+ -> \d+ KB \(\+([\d.]+) MB\)", out)
    ants = grab(r"m=(\d+) hormigas")
    iters = grab(r"hormigas iters=(\d+)")
    tours = grab(r"totales [~= ]+(\d+)")
    rows.append({"n": n, "ants": ants, "iters": iters, "ant_tours": tours,
                 "t_knn": tknn, "t_aco": taco, "t_2opt": topt, "t_total": total,
                 "longitud": length, "peak_kb": peak,
                 "rss_delta_mb": float(rss.group(1)) if rss else ""})
    print(f"  total={total}s peak={peak}KB L={length}", flush=True)

out_csv = os.path.join(DATA_DIR, "resultados.csv")
with open(out_csv, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    w.writeheader(); w.writerows(rows)
print(f"{out_csv} listo")
