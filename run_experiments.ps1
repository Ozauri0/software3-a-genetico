# run_experiments.ps1
# ===========================================================================
# Script de diseno experimental para INFO1194 - Actividad 2
# Ejecuta las 15 repeticiones x 3 instancias x variantes/hilos requeridas
# por la rubrica y calcula las metricas agregadas al final.
#
# Uso:
#   .\run_experiments.ps1                          # experimento completo
#   .\run_experiments.ps1 -Clean                   # limpiar CSVs antes de ejecutar
#   .\run_experiments.ps1 -SkipGen                 # saltar generacion de instancias
#   .\run_experiments.ps1 -DryRun                  # ver comandos sin ejecutar
#   .\run_experiments.ps1 -Reps 3 -Instance small  # solo small, 3 repeticiones
#
# IMPORTANTE: usar -Clean cuando se quiera un experimento limpio desde cero.
# Sin -Clean, los resultados se ACUMULAN en el CSV existente (riesgo de
# contaminacion con filas de pruebas manuales o ejecuciones anteriores).
# ===========================================================================

param(
    [string] $Exe      = "./mochila_ga",
    [int]    $Reps     = 15,
    [string] $Results  = "results/resultados.csv",
    [string] $Instance = "all",   # all | small | medium | large
    [switch] $SkipGen,
    [switch] $DryRun,
    [switch] $Clean    # eliminar resultados anteriores antes de empezar
)

# ---------------------------------------------------------------------------
# Configuracion de instancias
# small : 100 items,  500 gen, pop 200  -> verificacion funcional
# medium: 1000 items, 300 gen, pop 200  -> comparacion de rendimiento
# large : 10000 items, 500 gen, pop 150, mut=0.001 -> evaluacion real del paralelismo
#   NOTA: para n=10000, mutation_rate=0.02 implica ~200 bits volteados por individuo
#   (busqueda aleatoria). Se usa 0.001 (10 mutaciones/individuo) como balance
#   entre exploracion y convergencia.
# ---------------------------------------------------------------------------
$AllInstances = @(
    @{ Label="small";  Name="data/small";  Gen=500; Pop=200; Mut=0.02  },
    @{ Label="medium"; Name="data/medium"; Gen=300; Pop=200; Mut=0.02  },
    @{ Label="large";  Name="data/large";  Gen=500; Pop=150; Mut=0.001 }
)

# Filtrar instancias segun parametro
# @() fuerza array para que .Count devuelva numero de elementos (no de claves del hashtable)
$Instances = @(if ($Instance -eq "all") { $AllInstances }
               else { $AllInstances | Where-Object { $_.Label -eq $Instance } })

if ($Instances.Count -eq 0) {
    Write-Host "Instancia desconocida: $Instance. Use: all | small | medium | large" -ForegroundColor Red
    exit 1
}

# Semillas registradas (reproducibles)
$Seeds = 1..$Reps

# Configuraciones de hilos (rubrica: 1, 2, 4, 8)
$ThreadsList = @(1, 2, 4, 8)

# ---------------------------------------------------------------------------
# Funcion de ejecucion
# ---------------------------------------------------------------------------
function Invoke-Experiment {
    param([string[]]$ArgList)
    if ($DryRun) {
        Write-Host ("  [DRY] $Exe " + ($ArgList -join " ")) -ForegroundColor DarkGray
    } else {
        & $Exe @ArgList 2>&1 | Out-Null
    }
}

# Calcular total de ejecuciones para mostrar progreso
$n_variants = 1 + $ThreadsList.Count + $ThreadsList.Count  # seq + parallel + islands
$total_exp  = $Instances.Count * ($Reps + ($ThreadsList.Count * $Reps * 2))
$count      = 0
$t_start    = Get-Date

# ---------------------------------------------------------------------------
# Paso 0: Limpiar resultados anteriores (si se pide -Clean)
# ---------------------------------------------------------------------------
if ($Clean) {
    $results_dir = Split-Path $Results -Parent
    $metrics     = Join-Path $results_dir "metricas.csv"

    if (-not $DryRun) {
        # Si -Instance es 'all', eliminar los archivos completos
        if ($Instance -eq "all") {
            if (Test-Path $Results) {
                Remove-Item $Results -Force
                Write-Host "  [CLEAN] Eliminado: $Results" -ForegroundColor DarkYellow
            }
            if (Test-Path $metrics) {
                Remove-Item $metrics -Force
                Write-Host "  [CLEAN] Eliminado: $metrics" -ForegroundColor DarkYellow
            }
        } else {
            # Solo eliminar filas de la instancia seleccionada del CSV
            $instNames = $Instances | ForEach-Object { $_.Name }
            foreach ($csv in @($Results, $metrics)) {
                if (Test-Path $csv) {
                    $h     = Get-Content $csv | Select-Object -First 1
                    $rows  = Get-Content $csv | Select-Object -Skip 1
                    $clean = $rows | Where-Object {
                        $line = $_
                        -not ($instNames | Where-Object { $line -match [regex]::Escape($_) })
                    }
                    ($h + "`n" + ($clean -join "`n")) | Set-Content $csv -Encoding UTF8
                    $removed = $rows.Count - $clean.Count
                    Write-Host "  [CLEAN] ${csv}: eliminadas $removed filas de '$Instance'" -ForegroundColor DarkYellow
                }
            }
        }
    } else {
        Write-Host "  [DRY][CLEAN] Se eliminarian filas de '$Instance' en $Results" -ForegroundColor DarkGray
    }
    Write-Host ""
}

