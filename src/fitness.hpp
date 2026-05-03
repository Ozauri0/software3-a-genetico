#pragma once
#include "instance_loader.hpp"
#include <vector>

// ---------------------------------------------------------------------------
// Parametros de penalizacion
// ---------------------------------------------------------------------------
// Las penalizaciones duras (alpha, beta, delta, epsilon) deben ser lo
// suficientemente grandes para que cualquier violacion supere el beneficio
// maximo posible. Se recomiendan valores basados en max_valor * n.
//
// La penalizacion blanda (gamma_cat) es moderada: penaliza categorias fuera
// de rango pero no descarta al individuo inmediatamente.
// ---------------------------------------------------------------------------

struct PenaltyParams {
    double alpha;      // peso excedido  [DURA]
    double beta;       // volumen exc.   [DURA]
    double gamma_cat;  // categorias     [BLANDA]
    double delta;      // incompatib.    [DURA]
    double epsilon;    // dependencias   [DURA]

    // Construye parametros calibrados automaticamente segun la instancia.
    // alpha = beta = delta = epsilon = 10 * max_valor
    // gamma_cat = avg_valor / 2
    static PenaltyParams fromInstance(const KnapsackInstance& inst);
};

// ---------------------------------------------------------------------------
// Funciones de aptitud
// ---------------------------------------------------------------------------

// Valor total de los items seleccionados (sin penalizaciones)
double totalValue(const std::vector<int>& chromo,
                  const KnapsackInstance& inst);

// --- Funciones de penalizacion individuales --------------------------------

// Exceso de peso (0 si no supera W)
double excesoPeso(const std::vector<int>& chromo,
                  const KnapsackInstance& inst);

// Exceso de volumen (0 si no supera V)
double excesoVolumen(const std::vector<int>& chromo,
                     const KnapsackInstance& inst);

// Numero de pares incompatibles activos [DURA]
int countIncompatibilidades(const std::vector<int>& chromo,
                             const KnapsackInstance& inst);

// Numero de dependencias incumplidas:
// item_a seleccionado pero item_requerido NO seleccionado [DURA]
int countDependencias(const std::vector<int>& chromo,
                      const KnapsackInstance& inst);

// Suma de violaciones de reglas de categoria (|conteo - min| + |conteo - max|) [BLANDA]
int violacionesCategoria(const std::vector<int>& chromo,
                         const KnapsackInstance& inst);

// --- Funcion principal de aptitud ------------------------------------------

double computeFitness(const std::vector<int>& chromo,
                      const KnapsackInstance& inst,
                      const PenaltyParams&    params);

// Evalua si un cromosoma es factible respecto SOLO a restricciones DURAS:
//   - peso <= W
//   - volumen <= V
//   - sin incompatibilidades activas
//   - sin dependencias incumplidas
bool isFeasible(const std::vector<int>& chromo,
                const KnapsackInstance& inst);
