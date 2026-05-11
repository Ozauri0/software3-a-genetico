# Algoritmo Genético Paralelo – Problema de la Mochila Extendida
## INFO1194 – Actividad 2

Implementación en **C++17 con OpenMP** de un algoritmo genético para el problema de la mochila extendida (peso, volumen, categorías, incompatibilidades y dependencias). Incluye tres variantes: secuencial (línea base), paralela con OpenMP y modelo de islas.

---

## Estructura del proyecto

```
software3-a-genetico/
├── src/
│   ├── main.cpp                    # CLI, generador de instancias, punto de entrada
│   ├── instance_loader.hpp/.cpp    # Carga de CSVs
│   ├── fitness.hpp/.cpp            # Función de aptitud y penalizaciones
│   ├── genetic_algorithm.hpp/.cpp  # GA secuencial y paralelo (OpenMP)
│   └── island_model.hpp/.cpp       # Modelo de islas
├── data/
│   ├── small/    # 100 ítems
│   ├── medium/   # 1.000 ítems
│   └── large/    # 10.000 ítems
├── results/
│   ├── resultados.csv   # salida raw de cada ejecución (ignorado por git)
│   └── metricas.csv     # métricas agregadas (ignorado por git)
├── run_experiments.ps1  # diseño experimental completo (Windows/PowerShell)
├── run_experiments.sh   # diseño experimental completo (Linux/macOS/WSL)
├── show_results.ps1     # visualizador de métricas (Windows/PowerShell)
├── show_results.sh      # visualizador de métricas (Linux/macOS/WSL)
├── mochila_ga.exe       # ejecutable compilado (Windows)
├── mochila_ga           # ejecutable compilado (Linux)
├── .gitignore
└── README.md
```

---

## Requisitos

### Windows

1. Instalar **MSYS2** desde https://www.msys2.org
2. Abrir la terminal **MSYS2 MinGW64**
3. Instalar GCC con soporte OpenMP:
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   ```
4. Agregar `C:\msys64\mingw64\bin` al PATH del sistema
5. Verificar: `g++ --version` debe mostrar GCC ≥ 12

### Linux / macOS / WSL

GCC con OpenMP suele venir instalado. Si no:
```bash
# Ubuntu/Debian
sudo apt install g++
# Arch/CachyOS
sudo pacman -S gcc
```

---

## Compilación

Desde la raíz del proyecto:
```bash
# Windows (MinGW)
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga.exe

# Linux / macOS / WSL
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
```

---

## Modos de uso

### 1. Generar instancias
```bash
./mochila_ga --generate small|medium|large [--seed N] [--pct-w 0.40] [--pct-v 0.40]
```
| Instancia | Ítems | Capacidades |
|---|---|---|
| `small` | 100 | 40 % del total de peso/volumen |
| `medium` | 1.000 | 40 % del total de peso/volumen |
| `large` | 10.000 | 40 % del total de peso/volumen |

### 2. Ejecutar variantes
```bash
# Variante 1 – Secuencial (línea base)
./mochila_ga --instance data/small --variant sequential --seed 1

# Variante 2 – Paralelo con OpenMP
./mochila_ga --instance data/medium --variant parallel --threads 4 --seed 1

# Variante 3 – Modelo de islas
./mochila_ga --instance data/large --variant islands --threads 4 --seed 1 \
             --islands 4 --mig-interval 25 --migrants 2
```

### 3. Calcular métricas
Después de acumular resultados en `results/resultados.csv`:
```bash
./mochila_ga --analyze --results results/resultados.csv
```
Genera `results/metricas.csv` con: tiempo promedio y desviación estándar, valor promedio y desviación estándar, mejor valor factible, % factibles, speed-up $S_p = T_1/T_p$ y eficiencia $E_p = S_p/p$.

### 4. Diseño experimental completo (405 ejecuciones)
```powershell
# PowerShell – 15 semillas × 3 instancias × 9 configuraciones
.\run_experiments.ps1