# ---------------------------------------------------------------------------
# Paso 1: Generar instancias
# ---------------------------------------------------------------------------
if (-not $SkipGen) {
    Write-Host "`n=== Generando instancias ===" -ForegroundColor Cyan
    foreach ($inst in $Instances) {
        Write-Host "  Generando $($inst.Label) ..."
        Invoke-Experiment @("--generate", $inst.Label, "--seed", "42")
        if (-not $DryRun) { Write-Host "  OK: $($inst.Label)" -ForegroundColor Green }
    }
}

# ---------------------------------------------------------------------------
# Paso 2: Ejecutar experimentos
# ---------------------------------------------------------------------------
Write-Host "`n=== Ejecutando $total_exp experimentos ($Reps semillas x $($Instances.Count) instancias) ===" -ForegroundColor Cyan

foreach ($inst in $Instances) {
    Write-Host "`n--- Instancia: $($inst.Label) (gen=$($inst.Gen), pop=$($inst.Pop), mut=$($inst.Mut)) ---" -ForegroundColor Yellow

    # ------------------------------------------------------------------
    # Variante 1: Secuencial (1 hilo) - linea base para speed-up
    # ------------------------------------------------------------------
    Write-Host "  [V1] sequential  (1 hilo, $Reps repeticiones)"
    foreach ($seed in $Seeds) {
        Invoke-Experiment @(
            "--instance", $inst.Name,
            "--variant",  "sequential",
            "--threads",  "1",
            "--seed",     "$seed",
            "--gen",      "$($inst.Gen)",
            "--pop",      "$($inst.Pop)",
            "--mut",      "$($inst.Mut)",
            "--results",  $Results
        )
        $count++
        $pct = [math]::Round(100.0 * $count / $total_exp, 1)
        Write-Host ("    [$count/$total_exp ${pct}%] seq seed=$seed") -NoNewline
        if (-not $DryRun) { Write-Host " OK" } else { Write-Host "" }
    }

    # ------------------------------------------------------------------
    # Variante 2: Paralelo con OpenMP (1, 2, 4, 8 hilos)
    # ------------------------------------------------------------------
    Write-Host "  [V2] parallel    (1-2-4-8 hilos, $Reps repeticiones cada uno)"
    foreach ($t in $ThreadsList) {
        foreach ($seed in $Seeds) {
            Invoke-Experiment @(
                "--instance", $inst.Name,
                "--variant",  "parallel",
                "--threads",  "$t",
                "--seed",     "$seed",
                "--gen",      "$($inst.Gen)",
                "--pop",      "$($inst.Pop)",
                "--mut",      "$($inst.Mut)",
                "--results",  $Results
            )
            $count++
            $pct = [math]::Round(100.0 * $count / $total_exp, 1)
            Write-Host ("    [$count/$total_exp ${pct}%] parallel t=$t seed=$seed") -NoNewline
            if (-not $DryRun) { Write-Host " OK" } else { Write-Host "" }
        }
    }

    # ------------------------------------------------------------------
    # Variante 3: Modelo de islas (num_islands = num_threads)
    # ------------------------------------------------------------------
    Write-Host "  [V3] islands     (1-2-4-8 islas, $Reps repeticiones cada uno)"
    foreach ($t in $ThreadsList) {
        foreach ($seed in $Seeds) {
            Invoke-Experiment @(
                "--instance",    $inst.Name,
                "--variant",     "islands",
                "--threads",     "$t",
                "--islands",     "$t",
                "--seed",        "$seed",
                "--gen",         "$($inst.Gen)",
                "--pop",         "$($inst.Pop)",
                "--mut",         "$($inst.Mut)",
                "--mig-interval","25",
                "--migrants",    "2",
                "--results",     $Results
            )
            $count++
            $pct = [math]::Round(100.0 * $count / $total_exp, 1)
            Write-Host ("    [$count/$total_exp ${pct}%] islands n=$t seed=$seed") -NoNewline
            if (-not $DryRun) { Write-Host " OK" } else { Write-Host "" }
        }
    }
}

# ---------------------------------------------------------------------------
# Paso 3: Calcular metricas agregadas
# ---------------------------------------------------------------------------
$elapsed = (Get-Date) - $t_start
Write-Host "`n=== Tiempo total: $([math]::Round($elapsed.TotalMinutes, 2)) min ===" -ForegroundColor Cyan
Write-Host "`n=== Calculando metricas (speed-up, eficiencia, desviacion estandar) ===" -ForegroundColor Cyan
if (-not $DryRun) {
    & $Exe --analyze --results $Results
}

$metrics_dir  = Split-Path $Results -Parent
$metrics_file = Join-Path $metrics_dir "metricas.csv"

Write-Host "`n=== EXPERIMENTOS COMPLETADOS ===" -ForegroundColor Green
Write-Host "  Resultados : $Results"
Write-Host "  Metricas   : $metrics_file"
Write-Host "`nPara volver a calcular metricas sin re-ejecutar:"
Write-Host "  $Exe --analyze --results $Results" -ForegroundColor DarkGray
