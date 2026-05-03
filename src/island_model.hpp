#pragma once
#include "genetic_algorithm.hpp"
#include <vector>

// ---------------------------------------------------------------------------
// Parametros del modelo de islas
// ---------------------------------------------------------------------------

struct IslandParams {
    int num_islands        = 4;    // numero de islas (= num_threads recomendado)
    int migration_interval = 25;   // cada cuantas generaciones migrar
    int migrants_count     = 2;    // cuantos individuos emigran por isla
    int total_generations  = 500;  // generaciones totales a evolucionar
};

// ---------------------------------------------------------------------------
// IslandModel  –  Variante 3
//
// Cada isla es un KnapsackGA independiente. Las islas evolucionan en paralelo
// con #pragma omp parallel for. La migracion ocurre cada migration_interval
// generaciones y sigue una topologia de ANILLO: isla i -> isla (i+1) % n.
//
// Sincronizacion:
//   - #pragma omp barrier antes de la fase de migracion.
//   - Los migrantes se copian a un buffer temporal ANTES de modificar ninguna isla,
//     evitando condiciones de carrera entre islas vecinas.
// ---------------------------------------------------------------------------

class IslandModel {
public:
    IslandModel(const KnapsackInstance& inst,
                const GAParams&         ga_params,
                const PenaltyParams&    penalty,
                const IslandParams&     island_params);

    // Ejecuta el modelo de islas y retorna el mejor cromosoma factible global.
    std::vector<int> run();

    // Estadisticas post-ejecucion
    double getBestFeasibleValue() const { return global_best_value_; }
    bool   foundFeasible()        const { return global_found_;       }

private:
    const KnapsackInstance& inst_;
    GAParams                ga_params_;
    PenaltyParams           penalty_;
    IslandParams            island_params_;

    double           global_best_value_ = -1e18;
    bool             global_found_      = false;
    std::vector<int> global_best_chromo_;

    void migrate(std::vector<KnapsackGA>& islands) const;
    void collectBest(const std::vector<KnapsackGA>& islands);
};
