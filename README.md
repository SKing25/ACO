# Optimización por Colonia de Hormigas (ACO) para el TSP

Implementación en C++17 de alto rendimiento y bajo consumo de memoria del algoritmo de **Optimización por Colonia de Hormigas (ACO)** para resolver el Problema del Viajante de Comercio (*Traveling Salesperson Problem*, TSP) sobre instancias euclidianas uniformes en tres escalas de tamaño: **$N = 20$**, **$N = 2.000$** y **$N = 200.000$** ciudades.

---

## Estructura del Repositorio

```text
ACO/
├── src/                  # Código fuente C++ de alto rendimiento
│   └── aco.cpp           # Implementación principal optimizada (8.2 MB pico para 200k)
│
├── bin/                  # Binarios ejecutables compilados
│   ├── aco               # Binario principal
│   └── aco1              # Binario del benchmark oficial
│
├── data/                 # Instancias generadas y resultados experimentales (.csv)
│   ├── coords20.csv      # Coordenadas (x, y) para N = 20
│   ├── coords2000.csv    # Coordenadas (x, y) para N = 2.000
│   ├── coords200k.csv    # Coordenadas (x, y) para N = 200.000
│   ├── tour20.csv        # Permutación del tour final N = 20
│   ├── tour2000.csv      # Permutación del tour final N = 2.000
│   ├── tour200k.csv      # Permutación del tour final N = 200.000
│   ├── resultados.csv    # Métricas de tiempo, memoria y calidad por escala
│   └── resultados_semillas.csv # Estudio estadístico multi-semilla
│
├── scripts/              # Herramientas de automatización y análisis en Python
│   ├── ACO_TSP.ipynb     # Jupyter Notebook interactivo con visualizaciones
│   ├── barrido.py        # Automatización de barrido sobre las 9 escalas
│   ├── graficas.py       # Generador de curvas (tiempo, memoria, calidad, tours)
│   └── semillas.py       # Evaluación de robustez estadística multi-semilla
│
├── Informe_Tecnico/       # Informes técnicos en LaTeX y PDF
│   ├── informe.pdf       # Informe Técnico Oficial (formal, 11 páginas)
│   ├── informe.tex       # Código fuente LaTeX del informe oficial
│   ├── informe_sustentacion.pdf # Guía de sustentación (didáctico con capturas, 20 págs.)
│   ├── informe_sustentacion.tex # Código fuente LaTeX de la sustentación
│   └── img/              # Figuras experimentales y curvas de convergencia
│       ├── fig_tiempo.png
│       ├── fig_memoria.png
│       ├── fig_calidad.png
│       ├── fig_hormigas.png
│       ├── fig_tours.png
│       ├── fig_semillas.png
│       ├── vscode_editor_real.png
│       └── code_snippets/
│
├── docs/                 # Documentación y material de referencia
│   ├── 07_IA_2026_2.pdf  # Diapositivas guía de la asignatura (Sesión 07)
│   ├── COMPARATIVA.md    # Análisis comparativo técnico
│   └── INFORME.md        # Resumen ejecutivo en formato Markdown
│
└── README.md             # Guía general del proyecto
```

---

<<<<<<< HEAD
## Compilación y Ejecución
=======
## 🚀 Compilación y Ejecución Directa
>>>>>>> 73d5090 (Add table of contents for the technical report on TSP and ACO algorithm)

### Requisitos
* Compilador C++17 compatible con OpenMP (`g++` recomendado).
* Python 3 con `numpy` y `matplotlib`.
* Distribución TeX Live con `pdflatex` (para compilar los informes).

### Compilación de C++
```bash
g++ -O3 -march=native -fopenmp -std=c++17 -Wall -Wextra src/aco.cpp -o bin/aco
```

### Ejecución
* **Ejecutar el conjunto completo de escalas (20, 2.000 y 200.000 ciudades):**
  ```bash
  ./bin/aco
  ```
* **Ejecutar una escala específica (ej. 200.000 ciudades):**
  ```bash
  ./bin/aco -n 200000
  ```
* **Ejecutar escala pequeña ($N=20$) con volcado de coordenadas y tour:**
  ```bash
  ./bin/aco -n 20 --dump-coords data/coords20.csv --dump-tour data/tour20.csv
  ```

### Compilación de los Informes en LaTeX
```bash
cd Informe_Tecnico
pdflatex -interaction=nonstopmode informe.tex && pdflatex -interaction=nonstopmode informe.tex
pdflatex -interaction=nonstopmode informe_sustentacion.tex && pdflatex -interaction=nonstopmode informe_sustentacion.tex
```

---

## Arquitectura de Ahorro Extremo de Recursos

| Dimensión | Matriz Densa Ingenua ($N \times N$) | Arquitectura Optimizada (`src/aco.cpp`) | Factor de Ahorro |
|---|---|---|---|
| **Distancias ($N=200k$)** | $160\text{ GB}$ en RAM | **$0\text{ MB}$** (cálculo euclidiano al vuelo) | $\infty$ |
| **Feromonas ($N=200k$)** | $160\text{ GB}$ en RAM | **$6.4\text{ MB}$** (dispersa $k$-NN con $k=8$) | $25.000\times$ |
| **Cómputo Global ($N=200k$)** | $4 \times 10^{10}$ operaciones serie | **Grilla $15\times 15$ paralela (OpenMP)** | $\approx 200\times$ aceleración |
| **Pico Físico RAM (Linux)** | $> 320\text{ GB}$ (colapso del SO) | **$8.21\text{ MB}$** (medido en `/proc/self/status`) | $\mathbf{\approx 19.500\times}$ |
