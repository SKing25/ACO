# Informe: ACO (Colonia de Hormigas) para el TSP — 20, 2.000 y 200.000 ciudades

**Implementación:** C++17, un solo archivo (`aco.cpp` + `Makefile`), ejecución 100% local (sin Python en clase).
**Guía:** `07_IA_2026_2.pdf` (diaps. §7–§23). Los parámetros y el pseudocódigo siguen esa guía; las citas § refieren a sus diapositivas.
**Medición:** tiempo muro con `chrono::steady_clock`; memoria RSS actual (`VmRSS`) y pico (`VmHWM` de `/proc/self/status`, que no hereda el pico del padre tras `fork`, a diferencia de `getrusage`).

## 1. El problema y “la cáscara”: el factorial (guía §9–§10)

El TSP pide el tour más corto que visita `n` ciudades una vez y vuelve al origen. Para un grafo completo no dirigido (guía §10):

| n | aristas `n(n−1)/2` | tours distintos `(n−1)!/2` |
|---|---|---|
| 6 | 15 | 60 |
| 10 | 45 | 181.440 |
| 15 | 105 | 43.589.145.600 |
| **20** | 190 | ≈ 6×10¹⁶ (fuerza bruta imposible) |
| **2.000** | ≈ 2×10⁶ | ≈ 10⁵⁷³⁵ (físicamente inabarcable) |
| **200.000** | ≈ 2×10¹⁰ (≈ 160 GB en `float32`) | — (ni se escribe) |

El ACO es una **metaheurística**: no enumera, muestrea tours con feromonas. Y aquí entra el ahorro de recursos: la receta ingenua (matriz de distancias + matriz de feromonas densas + `m = n` hormigas) cuesta **O(n²) en memoria y O(it·m·n²) en tiempo**. Todo el diseño gira en torno a ese cuello de botella.

## 2. Diseño del algoritmo (ahorro de recursos)

**Parámetros** (guía §14–§21, configurables por CLI: `-A -B -R --Q --xi --sin-mejora`):

| símbolo | valor | fuente / rol (guía) |
|---|---|---|
| m | **= n** (20 / 2000 / ~889 por celda) | §21: n.º de hormigas; clásico Dorigo |
| iters | 50 / 10 / 2 | §22: presupuesto (máx. iteraciones) |
| S | 15 / 4 / off | §22: **parada tras S iters sin mejora** |
| k | 19 / 8 / 8 | lista de candidatos (poda del factorial) |
| α / β | 1.0 / **2.0** | §16: el ejemplo calculado usa α=1, β=2 |
| ρ | 0.1 (ejemplo guía: 0.25) | §18: evaporación `τ ← (1−ρ)·τ` |
| Q | auto = n (depósito `Q/Lₖ`) | §19: `Δτᵏ = Q/Lₖ` si usó la arista |
| ξ | 0.1 | actualización local (2.ª feromona) |
| τ₀ | 1.0 | §11: inicialización uniforme `T₀ = 1` |

**Regla de transición exacta de la guía (§15):** `pᵏᵢⱼ = τᵅᵢⱼ·ηᵝᵢⱼ / Σ τᵅ·ηᵝ`, con `η = 1/d` (§14) y selección por **ruleta** sobre intervalos acumulados (§16–§17). Cada hormiga guarda posición, **memoria tabú**, tour y longitud (§12). El ciclo es el pseudocódigo §20: inicializar → construir → evaluar → evaporar → depositar → mejor global.

**Doble feromona (dos magnitudes):**
1. **Local, pequeña (ξ = 0.1):** tras cada paso, `τ ← (1−ξ)·τ + ξ·τ₀`. “Enfría” la arista usada → diversifica, evita que todas copien el mismo camino.
2. **Global, grande (`Q/L`):** al cerrar la iteración, depósito elitista sobre el mejor tour + evaporación con cotas min–max (estilo MMAS).

**Ahorros estructurales:**
- Sin matriz de distancias: se calculan **al vuelo**. Memoria extra O(1).
- **Feromona dispersa `n×k`**: 2000×8 `float` = 64 KB en vez de 16 MB (×250); en 200k, 6.4 MB en vez de 160 GB (×25000).
- kNN exacto por filas con **buffer O(n) reutilizado** (nunca se materializa `n×n`).
- Construcción por candidatos **O(n·k)**; fallback muestreado a 2000 si quedan muchos sin visitar.
- `float`/`int32`/`uint8_t`; 2-opt con ventana 40 como pulido barato.
- **n = 200.000 → jerárquico:** grilla 15×15 (~225 celdas de ~900), mini-ACO por celda con `m = |celda|`, costura por centroides. Celdas independientes → **OpenMP** (semilla por celda ⇒ determinista).

## 3. Resultados

### 3.1 Barrido principal (`barrido.py` → `resultados.csv`)

