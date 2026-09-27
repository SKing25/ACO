# Optimización por Colonia de Hormigas (ACO) para el TSP

Implementación en C++17 de alto rendimiento y bajo consumo de memoria del algoritmo de **Optimización por Colonia de Hormigas (ACO)** para resolver el Problema del Viajante de Comercio (*Traveling Salesperson Problem*, TSP) sobre instancias euclidianas uniformes en tres escalas de tamaño: **$N = 20$**, **$N = 2.000$** y **$N = 200.000$** ciudades.

---

## 📁 Estructura del Repositorio

```text
ACO/
├── src/                  # Código fuente C++ de alto rendimiento
│   ├── aco.cpp           # Implementación principal optimizada (8.2 MB pico para 200k)
│   └── Makefile          # Makefile interno del código fuente
│
├── bin/                  # Binarios ejecutables compilados
│   ├── aco               # Binario principal
│   └── aco1              # Binario del benchmark oficial
│
├── data/                 # Instancias generadas y resultados experimentales
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
│   ├── graficas.py       # Generador de curvas (log-log, memoria, calidad, tours)
│   └── semillas.py       # Evaluación de robustez estadística multi-semilla
│
├── Informe_Tecnico/       # Informe académico completo en LaTeX y PDF
│   ├── informe_aco.tex   # Código fuente LaTeX del informe (20 páginas)
│   ├── informe_aco.pdf   # Documento compilado final
│   ├── Makefile          # Reglas para compilar el PDF con pdflatex
│   └── img/              # Figuras experimentales y capturas de código
│       ├── fig_tiempo.png
│       ├── fig_memoria.png
│       ├── fig_calidad.png
│       ├── fig_hormigas.png
│       ├── fig_tours.png
│       ├── fig_semillas.png
│       ├── vscode_editor_real.png
│       └── code_snippets/ # 14 capturas modulares de VS Code
│
├── docs/                 # Documentación y material de referencia
│   ├── 07_IA_2026_2.pdf  # Diapositivas guía de la asignatura (Sesión 07)
│   ├── COMPARATIVA.md    # Análisis comparativo técnico
│   └── INFORME.md        # Resumen ejecutivo en formato Markdown
│
├── Makefile              # Makefile raíz que orquesta compilación y ejecución
├── README.md             # Documento de descripción y guía de uso
└── informe_aco.pdf -> Informe_Tecnico/informe_aco.pdf  # Acceso directo al informe
```

---

## 🚀 Compilación y Ejecución

### Requisitos
* Compilador C++17 compatible con OpenMP (`g++` recomendado).
* Python 3 con `numpy` y `matplotlib`.
* Distribución TeX Live con `pdflatex` (para compilar el informe).

### Comandos Rápidos
* **Compilar el proyecto:**
  ```bash
  make
  ```
* **Ejecutar con parámetros por defecto (20, 2.000 y 200.000 ciudades):**
  ```bash
  make run
  ```
* **Ejecutar una escala específica (ej. 200.000 ciudades):**
  ```bash
  ./bin/aco -n 200000
  ```
* **Recompilar el informe en PDF:**
  ```bash
  make informe
  ```
* **Limpiar binarios y temporales:**
  ```bash
  make clean
  ```

---

## 📊 Arquitectura de Ahorro Extremo de Recursos

| Dimensión | Matriz Densa Ingenua ($N \times N$) | Arquitectura Optimizada (`src/aco.cpp`) | Factor de Ahorro |
|---|---|---|---|
| **Distancias ($N=200k$)** | $160\text{ GB}$ en RAM | **$0\text{ MB}$** (cálculo euclidiano al vuelo) | $\infty$ |
| **Feromonas ($N=200k$)** | $160\text{ GB}$ en RAM | **$6.4\text{ MB}$** (dispersa $k$-NN con $k=8$) | $25.000\times$ |
| **Cómputo Global ($N=200k$)** | $4 \times 10^{10}$ operaciones serie | **Grilla $15\times 15$ paralela (OpenMP)** | $\approx 200\times$ aceleración |
| **Pico Físico RAM (Linux)** | $> 320\text{ GB}$ (colapso del SO) | **$8.21\text{ MB}$** (medido en `/proc/self/status`) | $\mathbf{\approx 19.500\times}$ |