# Filtrar por instancia o reducir repeticiones para prueba rápida
.\run_experiments.ps1 -Instance small -Reps 3

# Ver comandos sin ejecutar
.\run_experiments.ps1 -DryRun
```
```bash
# Linux/macOS/WSL
./run_experiments.sh
./run_experiments.sh --instancia small --reps 3
```
Las semillas usadas son `{1, 2, …, 15}`.

### 5. Visualizar resultados
```powershell
# PowerShell
.\show_results.ps1                       # todas las instancias
.\show_results.ps1 -Instancia large      # filtrar por instancia
```
```bash
# Linux/macOS/WSL
./show_results.sh
./show_results.sh --instancia large
```

---

## Opciones completas del ejecutable

| Opción | Descripción | Default |
|---|---|---|
| `--instance <ruta>` | Carpeta de instancia | — |
| `--variant <v>` | `sequential` \| `parallel` \| `islands` | — |
| `--threads <N>` | Hilos OpenMP | 1 |
| `--seed <N>` | Semilla aleatoria | 42 |
| `--pop <N>` | Tamaño de población | 200 |
| `--gen <N>` | Generaciones máximas | 500 |
| `--mut <f>` | Tasa de mutación | 0.02 |
| `--cross <f>` | Tasa de cruzamiento | 0.85 |
| `--elite <N>` | Elitismo | 5 |
| `--tournament <N>` | Tamaño del torneo | 5 |
| `--islands <N>` | Número de islas | 4 |
| `--mig-interval <N>` | Generaciones entre migraciones | 25 |
| `--migrants <N>` | Individuos migrantes por isla | 2 |
| `--results <archivo>` | Ruta al CSV de salida | `results/resultados.csv` |
| `--analyze` | Calcular métricas desde CSV | — |
| `--generate <tamaño>` | Generar instancia (`small`/`medium`/`large`) | — |
| `--pct-w <f>` | Fracción del peso total como capacidad W | 0.40 |
| `--pct-v <f>` | Fracción del volumen total como capacidad V | 0.40 |

---

## Formato de archivos CSV de entrada

**`items.csv`**
```
id,valor,peso,volumen,categoria
0,85.3000,12.4000,7.1000,cat0
```

**`category_rules.csv`** — restricciones blandas por categoría
```
categoria,minimo,maximo
cat0,2,10
```

**`incompatibilities.csv`** — pares que no pueden coexistir (restricción dura)
```
id_item_a,id_item_b
0,5
```

**`dependencies.csv`** — si seleccionas A, debes seleccionar B (restricción dura)
```
id_item,id_requerido
10,2
```

**`knapsack_config.csv`** — generado automáticamente por `--generate`
```
W,V
1089.3200,556.8630
```

---

## Restricciones duras y blandas

| Restricción | Tipo | Parámetro de penalización | Justificación |
|---|---|---|---|
| Exceso de peso (W) | **Dura** | α = 10 × max\_v × n | Supera cualquier ganancia posible |
| Exceso de volumen (V) | **Dura** | β = 10 × max\_v × n | Ídem |
| Incompatibilidades | **Dura** | δ = 10 × max\_v × n | Ídem |
| Dependencias | **Dura** | ε = 10 × max\_v × n | Ídem |
| Categorías (mín/máx) | **Blanda** | γ = avg\_v / 2 | Penaliza sin descartar |

La solución reportada es siempre la mejor **factible** encontrada (`getBestFeasible()`), nunca un individuo que viole restricciones duras.

---

## Métricas

$$S_p = \frac{T_1}{T_p} \qquad E_p = \frac{S_p}{p}$$

Calculadas automáticamente con `--analyze` y exportadas a `results/metricas.csv` (13 columnas):
`variante, hilos, instancia, n, avg_time_ms, std_time_ms, avg_valor, std_valor, max_fitness, max_valor_factible, pct_factibles, speedup, eficiencia`

---

## Notas de paralelismo

- **`evaluatePopulation()`**: `#pragma omp parallel for schedule(dynamic)` — índices independientes; `inst_` y `penalty_` son `const` (sin race condition).
- **Operadores genéticos**: `thread_local std::mt19937` sembrado con `seed + thread_id + gen*1000` — RNG privado por hilo.
- **Modelo de islas**: cada isla evoluciona en su propio hilo; la migración usa buffer de dos fases (exportar → inyectar) para evitar escrituras cruzadas.

