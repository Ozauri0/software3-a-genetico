#!/usr/bin/env bash
# ===========================================================================
# run_experiments.sh  –  Linux/Ubuntu
# Script de diseno experimental para INFO1194 - Actividad 2
# Equivalente a run_experiments.ps1 para sistemas Unix.
#
# Uso:
#   chmod +x run_experiments.sh          # (solo la primera vez)
#   ./run_experiments.sh                 # experimento completo
#   ./run_experiments.sh --clean         # limpiar CSVs antes de ejecutar
#   ./run_experiments.sh --skip-gen      # saltar generacion de instancias
#   ./run_experiments.sh --dry-run       # ver comandos sin ejecutar
#   ./run_experiments.sh --instance small --reps 3   # solo small, 3 reps
#
# IMPORTANTE: usar --clean cuando se quiera un experimento limpio desde cero.
# Sin --clean, los resultados se ACUMULAN en el CSV (riesgo de contaminacion).
#
# Compilacion requerida (desde la raiz del proyecto):
#   g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga
# ===========================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Valores por defecto de parametros
# ---------------------------------------------------------------------------
EXE="./mochila_ga"
REPS=15
RESULTS="results/resultados.csv"
INSTANCE="all"    # all | small | medium | large
SKIP_GEN=0
DRY_RUN=0
CLEAN=0

# ---------------------------------------------------------------------------
# Parseo de argumentos
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe)        EXE="$2";      shift 2 ;;
        --reps)       REPS="$2";     shift 2 ;;
        --results)    RESULTS="$2";  shift 2 ;;
        --instance)   INSTANCE="$2"; shift 2 ;;
        --skip-gen)   SKIP_GEN=1;    shift   ;;
        --dry-run)    DRY_RUN=1;     shift   ;;
        --clean)      CLEAN=1;       shift   ;;
        -h|--help)
            sed -n '2,20p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Opcion desconocida: $1. Use --help para ver las opciones." >&2
            exit 1
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Configuracion de instancias
# Formato: "label:data_path:gen:pop:mut"
# small : 100 items,  500 gen, pop 200, mut=0.02
# medium: 1000 items, 300 gen, pop 200, mut=0.02
# large : 10000 items, 500 gen, pop 150, mut=0.001
#   NOTA: para n=10000, mut=0.02 implica ~200 bits/individuo (busqueda aleatoria).
#   Se usa 0.001 (10 mutaciones/individuo) como balance exploracion/convergencia.
# ---------------------------------------------------------------------------
declare -A INST_GEN=( [small]=500  [medium]=300  [large]=500  )
declare -A INST_POP=( [small]=200  [medium]=200  [large]=150  )
declare -A INST_MUT=( [small]=0.02 [medium]=0.02 [large]=0.001 )
declare -A INST_NAME=(
    [small]="data/small"
    [medium]="data/medium"
    [large]="data/large"
)

# Determinar lista de instancias a procesar
if [[ "$INSTANCE" == "all" ]]; then
    INST_LIST=("small" "medium" "large")
elif [[ -n "${INST_NAME[$INSTANCE]+_}" ]]; then
    INST_LIST=("$INSTANCE")
else
    echo "Instancia desconocida: '$INSTANCE'. Use: all | small | medium | large" >&2
    exit 1
fi

THREADS_LIST=(1 2 4 8)

