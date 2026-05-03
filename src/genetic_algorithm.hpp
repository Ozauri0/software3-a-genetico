#pragma once
#include "instance_loader.hpp"
#include "fitness.hpp"
#include <vector>
#include <random>
#include <string>
#include <limits>

// ---------------------------------------------------------------------------
// Parametros del algoritmo genetico
// ---------------------------------------------------------------------------

struct GAParams {
    int    pop_size        = 200;
    int    max_gen         = 500;
    double mutation_rate   = 0.02;
    double crossover_rate  = 0.85;
    int    elite_size      = 5;
    int    tournament_size = 5;
    int    seed            = 42;
    int    num_threads     = 1;   // usado en evaluatePopulation paralela
};

// ---------------------------------------------------------------------------
// KnapsackGA  –  Variante 1 (secuencial) y Variante 2 (paralela con OpenMP)
// La paralelizacion se activa cuando num_threads > 1 (via omp_set_num_threads)
// ---------------------------------------------------------------------------

class KnapsackGA {
public:
    KnapsackGA(const KnapsackInstance& inst,
               const GAParams&         params,
               const PenaltyParams&    penalty);

    // Ejecuta el algoritmo y retorna el mejor cromosoma FACTIBLE encontrado.
    // Si no se encontro ninguno factible, retorna el de mayor fitness.
    std::vector<int> run();

    // Acceso a estadisticas post-ejecucion
    double getBestFitnessFound()  const { return best_fitness_ever_;  }
    double getBestFeasibleValue() const { return best_feasible_value_; }
    bool   foundFeasible()        const { return found_feasible_;      }
    const std::vector<int>& getBestChromosome()  const { return best_chromosome_;  }
    const std::vector<int>& getBestFeasibleChromo() const { return best_feasible_; }

    // Permite inyectar individuos externos (usado por modelo de islas)
    void injectIndividuals(const std::vector<std::vector<int>>& newcomers);

    // Retorna los K mejores individuos de la poblacion actual por fitness
    std::vector<std::vector<int>> getTopK(int k) const;

    // Corre solo una generacion (usado por modelo de islas)
    void runOneGeneration();

    // Inicializa la poblacion (debe llamarse antes de runOneGeneration)
    void initialize();

    int  getGeneration() const { return generation_; }

private:
    // -----------------------------------------------------------------------
    // Datos (solo lectura fuera del constructor)
    // -----------------------------------------------------------------------
    const KnapsackInstance& inst_;
    GAParams                params_;
    PenaltyParams           penalty_;

    // -----------------------------------------------------------------------
    // Estado evolutivo
    // -----------------------------------------------------------------------
    std::vector<std::vector<int>> population_;
    std::vector<double>           fitness_values_;
    int                           generation_ = 0;

    // Mejor solucion global (mayor fitness, puede no ser factible)
    std::vector<int> best_chromosome_;
    double           best_fitness_ever_  = -std::numeric_limits<double>::max();

    // Mejor solucion factible encontrada en toda la ejecucion
    std::vector<int> best_feasible_;
    double           best_feasible_value_ = -std::numeric_limits<double>::max();
    bool             found_feasible_      = false;

    // RNG principal (secuencial)
    std::mt19937 rng_;

    // -----------------------------------------------------------------------
    // Operadores internos
    // -----------------------------------------------------------------------

    // Genera un cromosoma aleatorio de n bits
    std::vector<int> randomChromosome();

    // Evaluacion de fitness de toda la poblacion.
    // Usa #pragma omp parallel for cuando num_threads > 1.
    // Datos compartidos: inst_ (const), penalty_ (const), population_ (lectura)
    // Datos privados: indice i, resultado fitness_values_[i]
    void evaluatePopulation();

    // Seleccion por torneo de k individuos: retorna indice del ganador
    int tournamentSelect(std::mt19937& rng) const;

    // Cruce de un punto: retorna dos hijos
    std::pair<std::vector<int>, std::vector<int>>
    crossover(const std::vector<int>& p1,
              const std::vector<int>& p2,
              std::mt19937& rng) const;

    // Mutacion por bit-flip con probabilidad mutation_rate
    void mutate(std::vector<int>& chromo, std::mt19937& rng) const;

    // Copia los elite_size mejores de population_ al inicio de newPop
    void applyElitism(std::vector<std::vector<int>>& newPop) const;

    // Actualiza best_chromosome_ y best_feasible_ con la poblacion actual
    void updateBest();
};