---

## Guía para el grupo

Ver [GUIA_GRUPO.md](GUIA_GRUPO.md) para instrucciones paso a paso orientadas a los integrantes del grupo.


Implementación en **C++17 con OpenMP** de un algoritmo genético para el problema de la mochila extendida (peso, volumen, categorías, incompatibilidades y dependencias). Incluye tres variantes obligatorias: secuencial, paralela y modelo de islas.

---

## Estructura del proyecto

```
software3-a-genetico/
├── src/
│   ├── main.cpp                    # CLI, generador de instancias, punto de entrada
│   ├── instance_loader.hpp/.cpp    # Carga de CSVs
│   ├── fitness.hpp/.cpp            # Función de aptitud y penalizaciones
│   ├── genetic_algorithm.hpp/.cpp  # GA secuencial y paralelo (OpenMP)
│   └── island_model.hpp/.cpp       # Modelo de islas
├── data/
│   ├── small/    # 100 ítems
│   ├── medium/   # 1.000 ítems
│   └── large/    # 10.000 ítems
├── results/
│   ├── resultados.csv   # salida raw de cada ejecución
│   └── metricas.csv     # métricas agregadas (speed-up, eficiencia, etc.)
├── run_experiments.ps1  # script de diseño experimental completo
└── README.md
```

---

## Requisitos – Windows (sin compilador instalado)

1. Instalar **MSYS2** desde https://www.msys2.org
2. Abrir la terminal **MSYS2 MinGW64**
3. Instalar GCC con soporte OpenMP:
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   ```
4. Agregar `C:\msys64\mingw64\bin` al PATH del sistema
5. Verificar: `g++ --version` debe mostrar GCC ≥ 12

---

## Compilación

Desde la raíz del proyecto (terminal MSYS2 MinGW64 o PowerShell con g++ en PATH):
```bash
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
```

---

## Modos de uso

### Generar instancias
```bash
./mochila_ga --generate small|medium|large [--seed N] [--pct-w 0.40] [--pct-v 0.40]
```
| Instancia | Ítems | Categorías | Generaciones sugeridas |
|---|---|---|---|
| small | 100 | 5 | 500 |
| medium | 1.000 | 10 | 300 |
| large | 10.000 | 20 | 100 |

### Ejecutar variantes
```bash
# Variante 1 – Secuencial (línea base)
./mochila_ga --instance data/small --variant sequential --seed 42

# Variante 2 – Paralelo con OpenMP
./mochila_ga --instance data/medium --variant parallel --threads 4 --seed 42

# Variante 3 – Modelo de islas
./mochila_ga --instance data/large --variant islands --threads 4 --seed 42
```

### Calcular métricas experimentales
Después de acumular resultados en `results/resultados.csv`:
```bash
./mochila_ga --analyze --results results/resultados.csv
```
Genera `results/metricas.csv` con: tiempo promedio, desviación estándar, mejor valor factible, % factibles, speed-up $S_p = T_1/T_p$ y eficiencia $E_p = S_p/p$.

### Diseño experimental completo (rúbrica §7)
```powershell
# PowerShell – ejecuta todas las repeticiones y calcula métricas
.\run_experiments.ps1

# Solo instancia small para prueba rápida
.\run_experiments.ps1 -Instance small -Reps 3

