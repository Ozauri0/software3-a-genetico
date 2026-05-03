#include "island_model.hpp"
#include <omp.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

IslandModel::IslandModel(const KnapsackInstance& inst,
                         const GAParams&         ga_params,
                         const PenaltyParams&    penalty,
                         const IslandParams&     island_params)
    : inst_(inst), ga_params_(ga_params),
      penalty_(penalty), island_params_(island_params)
{}

// ---------------------------------------------------------------------------
// migrate  (topologia de anillo)
//
// Se ejecuta de forma secuencial tras un barrier de todas las islas.
// Algoritmo:
//   1. Cada isla exporta sus K mejores individuos a un buffer temporal.
//   2. Una vez recopilados TODOS los buffers (sin modificar ninguna isla),
//      se inyectan en la isla siguiente del anillo.
// Esto evita que una isla reciba inmigrantes de su vecina antes de que
// esta haya exportado los suyos propios.
// ---------------------------------------------------------------------------

void IslandModel::migrate(std::vector<KnapsackGA>& islands) const {
    int n = (int)islands.size();
    int k = island_params_.migrants_count;

    // Paso 1: recopilar migrantes de cada isla en buffer
    std::vector<std::vector<std::vector<int>>> buffers(n);
    for (int i = 0; i < n; ++i)
        buffers[i] = islands[i].getTopK(k);

    // Paso 2: inyectar en isla siguiente (anillo)
    for (int i = 0; i < n; ++i) {
        int dest = (i + 1) % n;
        islands[dest].injectIndividuals(buffers[i]);
    }
}

// ---------------------------------------------------------------------------
// collectBest
// ---------------------------------------------------------------------------

void IslandModel::collectBest(const std::vector<KnapsackGA>& islands) {
    for (const auto& island : islands) {
        if (island.foundFeasible() &&
            island.getBestFeasibleValue() > global_best_value_) {
            global_best_value_  = island.getBestFeasibleValue();
            global_best_chromo_ = island.getBestFeasibleChromo();
            global_found_       = true;
        }
    }
    // Fallback: si ninguna isla encontro solucion factible, tomar mejor fitness
    if (!global_found_) {
        double best_fit = -1e18;
        for (const auto& island : islands) {
            if (island.getBestFitnessFound() > best_fit) {
                best_fit            = island.getBestFitnessFound();
                global_best_chromo_ = island.getBestChromosome();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------
// Paralelismo:
//   - Cada isla evoluciona en su propio hilo via #pragma omp parallel for.
//   - Las islas NO comparten poblacion durante la evolucion -> sin race conditions.
//   - La migracion se ejecuta en secuencial tras un #pragma omp barrier.
//   - Escritura en islands[i] es exclusiva del hilo i (sin necesidad de critical).
// ---------------------------------------------------------------------------

std::vector<int> IslandModel::run() {
    int n     = island_params_.num_islands;
    int total = island_params_.total_generations;
    int mig   = island_params_.migration_interval;

    // Crear una isla por subpoblacion con semilla distinta por isla
    std::vector<KnapsackGA> islands;
    islands.reserve(n);
    for (int i = 0; i < n; ++i) {
        GAParams local_params   = ga_params_;
        local_params.seed       += i * 997;   // semilla unica por isla
        local_params.num_threads = 1;         // cada isla corre secuencial internamente
        islands.emplace_back(inst_, local_params, penalty_);
    }

    // Inicializar todas las islas
    #pragma omp parallel for num_threads(n) schedule(static) \
        shared(islands) firstprivate(n) default(none)
    for (int i = 0; i < n; ++i)
        islands[i].initialize();

    // Bucle principal de generaciones
    for (int gen = 0; gen < total; gen += mig) {
        int steps = std::min(mig, total - gen);

        // Evolucion paralela de islas durante `steps` generaciones
        #pragma omp parallel for num_threads(n) schedule(static) \
            shared(islands) firstprivate(n, steps) default(none)
        for (int i = 0; i < n; ++i) {
            for (int s = 0; s < steps; ++s)
                islands[i].runOneGeneration();
        }

        // Sincronizar antes de migrar (el barrier esta implicito al salir del parallel for)
        // Migracion en topologia de anillo (secuencial, segura)
        if (gen + steps < total)
            migrate(islands);
    }

    collectBest(islands);
    return global_best_chromo_;
}