# Calcular total de ejecuciones
N_INST=${#INST_LIST[@]}
N_THREADS=${#THREADS_LIST[@]}
TOTAL_EXP=$(( N_INST * (REPS + N_THREADS * REPS * 2) ))
COUNT=0
T_START=$(date +%s)

# Colores ANSI (desactivados si no hay terminal)
if [[ -t 1 ]]; then
    CY="\033[36m"; YL="\033[33m"; GR="\033[32m"; GY="\033[90m"; NC="\033[0m"; DY="\033[33m"
else
    CY=""; YL=""; GR=""; GY=""; NC=""; DY=""
fi

# ---------------------------------------------------------------------------
# Funcion auxiliar: ejecutar o simular
# ---------------------------------------------------------------------------
run_experiment() {
    if [[ $DRY_RUN -eq 1 ]]; then
        echo -e "  ${GY}[DRY] $EXE $*${NC}"
    else
        "$EXE" "$@" 2>/dev/null
    fi
}

# ---------------------------------------------------------------------------
# Paso 0: Limpiar resultados anteriores (-–clean)
# ---------------------------------------------------------------------------
if [[ $CLEAN -eq 1 ]]; then
    RESULTS_DIR=$(dirname "$RESULTS")
    METRICS="${RESULTS_DIR}/metricas.csv"

    if [[ $DRY_RUN -eq 1 ]]; then
        echo -e "${GY}  [DRY][CLEAN] Se eliminarian filas de '$INSTANCE' en $RESULTS${NC}"
    elif [[ "$INSTANCE" == "all" ]]; then
        # Eliminar archivos completos
        for f in "$RESULTS" "$METRICS"; do
            if [[ -f "$f" ]]; then
                rm -f "$f"
                echo -e "${DY}  [CLEAN] Eliminado: $f${NC}"
            fi
        done
    else
        # Eliminar solo filas de la instancia seleccionada usando awk
        inst_path="${INST_NAME[$INSTANCE]}"
        for f in "$RESULTS" "$METRICS"; do
            if [[ -f "$f" ]]; then
                tmp=$(mktemp)
                removed=0
                while IFS= read -r line; do
                    if echo "$line" | grep -qF "$inst_path"; then
                        (( removed++ )) || true
                    else
                        echo "$line" >> "$tmp"
                    fi
                done < "$f"
                mv "$tmp" "$f"
                echo -e "${DY}  [CLEAN] $f: eliminadas $removed filas de '$INSTANCE'${NC}"
            fi
        done
    fi
    echo ""
fi

# Crear directorio de resultados si no existe
if [[ $DRY_RUN -eq 0 ]]; then
    mkdir -p "$(dirname "$RESULTS")"
fi

# Verificar que el ejecutable existe
if [[ $DRY_RUN -eq 0 && ! -x "$EXE" ]]; then
    echo "Error: no se encontro '$EXE'. Compile primero:" >&2
    echo "  g++ -O2 -fopenmp -std=c++17 src/*.cpp -o mochila_ga" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Paso 1: Generar instancias
# ---------------------------------------------------------------------------
if [[ $SKIP_GEN -eq 0 ]]; then
    echo -e "\n${CY}=== Generando instancias ===${NC}"
    for label in "${INST_LIST[@]}"; do
        echo "  Generando $label ..."
        run_experiment --generate "$label" --seed 42
        [[ $DRY_RUN -eq 0 ]] && echo -e "  ${GR}OK: $label${NC}"
    done
fi

# ---------------------------------------------------------------------------
# Paso 2: Ejecutar experimentos
# ---------------------------------------------------------------------------
echo -e "\n${CY}=== Ejecutando $TOTAL_EXP experimentos ($REPS semillas x $N_INST instancias) ===${NC}"

for label in "${INST_LIST[@]}"; do
    inst_name="${INST_NAME[$label]}"
    gen="${INST_GEN[$label]}"
    pop="${INST_POP[$label]}"
    mut="${INST_MUT[$label]}"

    echo -e "\n${YL}--- Instancia: $label (gen=$gen, pop=$pop, mut=$mut) ---${NC}"

    # ------------------------------------------------------------------
    # Variante 1: Secuencial (1 hilo) – linea base para speed-up
    # ------------------------------------------------------------------
    echo "  [V1] sequential  (1 hilo, $REPS repeticiones)"
    for seed in $(seq 1 "$REPS"); do
        run_experiment \
            --instance "$inst_name" \
            --variant  sequential   \
            --threads  1            \
            --seed     "$seed"      \
            --gen      "$gen"       \
            --pop      "$pop"       \
            --mut      "$mut"       \
            --results  "$RESULTS"
        (( COUNT++ )) || true
        pct=$(awk "BEGIN{printf \"%.1f\", 100.0*$COUNT/$TOTAL_EXP}")
        printf "    [%d/%d %s%%] seq seed=%d" "$COUNT" "$TOTAL_EXP" "$pct" "$seed"
        [[ $DRY_RUN -eq 0 ]] && echo " OK" || echo ""
    done

    # ------------------------------------------------------------------
    # Variante 2: Paralelo con OpenMP (1, 2, 4, 8 hilos)
    # ------------------------------------------------------------------
    echo "  [V2] parallel    (1-2-4-8 hilos, $REPS repeticiones cada uno)"
    for t in "${THREADS_LIST[@]}"; do
        for seed in $(seq 1 "$REPS"); do
            run_experiment \
                --instance "$inst_name" \
                --variant  parallel     \
                --threads  "$t"         \
                --seed     "$seed"      \
                --gen      "$gen"       \
                --pop      "$pop"       \
                --mut      "$mut"       \
                --results  "$RESULTS"
            (( COUNT++ )) || true
            pct=$(awk "BEGIN{printf \"%.1f\", 100.0*$COUNT/$TOTAL_EXP}")
            printf "    [%d/%d %s%%] parallel t=%d seed=%d" "$COUNT" "$TOTAL_EXP" "$pct" "$t" "$seed"
            [[ $DRY_RUN -eq 0 ]] && echo " OK" || echo ""
        done
    done

    # ------------------------------------------------------------------
    # Variante 3: Modelo de islas (num_islands = num_threads)
    # ------------------------------------------------------------------
    echo "  [V3] islands     (1-2-4-8 islas, $REPS repeticiones cada uno)"
    for t in "${THREADS_LIST[@]}"; do
        for seed in $(seq 1 "$REPS"); do
            run_experiment \
                --instance    "$inst_name" \
                --variant     islands      \
                --threads     "$t"         \
                --islands     "$t"         \
                --seed        "$seed"      \
                --gen         "$gen"       \
                --pop         "$pop"       \
                --mut         "$mut"       \
                --mig-interval 25          \
                --migrants    2            \
                --results     "$RESULTS"
            (( COUNT++ )) || true
            pct=$(awk "BEGIN{printf \"%.1f\", 100.0*$COUNT/$TOTAL_EXP}")
            printf "    [%d/%d %s%%] islands n=%d seed=%d" "$COUNT" "$TOTAL_EXP" "$pct" "$t" "$seed"
            [[ $DRY_RUN -eq 0 ]] && echo " OK" || echo ""
        done
    done
done

# ---------------------------------------------------------------------------
# Paso 3: Calcular metricas agregadas
# ---------------------------------------------------------------------------
T_END=$(date +%s)
ELAPSED=$(( T_END - T_START ))
MIN=$(( ELAPSED / 60 ))
SEC=$(( ELAPSED % 60 ))
echo -e "\n${CY}=== Tiempo total: ${MIN}m ${SEC}s ===${NC}"

echo -e "\n${CY}=== Calculando metricas (speed-up, eficiencia, desviacion estandar) ===${NC}"
if [[ $DRY_RUN -eq 0 ]]; then
    "$EXE" --analyze --results "$RESULTS"
fi

RESULTS_DIR=$(dirname "$RESULTS")
METRICS="${RESULTS_DIR}/metricas.csv"

echo -e "\n${GR}=== EXPERIMENTOS COMPLETADOS ===${NC}"
echo "  Resultados : $RESULTS"
echo "  Metricas   : $METRICS"
echo ""
echo "Para volver a calcular metricas sin re-ejecutar:"
echo "  $EXE --analyze --results $RESULTS"