# Ver comandos sin ejecutar
.\run_experiments.ps1 -DryRun
```

---

## Opciones completas

| Opción | Descripción | Default |
|---|---|---|
| `--instance <ruta>` | Carpeta de instancia | — |
| `--variant <v>` | `sequential` \| `parallel` \| `islands` | — |
| `--threads <N>` | Hilos OpenMP | 1 |
| `--seed <N>` | Semilla aleatoria | 42 |
| `--pop <N>` | Tamaño de población | 200 |
| `--gen <N>` | Generaciones | 500 |
| `--mut <f>` | Tasa de mutación | 0.02 |
| `--cross <f>` | Tasa de cruzamiento | 0.85 |
| `--elite <N>` | Elitismo | 5 |
| `--tournament <N>` | Tamaño del torneo | 5 |
| `--islands <N>` | Número de islas | 4 |
| `--mig-interval <N>` | Generaciones entre migraciones | 25 |
| `--migrants <N>` | Individuos migrantes por isla | 2 |
| `--results <archivo>` | Ruta al CSV de salida | `results/resultados.csv` |
| `--analyze` | Calcular métricas desde CSV | — |

---

## Formato de archivos CSV de entrada

**`items.csv`** — un ítem por fila
```
id,valor,peso,volumen,categoria
0,85.3000,12.4000,7.1000,cat0
```

**`category_rules.csv`** — restricciones blandas por categoría
```
categoria,minimo,maximo
cat0,2,10
```

**`incompatibilities.csv`** — pares que no pueden coexistir (restricción dura)
```
id_item_a,id_item_b
0,5
```

**`dependencies.csv`** — si seleccionas A, debes seleccionar B (restricción dura)
```
id_item,id_requerido
10,2
```

**`knapsack_config.csv`** — generado automáticamente por `--generate`
```
W,V
1089.3200,556.8630
```

---

## Restricciones duras y blandas

| Restricción | Tipo | Parámetro | Justificación |
|---|---|---|---|
| Exceso de peso (W) | **Dura** | α = 10 × max_v × n | Supera cualquier ganancia posible |
| Exceso de volumen (V) | **Dura** | β = 10 × max_v × n | Ídem |
| Incompatibilidades | **Dura** | δ = 10 × max_v × n | Ídem |
| Dependencias | **Dura** | ε = 10 × max_v × n | Ídem |
| Categorías (mín/máx) | **Blanda** | γ = avg_v / 2 | Penaliza sin descartar |

La solución reportada es siempre la mejor **factible** encontrada (`getBestFeasible()`), nunca el individuo de mayor fitness si viola restricciones duras.

---

## Métricas obligatorias (rúbrica §8)

$$S_p = \frac{T_1}{T_p} \qquad E_p = \frac{S_p}{p}$$

Todas las métricas se calculan automáticamente con `--analyze` y se exportan a `results/metricas.csv`.

---

## Notas de paralelismo

- **`evaluatePopulation()`**: `#pragma omp parallel for schedule(dynamic)` — los índices son independientes; `inst_` y `penalty_` son `const` (sin race condition).
- **Operadores genéticos**: `thread_local std::mt19937` sembrado con `seed + thread_id + gen*1000` — RNG privado por hilo.
- **Modelo de islas**: cada isla evoluciona en su propio hilo; la migración se realiza sobre un buffer temporal antes de inyectar, evitando escrituras cruzadas.

---

## Guía rápida para el grupo

Ver [GUIA_GRUPO.md](GUIA_GRUPO.md) para instrucciones paso a paso orientadas a los integrantes del grupo.


---

## Requisitos

### Windows (sin compilador instalado)

1. Instalar **MSYS2** desde https://www.msys2.org
2. Abrir la terminal **MSYS2 MinGW64**
3. Instalar GCC con OpenMP:
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   ```
4. Agregar al PATH del sistema: `C:\msys64\mingw64\bin`
5. Verificar instalación:
   ```bash
   g++ --version
   ```

---

## Compilación

Desde la raíz del proyecto:
```bash
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
```

---

## Uso

### 1. Generar instancias

```bash
# Instancia pequeña (100 ítems, seed=42)
./mochila_ga --generate small --seed 42

# Instancia mediana (1.000 ítems)
./mochila_ga --generate medium --seed 42

# Instancia grande (10.000 ítems)
./mochila_ga --generate large --seed 42

