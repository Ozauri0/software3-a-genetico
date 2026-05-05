# GUÍA DEL GRUPO – Cómo probar y ejecutar los experimentos

> Esta guía está pensada para integrantes del grupo que no participaron directamente en el código. Explica qué hace cada paso, qué busca y cómo verificarlo.

---

## Requisito previo: instalar el compilador

**Objetivo:** tener `g++` con soporte OpenMP disponible en Windows.

1. Descarga e instala **MSYS2** desde https://www.msys2.org (instalador `.exe`)
2. Abre la terminal **MSYS2 MinGW64** (búscala en el menú inicio)
3. Instala GCC:
   ```
   pacman -S mingw-w64-x86_64-gcc
   ```
4. Agrega `C:\msys64\mingw64\bin` al PATH de Windows:
   - Busca "Variables de entorno" en el menú inicio
   - En PATH del sistema, agrega esa ruta
5. Abre una nueva PowerShell y verifica: `g++ --version`

---

## Paso 1 – Compilar el proyecto

**Qué busca:** generar el ejecutable `mochila_ga.exe` a partir del código fuente.

```powershell
cd C:\Users\ozaur\Documents\software3-a-genetico
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
```

Si no aparece ningún mensaje de error → compilación exitosa. Deberías ver `mochila_ga.exe` en la carpeta.

---

## Paso 2 – Generar las instancias de prueba

**Qué busca:** crear los archivos CSV con los ítems y restricciones para las tres instancias requeridas por la rúbrica (pequeña, mediana, grande).

```powershell
./mochila_ga --generate small  --seed 42
./mochila_ga --generate medium --seed 42
./mochila_ga --generate large  --seed 42
```

Esto crea las carpetas `data/small/`, `data/medium/` y `data/large/`, cada una con:
- `items.csv` — los ítems (id, valor, peso, volumen, categoría)
- `category_rules.csv` — reglas de categoría (restricción blanda)
- `incompatibilities.csv` — pares incompatibles (restricción dura)
- `dependencies.csv` — dependencias entre ítems (restricción dura)
- `knapsack_config.csv` — capacidades W y V (40% del total)

**Verificación:** `Get-ChildItem data/small/` debe mostrar los 5 archivos.

---

## Paso 3 – Prueba rápida de las tres variantes

**Qué busca:** confirmar que las tres variantes del algoritmo funcionan y producen soluciones factibles.

### Variante 1 – Secuencial
Es la línea base. Corre con 1 hilo, sin paralelismo. Sirve para comparar tiempo vs. las versiones paralelas.
```powershell
./mochila_ga --instance data/small --variant sequential --seed 42
```

### Variante 2 – Paralelo con OpenMP
Misma lógica que la secuencial, pero la evaluación de fitness y los operadores genéticos se ejecutan en paralelo con 4 hilos.
```powershell
./mochila_ga --instance data/small --variant parallel --threads 4 --seed 42
```

### Variante 3 – Modelo de islas
Divide la población en 4 subpoblaciones independientes que evolucionan en paralelo. Cada 25 generaciones, los 2 mejores individuos de cada isla migran a la isla siguiente (topología de anillo).
```powershell
./mochila_ga --instance data/small --variant islands --threads 4 --seed 42
```

**Lo que debe aparecer en pantalla para cada variante:**
```
=== <variante> (seed=42) ===
  Valor total      : XXXX.XX
  Peso utilizado   : XXXX / XXXX
  Volumen utilizado: XXXX / XXXX
  Factible         : SI          <-- esto es lo crítico
  Tiempo           : XXX ms
  Resultado exportado a: results/resultados.csv
```

Si dice **Factible: SI**, la solución respeta todas las restricciones duras.

---

## Paso 4 – Ejecutar el diseño experimental completo

**Qué busca:** acumular 15 repeticiones por cada combinación de variante × hilos × instancia, tal como exige la rúbrica (sección 7). Esto genera los datos para calcular speed-up y eficiencia.

### En Windows (PowerShell)

```powershell
.\run_experiments.ps1
```

El script ejecuta automáticamente:
- 3 instancias (small, medium, large)
- 3 variantes (sequential, parallel, islands)
- 4 configuraciones de hilos (1, 2, 4, 8)
- 15 semillas distintas por cada combinación

Todos los resultados se acumulan en `results/resultados.csv`.

#### Opciones disponibles

| Parámetro | Descripción | Ejemplo |
|---|---|---|
| `-Clean` | **Limpia el CSV antes de empezar** para evitar contaminación con filas de pruebas anteriores | `-Clean` |
| `-Instance` | Ejecutar solo una instancia (`all` \| `small` \| `medium` \| `large`) | `-Instance large` |
| `-Reps` | Número de semillas / repeticiones (por defecto 15) | `-Reps 3` |
| `-SkipGen` | No regenerar instancias (si ya existen en `data/`) | `-SkipGen` |
| `-DryRun` | Ver los comandos que se ejecutarían sin ejecutar nada | `-DryRun` |
| `-Exe` | Ruta al ejecutable (por defecto `./mochila_ga`) | `-Exe ./mochila_ga` |

#### Ejemplos de uso

