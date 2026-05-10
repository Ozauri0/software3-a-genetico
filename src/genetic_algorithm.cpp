#include "genetic_algorithm.hpp"
#include <omp.h>
#include <algorithm>
#include <numeric>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

KnapsackGA::KnapsackGA(const KnapsackInstance& inst,
                       const GAParams&         params,
                       const PenaltyParams&    penalty)
    : inst_(inst), params_(params), penalty_(penalty), rng_(params.seed)
{}

// ---------------------------------------------------------------------------
// randomChromosome
// ---------------------------------------------------------------------------

std::vector<int> KnapsackGA::randomChromosome() {
    int n = (int)inst_.items.size();
    std::vector<int> chromo(n);
    std::uniform_int_distribution<int> bit(0, 1);
    for (int i = 0; i < n; ++i)
        chromo[i] = bit(rng_);
    return chromo;
}

// ---------------------------------------------------------------------------
// initialize
// ---------------------------------------------------------------------------

void KnapsackGA::initialize() {
    int n = (int)inst_.items.size();
    population_.resize(params_.pop_size);
    fitness_values_.resize(params_.pop_size, 0.0);
    generation_ = 0;

    for (int i = 0; i < params_.pop_size; ++i)
        population_[i] = randomChromosome();

    evaluatePopulation();
    updateBest();
}

// ---------------------------------------------------------------------------
// evaluatePopulation
// ---------------------------------------------------------------------------
// Paralelizacion (Variante 2):
//   - Cada iteracion i es independiente: lee population_[i] (const durante el loop)
//     y escribe fitness_values_[i] (indice privado por iteracion).
//   - inst_ y penalty_ son const -> sin condicion de carrera.
//   - Se usa schedule(dynamic) para balancear carga si los cromosomas varian.
//   - El RNG no se usa aqui, por lo que no hay estado compartido mutable.
// ---------------------------------------------------------------------------

void KnapsackGA::evaluatePopulation() {
    int pop = (int)population_.size();

#ifdef _OPENMP
    omp_set_num_threads(params_.num_threads);
#endif

    #pragma omp parallel for schedule(dynamic) \
        shared(population_, fitness_values_, inst_, penalty_) \
        firstprivate(pop) \
        default(none)
    for (int i = 0; i < pop; ++i) {
        fitness_values_[i] = computeFitness(population_[i], inst_, penalty_);
    }
}

// ---------------------------------------------------------------------------
// tournamentSelect
// ---------------------------------------------------------------------------
// Paralelismo: este metodo recibe el RNG como parametro (thread_local en el
// llamador), por lo que no accede a estado compartido mutable.
// ---------------------------------------------------------------------------

int KnapsackGA::tournamentSelect(std::mt19937& rng) const {
    int pop = (int)population_.size();
    std::uniform_int_distribution<int> dist(0, pop - 1);

    int best_idx = dist(rng);
    for (int t = 1; t < params_.tournament_size; ++t) {
        int candidate = dist(rng);
        if (fitness_values_[candidate] > fitness_values_[best_idx])
            best_idx = candidate;
    }
    return best_idx;
}

// ---------------------------------------------------------------------------
// crossover  (cruce de un punto)
// ---------------------------------------------------------------------------

std::pair<std::vector<int>, std::vector<int>>
KnapsackGA::crossover(const std::vector<int>& p1,
                      const std::vector<int>& p2,
                      std::mt19937& rng) const {
    int n = (int)p1.size();
    std::uniform_int_distribution<int> dist(1, n - 1);
    int point = dist(rng);

    std::vector<int> c1(n), c2(n);
    for (int i = 0; i < point; ++i) { c1[i] = p1[i]; c2[i] = p2[i]; }
    for (int i = point; i < n; ++i)  { c1[i] = p2[i]; c2[i] = p1[i]; }

    return {c1, c2};
}

// ---------------------------------------------------------------------------
// mutate  (bit-flip)
// ---------------------------------------------------------------------------

void KnapsackGA::mutate(std::vector<int>& chromo, std::mt19937& rng) const {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    for (auto& bit : chromo)
        if (prob(rng) < params_.mutation_rate)
            bit ^= 1;
}

// ---------------------------------------------------------------------------
// applyElitism
// ---------------------------------------------------------------------------

void KnapsackGA::applyElitism(std::vector<std::vector<int>>& newPop) const {
    int pop = (int)population_.size();
    int elite = std::min(params_.elite_size, pop);

    // Indices ordenados por fitness descendente
    std::vector<int> idx(pop);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + elite, idx.end(),
        [&](int a, int b){ return fitness_values_[a] > fitness_values_[b]; });

    for (int i = 0; i < elite; ++i)
        newPop[i] = population_[idx[i]];
}

// ---------------------------------------------------------------------------
// updateBest
// ---------------------------------------------------------------------------

