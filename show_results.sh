#!/usr/bin/env bash
# show_results.sh — Visualizador de resultados experimentales
# Uso: ./show_results.sh [--metricas <path>] [--instancia <small|medium|large>]

METRICAS="results/metricas.csv"
INSTANCIA=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --metricas)  METRICAS="$2"; shift 2 ;;
        --instancia) INSTANCIA="$2"; shift 2 ;;
        *) echo "Opción desconocida: $1"; exit 1 ;;
    esac
done

if [[ ! -f "$METRICAS" ]]; then
    echo "No se encontró: $METRICAS"; exit 1
fi

# Leer CSV (skip header)
IFS=',' read -r -a HDR < "$METRICAS"

filtrar() {
    # $1 = campo a filtrar (por nombre de columna), $2 = valor
    awk -F',' -v col="$1" -v val="$2" '
        NR==1 { for(i=1;i<=NF;i++) if($i==col){idx=i}; next }
        (val=="" || $idx ~ val) { print }
    ' "$METRICAS"
}

fmt_row() {
    awk -F',' '{
        printf "  %-12s %-6s %-9s %10.1f %10.1f %12.0f %10.1f %9.3f %8.3f %7.1f\n",
            $1, $2, gensub(/data\//,"","g",$3),
            $5, $6, $7, $8, $12, $13, $11
    }'
}

ROWS=$(filtrar "instancia" "$INSTANCIA")

# ─── Tabla principal ──────────────────────────────────────────────────────────
echo ""
echo "=== TABLA DE MÉTRICAS ==="
printf "  %-12s %-6s %-9s %10s %10s %12s %10s %9s %8s %7s\n" \
    Variante Hilos Instancia "AvgTime(ms)" "StdTime(ms)" AvgValor StdValor Speedup Efic. "%Fact."
printf "  %s\n" "$(printf '─%.0s' {1..95})"
echo "$ROWS" | fmt_row

# ─── Speedup parallel ─────────────────────────────────────────────────────────
echo ""
echo "=== SPEEDUP — variante parallel ==="
printf "  %-9s %-6s %9s %8s\n" Instancia Hilos Speedup Efic.
printf "  %s\n" "$(printf '─%.0s' {1..38})"
echo "$ROWS" | awk -F',' '$1=="parallel" {
    printf "  %-9s %-6s %9.3f %8.3f\n",
        gensub(/data\//,"","g",$3), $2, $12, $13
}'

# ─── Calidad: avg_valor ± std_valor ──────────────────────────────────────────
echo ""
echo "=== CALIDAD DE SOLUCIÓN (avg ± std) ==="
printf "  %-12s %-6s %-9s %20s %12s\n" Variante Hilos Instancia "AvgValor ± StdValor" MaxValor
printf "  %s\n" "$(printf '─%.0s' {1..65})"
echo "$ROWS" | awk -F',' '{
    printf "  %-12s %-6s %-9s %12.0f ± %-8.0f %12.0f\n",
        $1, $2, gensub(/data\//,"","g",$3), $7, $8, $10
}'

# ─── Ganancia islands vs sequential (large) ───────────────────────────────────
echo ""
echo "=== GANANCIA DE CALIDAD: islands vs sequential (large) ==="
SEQ_VAL=$(awk -F',' '$1=="sequential" && $3~/large/ {print $7; exit}' "$METRICAS")
if [[ -n "$SEQ_VAL" ]]; then
    awk -F',' -v base="$SEQ_VAL" '
        $1=="islands" && $3~/large/ {
            gain = ($7 - base) / base * 100
            printf "  Islands/%s hilos  avg=%d  vs seq=%d  → %+.2f%%\n",
                $2, $7, base, gain
        }
    ' "$METRICAS"
fi

echo ""
