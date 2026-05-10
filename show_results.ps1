# show_results.ps1 - Visualizador de resultados
param(
    [string]$Metricas  = "results\metricas.csv",
    [string]$Instancia = ""
)

if (-not (Test-Path $Metricas)) { Write-Error "No se encontro: $Metricas"; exit 1 }

$filas = Import-Csv $Metricas
if ($Instancia) { $filas = $filas | Where-Object { $_.instancia -like "*$Instancia*" } }

Write-Host ""
Write-Host "=== TABLA DE METRICAS ===" -ForegroundColor Cyan
$filas | Select-Object `
    @{N="Variante";   E={$_.variante}},
    @{N="Hilos";      E={$_.hilos}},
    @{N="Instancia";  E={$_.instancia -replace "data/",""}},
    @{N="AvgTime(ms)";E={[math]::Round([double]$_.avg_time_ms,1)}},
    @{N="StdTime(ms)";E={[math]::Round([double]$_.std_time_ms,1)}},
    @{N="AvgValor";   E={[math]::Round([double]$_.avg_valor,0)}},
    @{N="StdValor";   E={[math]::Round([double]$_.std_valor,1)}},
    @{N="Speedup";    E={[math]::Round([double]$_.speedup,3)}},
    @{N="Efic.";      E={[math]::Round([double]$_.eficiencia,3)}},
    @{N="PctFact";    E={$_.pct_factibles}} |
    Format-Table -AutoSize

Write-Host "=== SPEEDUP — variante parallel ===" -ForegroundColor Cyan
$filas | Where-Object { $_.variante -eq "parallel" } |
    Select-Object `
        @{N="Instancia";E={$_.instancia -replace "data/",""}},
        @{N="Hilos";    E={$_.hilos}},
        @{N="Speedup";  E={[math]::Round([double]$_.speedup,3)}},
        @{N="Efic.";    E={[math]::Round([double]$_.eficiencia,3)}} |
    Format-Table -AutoSize

Write-Host "=== CALIDAD DE SOLUCION (avg +/- std) ===" -ForegroundColor Cyan
$filas | ForEach-Object {
    [PSCustomObject]@{
        Variante  = $_.variante
        Hilos     = $_.hilos
        Instancia = $_.instancia -replace "data/",""
        "AvgVal +/- StdVal" = "$([math]::Round([double]$_.avg_valor,0)) +/- $([math]::Round([double]$_.std_valor,0))"
        MaxValor  = [math]::Round([double]$_.max_valor_factible,0)
    }
} | Format-Table -AutoSize

Write-Host "=== GANANCIA: islands vs sequential (large) ===" -ForegroundColor Cyan
$seq_large    = $filas | Where-Object { $_.variante -eq "sequential" -and $_.instancia -like "*large*" }
$island_large = $filas | Where-Object { $_.variante -eq "islands"    -and $_.instancia -like "*large*" }
if ($seq_large -and $island_large) {
    $base = [double]$seq_large.avg_valor
    foreach ($isl in $island_large) {
        $val  = [double]$isl.avg_valor
        $pct  = [math]::Round(($val - $base) / $base * 100, 2)
        $sign = if ($pct -ge 0) { "+" } else { "" }
        Write-Host "  islands/$($isl.hilos) hilos  avg=$([math]::Round($val,0))  seq=$([math]::Round($base,0))  gain=$sign$pct pct"
    }
}
Write-Host ""