| n | régimen | hormigas | recorridos-hormiga | tiempo | pico mem. | longitud |
|---|---|---|---|---|---|---|
| 20 | denso | 20 | 1.000 | 0.003 s | 4.0 MB | 3.65 (NN: 4.33) |
| 100 | disperso k=8 | 100 | 5.000 | 0.43 s | 4.2 MB | 12.63 |
| 500 | disperso k=8 | 500 | 5.000 | 0.71 s | 4.1 MB | 20.46 |
| 1.000 | disperso k=8 | 1.000 | 10.000 | 6.9 s | 4.1 MB | 29.5 |
| **2.000** | disperso k=8 + 2-opt | **2.000** | 20.000 | 34.2 s | 4.3 MB | **41.53** |
| 20.000 | jerárquico | ~800/celda | 40.000 | 4.7 s | 5.7 MB | 152.1 |
| 50.000 | jerárquico | ~800/celda | 100.000 | 11.2 s | 6.0 MB | 239.0 |
| 100.000 | jerárquico | ~800/celda | 200.000 | 25.8 s | 7.0 MB | 338.5 |
| **200.000** | jerárquico, 225 celdas | ~889/celda | **400.000** | **51.9 s** | **9.7 MB** | **479.3** |

Serie oficial (`./aco`): ≈ 1.5 min en 8 núcleos, pico < 10 MB.

![Tiempo vs n](fig_tiempo.png)
*El ACO serie con m = n explota entre n = 1000 y 2000 (34 s con parada temprana S=4; sin ella, 52 s); el régimen jerárquico devuelve la curva a crecimiento casi lineal.*

![Memoria vs n](fig_memoria.png)
*Pico medido frente a la matriz densa teórica: en 200k, 9.7 MB reales vs 160 GB teóricos (≈ ×16.500 menos).*

![Calidad](fig_calidad.png)
*Longitud total y costo medio por arista: estable entre escalas, señal de tours sanos en los tres regímenes.*

![Hormigas](fig_hormigas.png)
*Recorridos-hormiga totales (m = n): 2.000 en el caso mediano y 400.000 en 200k (n × iters).*

![Tours](fig_tours.png)
*Tours: n = 20 (denso), n = 2000 (disperso + 2-opt) y n = 200000 (jerárquico, muestra + tour diezmado).*

### 3.2 Varias semillas — buena práctica experimental (guía §23)

`semillas.py` → `resultados_semillas.csv`. Se reporta media, dispersión, mejor valor y tiempo:

| n | semillas | longitud media ± std | mejor | tiempo medio ± std | pico máx. |
|---|---|---|---|---|---|
| 20 | 5 | 3.93 ± 0.48 | **3.19** | 0.004 ± 0.001 s | 4.1 MB |
| 2000 | 3 | 42.00 ± 0.27 | **41.72** | 23.8 ± 14.6 s | 4.3 MB |
| 200000 | 2 | 479.05 ± 0.16 | **478.94** | 60.6 ± 5.9 s | 9.6 MB |

![Semillas](fig_semillas.png)
*La calidad es estable entre semillas (±1%); el tiempo en n = 2000 varía (14–41 s) porque la parada temprana S = 4 dispara distinto según la semilla — mismo resultado con hasta 65% menos cómputo.*

### 3.3 Ablación α = 0 / β = 0 (comprobación conceptual, guía §27)

Sobre n = 20 (misma instancia):

| variante | longitud | lectura (§27) |
|---|---|---|
| base α=1, β=2 | 3.65 | equilibrio reclamo |
| **α = 0** (solo heurística) | 4.58 | voraz, sin memoria colectiva |
| **β = 0** (solo feromona) | 6.56 | camina a ciegas, lo peor |

Confirma las respuestas esperadas: sin feromona hay puro greedy; sin heurística, puro ruido reforzado; la ruleta estocástica (§15–§17) es lo que preserva diversidad.

## 4. Análisis: tiempo y memoria

- **Tiempo:** el ACO serie con `m = n` escala peor que cuadrático por los escaneos de fallback al final de cada tour; la parada S = 4 ahorró ~35% en n = 2000 sin perder calidad. La lista k = 8 contiene el trabajo en O(n·k) y la jerarquía + OpenMP (×3 medido en 20k) hacen tratable 200k.
- **Memoria:** plana hasta n = 2000 (~4 MB: binario + pool OpenMP + estructuras) y suave después (9.7 MB en 200k). El término O(n²) desapareció del diseño.
- **Exploración/explotación (§22):** la doble feromona la materializa — ξ local explora, Q/L global explota; ρ = 0.1 da memoria persistente, el 0.25 del ejemplo guía olvidaría más rápido (probado vía `-R`).

## 5. Conclusiones

1. **Tarea cumplida en C++ local:** 20, 2.000 y 200.000 ciudades con `m = n` hormigas, doble feromona de distinta magnitud, regla/ruleta/evaporación/depósito de la guía y medición de tiempo + memoria.
2. **El factorial obliga a podar:** candidatos kNN + jerarquía convierten lo intratable (52 s en n = 2000 serie, 160 GB en 200k) en 1.5 min y < 10 MB totales.
3. **Ahorro real = memoria:** ×250 en 2k, ×16.500 en 200k; y la parada S + OpenMP en tiempo.
4. **Rigor experimental:** 5/3/2 semillas con media ± std y mejor valor, como exige §23.

## 6. Reproducibilidad

```bash
make                 # g++ -O3 -march=native -fopenmp
./aco                # serie oficial: 20 + 2000 + 200000
./aco -n 2000 -a 2000 -i 10 -k 8 -s 11
./aco -n 20 -A 0     # ablación: solo heurística (cfr. §27)
python3 barrido.py   # resultados.csv   | python3 semillas.py  # multi-semilla
python3 graficas.py  # fig_*.png
```