```powershell
# Experimento completo desde cero (recomendado para entrega final)
.\run_experiments.ps1 -Clean

# Solo instancia pequeña con 3 repeticiones (prueba rápida)
.\run_experiments.ps1 -Instance small -Reps 3

# Re-ejecutar large sin borrar los otros resultados
.\run_experiments.ps1 -Instance large -SkipGen -Clean

# Ver todos los comandos sin ejecutar nada
.\run_experiments.ps1 -DryRun
```

> **Importante:** siempre usa `-Clean` cuando quieras un experimento limpio. Sin él, las filas se acumulan y pueden contaminar las métricas con ejecuciones manuales previas.

---

### En Linux / Ubuntu (Bash)

Primero compila con soporte OpenMP:
```bash
sudo apt install g++ libomp-dev   # solo la primera vez
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
```

Luego habilita y ejecuta el script equivalente:
```bash
chmod +x run_experiments.sh   # solo la primera vez
./run_experiments.sh
```

#### Opciones disponibles (bash)

| Flag | Descripción | Ejemplo |
|---|---|---|
| `--clean` | Limpia el CSV antes de empezar | `--clean` |
| `--instance` | Solo una instancia (`all` \| `small` \| `medium` \| `large`) | `--instance large` |
| `--reps N` | Número de repeticiones (por defecto 15) | `--reps 3` |
| `--skip-gen` | No regenerar instancias | `--skip-gen` |
| `--dry-run` | Ver comandos sin ejecutar | `--dry-run` |
| `--exe` | Ruta al ejecutable | `--exe ./mochila_ga` |

#### Ejemplos de uso

```bash
# Experimento completo desde cero
./run_experiments.sh --clean

# Solo instancia pequeña con 3 repeticiones
./run_experiments.sh --instance small --reps 3

# Re-ejecutar large sin borrar los otros resultados
./run_experiments.sh --instance large --skip-gen --clean

# Ver todos los comandos sin ejecutar nada
./run_experiments.sh --dry-run
```

---

## Paso 5 – Calcular las métricas del informe

**Qué busca:** obtener las métricas obligatorias de la rúbrica (sección 8): tiempo promedio, desviación estándar, mejor valor factible, % soluciones factibles, speed-up y eficiencia paralela.

```powershell
./mochila_ga --analyze --results results/resultados.csv
```

Imprime una tabla en pantalla y guarda `results/metricas.csv`. Ejemplo de salida:

```
=== METRICAS EXPERIMENTALES ===
Variante    Hilos  Instancia     Avg(ms)  Std(ms)  Mejor Valor  % Factible  Speed-up  Efic.
sequential  1      data/small    260.00   5.10     3149.94      100.0       1.000     1.000
parallel    2      data/small    170.00   3.20     3165.00      100.0       1.529     0.765
parallel    4      data/small    145.00   2.80     3183.30      100.0       1.793     0.448
parallel    8      data/small    140.00   4.10     3180.00      100.0       1.857     0.232
islands     4      data/small    315.00   6.50     3186.80      100.0       0.825     0.206
```

El speed-up se calcula como $S_p = T_1 / T_p$ donde $T_1$ es el tiempo del secuencial con 1 hilo.

---

## Resumen del flujo completo

```
Compilar → Generar instancias → Prueba rápida → Experimento completo → Analizar métricas
```

```powershell
# 1. Compilar
g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga

# 2. Generar instancias
./mochila_ga --generate small  --seed 42
./mochila_ga --generate medium --seed 42
./mochila_ga --generate large  --seed 42

# 3. Prueba rápida (verificar que todo funciona)
./mochila_ga --instance data/small --variant sequential --seed 42
./mochila_ga --instance data/small --variant parallel --threads 4 --seed 42
./mochila_ga --instance data/small --variant islands --seed 42

# 4. Experimento completo (las 15 repeticiones de la rúbrica)
.\run_experiments.ps1 -Clean

# 5. Obtener métricas para el informe
./mochila_ga --analyze --results results/resultados.csv
```

---

## Archivos de resultado

| Archivo | Contenido |
|---|---|
| `results/resultados.csv` | Una fila por ejecución: variante, hilos, semilla, tiempo, fitness, valor factible, factible |
| `results/metricas.csv` | Una fila por configuración con promedios, desviación estándar, speed-up y eficiencia |

---

## Preguntas frecuentes

**¿Por qué la variante paralela da valores distintos con la misma semilla?**
Porque el RNG se siembra con `seed + thread_id + generacion * 1000`. Cada hilo tiene su propia secuencia aleatoria, lo que hace que el resultado varíe dependiendo del número de hilos. Esto es esperado y documentado.

**¿Qué pasa si `Factible: NO`?**
El programa siempre reporta la mejor solución factible encontrada durante toda la ejecución. Si aparece NO, significa que ningún individuo respetó todas las restricciones duras. Aumenta el número de generaciones (`--gen 1000`) o la población (`--pop 400`).

**¿Por qué el modelo de islas es más lento que el paralelo en instancias pequeñas?**
El overhead de inicializar 4 subpoblaciones y sincronizar migraciones supera el beneficio del paralelismo en instancias pequeñas. El beneficio se aprecia más en instancias grandes.

**¿Cómo reproducir exactamente un resultado?**
Usa la misma semilla (`--seed N`) y el mismo número de hilos. El resultado para `sequential` con 1 hilo es 100% determinista. Para `parallel` e `islands`, el resultado es reproducible por hilo.