void KnapsackGA::updateBest() {
    int pop = (int)population_.size();
    for (int i = 0; i < pop; ++i) {
        if (fitness_values_[i] > best_fitness_ever_) {
            best_fitness_ever_ = fitness_values_[i];
            best_chromosome_   = population_[i];
        }
        if (isFeasible(population_[i], inst_)) {
            double val = totalValue(population_[i], inst_);
            if (val > best_feasible_value_) {
                best_feasible_value_ = val;
                best_feasible_       = population_[i];
                found_feasible_      = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// runOneGeneration
// ---------------------------------------------------------------------------
// Paralelismo (Variante 2 – operadores geneticos):
//   - Se genera la mitad de la nueva poblacion en paralelo.
//   - Cada hilo usa thread_local mt19937 sembrado con seed + thread_id
//     para garantizar reproducibilidad y evitar condiciones de carrera en RNG.
//   - population_ y fitness_values_ son leidos (no escritos) durante la
//     generacion de hijos; la escritura es en newPop[idx] privado por hilo.
//   - La elite se aplica de forma secuencial antes del loop paralelo.
// ---------------------------------------------------------------------------

void KnapsackGA::runOneGeneration() {
    int pop_size = params_.pop_size;
    int elite    = std::min(params_.elite_size, pop_size);

    std::vector<std::vector<int>> newPop(pop_size);

    // 1. Elitismo: los primeros `elite` lugares son los mejores actuales
    applyElitism(newPop);

    // 2. Generar el resto de la poblacion en paralelo
    //    Cada hilo tiene su propio RNG (thread_local) para evitar race conditions.
    int pairs_needed = (pop_size - elite + 1) / 2;

#ifdef _OPENMP
    omp_set_num_threads(params_.num_threads);
#endif

    #pragma omp parallel default(none) \
        shared(newPop, pairs_needed, elite, pop_size)
    {
        // RNG privado por hilo: sembrado con seed base + id del hilo
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
        std::mt19937 local_rng(params_.seed + tid + generation_ * 1000);

        #pragma omp for schedule(static)
        for (int pair = 0; pair < pairs_needed; ++pair) {
            int idx1 = elite + pair * 2;
            int idx2 = elite + pair * 2 + 1;

            // Seleccion por torneo (lectura de population_ y fitness_values_)
            int p1_idx = tournamentSelect(local_rng);
            int p2_idx = tournamentSelect(local_rng);

            std::vector<int> c1, c2;

            // Cruzamiento
            std::uniform_real_distribution<double> prob(0.0, 1.0);
            if (prob(local_rng) < params_.crossover_rate) {
                auto [ch1, ch2] = crossover(population_[p1_idx],
                                             population_[p2_idx],
                                             local_rng);
                c1 = std::move(ch1);
                c2 = std::move(ch2);
            } else {
                c1 = population_[p1_idx];
                c2 = population_[p2_idx];
            }

            // Mutacion
            mutate(c1, local_rng);
            mutate(c2, local_rng);

            if (idx1 < pop_size) newPop[idx1] = std::move(c1);
            if (idx2 < pop_size) newPop[idx2] = std::move(c2);
        }
    }

    population_ = std::move(newPop);
    evaluatePopulation();
    updateBest();
    ++generation_;
}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------

std::vector<int> KnapsackGA::run() {
    initialize();

    for (int g = 0; g < params_.max_gen; ++g)
        runOneGeneration();

    return found_feasible_ ? best_feasible_ : best_chromosome_;
}

// ---------------------------------------------------------------------------
// injectIndividuals  (modelo de islas)
// ---------------------------------------------------------------------------

void KnapsackGA::injectIndividuals(const std::vector<std::vector<int>>& newcomers) {
    if (newcomers.empty() || population_.empty()) return;

    int pop = (int)population_.size();
    // Reemplazar los peores individuos por los inmigrantes
    std::vector<int> idx(pop);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
        [&](int a, int b){ return fitness_values_[a] < fitness_values_[b]; });

    int k = std::min((int)newcomers.size(), pop);
    for (int i = 0; i < k; ++i) {
        population_[idx[i]] = newcomers[i];
        fitness_values_[idx[i]] = computeFitness(newcomers[i], inst_, penalty_);
    }
    updateBest();
}

// ---------------------------------------------------------------------------
// getTopK
// ---------------------------------------------------------------------------

std::vector<std::vector<int>> KnapsackGA::getTopK(int k) const {
    int pop = (int)population_.size();
    k = std::min(k, pop);

    std::vector<int> idx(pop);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + k, idx.end(),
        [&](int a, int b){ return fitness_values_[a] > fitness_values_[b]; });

    std::vector<std::vector<int>> top(k);
    for (int i = 0; i < k; ++i)
        top[i] = population_[idx[i]];
    return top;
}