# Personalizar capacidades (porcentaje de peso/volumen total)
./mochila_ga --generate medium --seed 42 --pct-w 0.35 --pct-v 0.35
```

### 2. Ejecutar variantes

#### Variante 1 – Secuencial
```bash
./mochila_ga --instance data/small --variant sequential --seed 42
```

#### Variante 2 – Paralelo con OpenMP
```bash
./mochila_ga --instance data/medium --variant parallel --threads 4 --seed 42
```

#### Variante 3 – Modelo de islas
```bash
./mochila_ga --instance data/large --variant islands --threads 4 --seed 42 \
             --islands 4 --mig-interval 25 --migrants 2
```

### Opciones completas

| Opción | Descripción | Valor por defecto |
|---|---|---|
| `--instance <ruta>` | Ruta a carpeta de instancia | — |
| `--variant <v>` | `sequential`, `parallel`, `islands` | — |
| `--threads <N>` | Número de hilos OpenMP | 1 |
| `--seed <N>` | Semilla aleatoria | 42 |
| `--pop <N>` | Tamaño de población | 200 |
| `--gen <N>` | Número de generaciones | 500 |
| `--mut <f>` | Tasa de mutación | 0.02 |
| `--cross <f>` | Tasa de cruzamiento | 0.85 |
| `--elite <N>` | Tamaño del elitismo | 5 |
| `--tournament <N>` | Tamaño del torneo | 5 |
| `--islands <N>` | Número de islas | 4 |
| `--mig-interval <N>` | Generaciones entre migraciones | 25 |
| `--migrants <N>` | Individuos que migran por isla | 2 |
| `--results <archivo>` | Ruta al CSV de resultados | `results/resultados.csv` |

---

## Formato de archivos CSV

### `items.csv`
```
id,valor,peso,volumen,categoria
0,85.3,12.4,7.1,cat0
1,42.1,30.0,15.5,cat1
```

### `category_rules.csv`
```
categoria,minimo,maximo
cat0,2,10
cat1,1,8
```
> `-1` en mínimo/máximo indica sin restricción.

### `incompatibilities.csv`
```
id_item_a,id_item_b
0,5
3,7
```

### `dependencies.csv`
```
id_item,id_requerido
10,2
15,3
```

### `knapsack_config.csv` (generado automáticamente)
```
W,V
1234.56,789.01
```

---

## Restricciones duras y blandas

| Restricción | Tipo | Penalización |
|---|---|---|
| Exceso de peso (W) | **Dura** | α = 10 × valor_máx × n |
| Exceso de volumen (V) | **Dura** | β = 10 × valor_máx × n |
| Incompatibilidades | **Dura** | δ = 10 × valor_máx × n |
| Dependencias | **Dura** | ε = 10 × valor_máx × n |
| Categorías (mín/máx) | **Blanda** | γ = promedio_valor / 2 |

La solución final reportada es siempre la mejor solución **factible** encontrada durante toda la ejecución (respeta las 4 restricciones duras).

---

## Reproducir experimentos (diseño experimental mínimo)

```bash
# 15 repeticiones con semillas registradas, instancia mediana, 4 hilos
for seed in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
  ./mochila_ga --instance data/medium --variant parallel --threads 4 --seed $seed
done
```

Los resultados se acumulan en `results/resultados.csv`. El speed-up se calcula como:

$$S_p = \frac{T_1}{T_p} \qquad E_p = \frac{S_p}{p}$$

---

## Notas de paralelismo

- **`evaluatePopulation()`**: `#pragma omp parallel for schedule(dynamic)` — cada índice `i` es independiente; `inst_` y `penalty_` son `const` (solo lectura).
- **Operadores genéticos**: `thread_local std::mt19937` sembrado con `seed + thread_id + gen*1000` — RNG privado por hilo, sin condición de carrera.
- **Modelo de islas**: cada isla evoluciona en su propio hilo; la migración ocurre en un buffer temporal antes de inyectar, evitando escrituras cruzadas.
