#include "instance_loader.hpp"
#include "fitness.hpp"
#include "genetic_algorithm.hpp"
#include "island_model.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <filesystem>
#include <iomanip>
#include <map>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace fs = std::filesystem;

// ===========================================================================
// Utilidades de impresion
// ===========================================================================

static void printSolution(const std::vector<int>& chromo,
                           const KnapsackInstance& inst,
                           const std::string& label) {
    double val  = totalValue(chromo, inst);
    double peso = 0.0, vol = 0.0;
    for (int i = 0; i < (int)inst.items.size(); ++i) {
        if (chromo[i]) { peso += inst.items[i].peso; vol += inst.items[i].volumen; }
    }
    bool feasible = isFeasible(chromo, inst);

    std::cout << "\n=== " << label << " ===\n"
              << "  Valor total      : " << std::fixed << std::setprecision(2) << val << "\n"
              << "  Peso utilizado   : " << peso << " / " << inst.W << "\n"
              << "  Volumen utilizado: " << vol  << " / " << inst.V << "\n"
              << "  Factible         : " << (feasible ? "SI" : "NO") << "\n";
}

// ===========================================================================
// Generador de instancias
// ===========================================================================

static void generateInstance(const std::string& basePath, int n_items,
                              int n_categories, int n_incomp, int n_dep,
                              unsigned seed, double pct_w, double pct_v) {
    fs::create_directories(basePath);
    std::mt19937 rng(seed);

    // --- Categorias ---
    std::vector<std::string> cats;
    cats.reserve(n_categories);
    for (int c = 0; c < n_categories; ++c)
        cats.push_back("cat" + std::to_string(c));

    std::uniform_int_distribution<int>    cat_dist(0, n_categories - 1);
    std::uniform_real_distribution<double> val_dist(1.0, 100.0);
    std::uniform_real_distribution<double> w_dist(1.0, 50.0);
    std::uniform_real_distribution<double> v_dist(1.0, 30.0);

    // --- items.csv ---
    double total_peso = 0.0, total_vol = 0.0;
    {
        std::ofstream f(basePath + "/items.csv");
        f << "id,valor,peso,volumen,categoria\n";
        for (int i = 0; i < n_items; ++i) {
            double p = w_dist(rng), v = v_dist(rng);
            total_peso += p; total_vol += v;
            f << i << ","
              << std::fixed << std::setprecision(4) << val_dist(rng) << ","
              << p << "," << v << ","
              << cats[cat_dist(rng)] << "\n";
        }
    }

    // --- category_rules.csv ---
    {
        std::ofstream f(basePath + "/category_rules.csv");
        f << "categoria,minimo,maximo\n";
        int items_per_cat = n_items / n_categories;
        std::uniform_int_distribution<int> min_dist(0, std::max(1, items_per_cat / 4));
        std::uniform_int_distribution<int> max_off(items_per_cat / 2, items_per_cat);
        for (const auto& c : cats) {
            int mn = min_dist(rng);
            int mx = mn + max_off(rng);
            f << c << "," << mn << "," << mx << "\n";
        }
    }

    // --- incompatibilities.csv ---
    {
        std::ofstream f(basePath + "/incompatibilities.csv");
        f << "id_item_a,id_item_b\n";
        std::uniform_int_distribution<int> id_dist(0, n_items - 1);
        int written = 0;
        int attempts = 0;
        std::set<std::pair<int,int>> seen;
        while (written < n_incomp && attempts < n_incomp * 20) {
            int a = id_dist(rng), b = id_dist(rng);
            ++attempts;
            if (a == b) continue;
            if (a > b) std::swap(a, b);
            if (seen.count({a, b})) continue;
            seen.insert({a, b});
            f << a << "," << b << "\n";
            ++written;
        }
    }

    // --- dependencies.csv ---
    {
        std::ofstream f(basePath + "/dependencies.csv");
        f << "id_item,id_requerido\n";
        std::uniform_int_distribution<int> id_dist(0, n_items - 1);
        int written = 0;
        int attempts = 0;
        std::set<int> used;
        while (written < n_dep && attempts < n_dep * 20) {
            int a = id_dist(rng), b = id_dist(rng);
            ++attempts;
            if (a == b) continue;
            if (used.count(a)) continue;  // cada item como origen solo una vez
            used.insert(a);
            f << a << "," << b << "\n";
            ++written;
        }
    }

    // --- knapsack_config.csv (W y V) ---
    {
        std::ofstream f(basePath + "/knapsack_config.csv");
        double W = pct_w * total_peso;
        double V = pct_v * total_vol;
        f << "W,V\n" << std::fixed << std::setprecision(4) << W << "," << V << "\n";
    }

    std::cout << "[generate] Instancia generada en: " << basePath
              << " (" << n_items << " items, W=" << std::fixed
              << std::setprecision(1) << pct_w*100 << "% del peso total)\n";
}

// ===========================================================================
// Lectura de W y V desde knapsack_config.csv
// ===========================================================================

static std::pair<double,double> readConfig(const std::string& basePath) {
    std::ifstream f(basePath + "/knapsack_config.csv");
    if (!f.is_open())
        throw std::runtime_error("No se encontro knapsack_config.csv en: " + basePath);
    std::string line;
    std::getline(f, line); // cabecera
    std::getline(f, line);
    std::stringstream ss(line);
    std::string wstr, vstr;
    std::getline(ss, wstr, ',');
    std::getline(ss, vstr, ',');
    return {std::stod(wstr), std::stod(vstr)};
}

// ===========================================================================
// Exportar resultado a CSV
// ===========================================================================

static void exportResult(const std::string& csv_path,
                          const std::string& variant,
                          int threads, unsigned seed,
                          const std::string& instance,
                          double time_ms, double best_fitness,
                          double best_feasible_value, bool feasible) {
    bool exists = fs::exists(csv_path);
    std::ofstream f(csv_path, std::ios::app);
    if (!exists)
        f << "variante,hilos,semilla,instancia,tiempo_ms,"
             "mejor_fitness,mejor_valor_factible,factible\n";
    f << variant << ","
      << threads << ","
      << seed    << ","
      << instance << ","
      << std::fixed << std::setprecision(4) << time_ms << ","
      << best_fitness << ","
      << best_feasible_value << ","
      << (feasible ? "1" : "0") << "\n";
}

// ===========================================================================
// Parseo de argumentos
// ===========================================================================

struct RunConfig {
    std::string mode;        // "run" o "generate"
    std::string instance;
    std::string variant;     // sequential | parallel | islands
    int         threads  = 1;
    unsigned    seed     = 42;
    // parametros GA
    int    pop_size   = 200;
    int    max_gen    = 500;
    double mut_rate   = 0.02;
    double cross_rate = 0.85;
    int    elite      = 5;
    int    tournament = 5;
    // modelo de islas
    int    islands    = 4;
    int    mig_int    = 25;
    int    migrants   = 2;
    // generador
    std::string gen_size;  // small | medium | large
    double pct_w = 0.40;
    double pct_v = 0.40;
    // salida
    std::string results_csv = "results/resultados.csv";
};

static void printUsage(const char* prog) {
    std::cout <<
        "Uso:\n"
        "  " << prog << " --generate small|medium|large [--seed N] [--pct-w 0.40] [--pct-v 0.40]\n"
        "  " << prog << " --instance <ruta> --variant sequential|parallel|islands\n"
        "              [--threads N] [--seed N] [--pop N] [--gen N]\n"
        "              [--mut 0.02] [--cross 0.85] [--elite N] [--tournament N]\n"
        "              [--islands N] [--mig-interval N] [--migrants N]\n"
        "              [--results <archivo.csv>]\n";
}

static RunConfig parseArgs(int argc, char* argv[]) {
    RunConfig cfg;
    std::map<std::string, std::string> args;

    for (int i = 1; i < argc - 1; ++i)
        if (argv[i][0] == '-' && argv[i][1] == '-')
            args[argv[i] + 2] = argv[i + 1];

    // Determinar modo
    if (args.count("generate")) {
        cfg.mode     = "generate";
        cfg.gen_size = args["generate"];
    } else if (args.count("instance")) {
        cfg.mode     = "run";
        cfg.instance = args["instance"];
    } else {
        cfg.mode = "help";
        return cfg;
    }

    auto getInt    = [&](const std::string& k, int    def){ return args.count(k) ? std::stoi(args[k])   : def; };
    auto getDbl    = [&](const std::string& k, double def){ return args.count(k) ? std::stod(args[k])   : def; };
    auto getStr    = [&](const std::string& k, const std::string& def){ return args.count(k) ? args[k] : def; };

    cfg.seed      = (unsigned)getInt("seed", 42);
    cfg.pct_w     = getDbl("pct-w", 0.40);
    cfg.pct_v     = getDbl("pct-v", 0.40);

    if (cfg.mode == "run") {
        cfg.variant   = getStr("variant", "sequential");
        cfg.threads   = getInt("threads", 1);
        cfg.pop_size  = getInt("pop", 200);
        cfg.max_gen   = getInt("gen", 500);
        cfg.mut_rate  = getDbl("mut", 0.02);
        cfg.cross_rate= getDbl("cross", 0.85);
        cfg.elite     = getInt("elite", 5);
        cfg.tournament= getInt("tournament", 5);
        cfg.islands   = getInt("islands", 4);
        cfg.mig_int   = getInt("mig-interval", 25);
        cfg.migrants  = getInt("migrants", 2);
        cfg.results_csv = getStr("results", "results/resultados.csv");
    }
    return cfg;
}

// ===========================================================================
// Analisis de resultados experimentales
// Calcula: avg_time, std_time, max_fitness, max_valor_factible, pct_factibles,
//          speed-up (Sp = T1/Tp) y eficiencia paralela (Ep = Sp/p)
// ===========================================================================

static void analyzeResults(const std::string& results_path) {
    struct Row {
        std::string variante, instancia;
        int    hilos, semilla;
        double tiempo_ms, mejor_fitness, mejor_valor_factible;
        bool   factible;
    };

    std::vector<Row> rows;
    {
        std::ifstream f(results_path);
        if (!f.is_open()) {
            std::cerr << "No se puede abrir: " << results_path << "\n";
            return;
        }
        // Lambda para dividir una linea CSV en tokens
        auto split = [](const std::string& s) {
            std::vector<std::string> tok;
            std::stringstream ss(s);
            std::string t;
            while (std::getline(ss, t, ',')) {
                size_t a = t.find_first_not_of(" \t\r\n");
                size_t b = t.find_last_not_of(" \t\r\n");
                tok.push_back(a == std::string::npos ? "" : t.substr(a, b - a + 1));
            }
            return tok;
        };
        std::string line;
        std::getline(f, line); // cabecera
        while (std::getline(f, line)) {
            auto tok = split(line);
            if (tok.size() < 8) continue;
            Row r;
            r.variante             = tok[0];
            r.hilos                = std::stoi(tok[1]);
            r.semilla              = std::stoi(tok[2]);
            r.instancia            = tok[3];
            r.tiempo_ms            = std::stod(tok[4]);
            r.mejor_fitness        = std::stod(tok[5]);
            r.mejor_valor_factible = std::stod(tok[6]);
            r.factible             = (tok[7] == "1");
            rows.push_back(r);
        }
    }

    if (rows.empty()) {
        std::cerr << "Sin datos en: " << results_path << "\n";
        return;
    }

    // Agrupar por (variante, hilos, instancia)
    using Key = std::tuple<std::string, int, std::string>;
    std::map<Key, std::vector<const Row*>> groups;
    for (auto& r : rows)
        groups[{r.variante, r.hilos, r.instancia}].push_back(&r);

    struct Stats {
        std::string variante, instancia;
        int    hilos, n;
        double avg_time, std_time;
        double avg_valor, std_valor;
        double max_fitness, max_valor_factible;
        double pct_factibles;
        double speedup = 0.0, eficiencia = 0.0;
    };

    std::vector<Stats> all_stats;
    for (auto& [key, grp] : groups) {
        Stats s;
        s.variante  = std::get<0>(key);
        s.hilos     = std::get<1>(key);
        s.instancia = std::get<2>(key);
        s.n         = (int)grp.size();
        s.max_fitness          = -1e18;
        s.max_valor_factible   = 0.0;
        double sum_t = 0.0, sum_t2 = 0.0;
        double sum_v = 0.0, sum_v2 = 0.0;
        int factibles = 0;
        for (auto* r : grp) {
            sum_t  += r->tiempo_ms;
            sum_t2 += r->tiempo_ms * r->tiempo_ms;
            sum_v  += r->mejor_valor_factible;
            sum_v2 += r->mejor_valor_factible * r->mejor_valor_factible;
            if (r->mejor_fitness > s.max_fitness)
                s.max_fitness = r->mejor_fitness;
            if (r->mejor_valor_factible > s.max_valor_factible)
                s.max_valor_factible = r->mejor_valor_factible;
            if (r->factible) ++factibles;
        }
        s.avg_time      = sum_t / s.n;
        double var_t    = (sum_t2 / s.n) - (s.avg_time * s.avg_time);
        s.std_time      = (var_t > 0.0) ? std::sqrt(var_t) : 0.0;
        s.avg_valor     = sum_v / s.n;
        double var_v    = (sum_v2 / s.n) - (s.avg_valor * s.avg_valor);
        s.std_valor     = (var_v > 0.0) ? std::sqrt(var_v) : 0.0;
        s.pct_factibles = 100.0 * factibles / s.n;
        all_stats.push_back(s);
    }

    // Speed-up: T1 = avg_time del grupo (sequential, 1 hilo) para la misma instancia.
    // Fallback: si no hay sequential, se usa parallel con 1 hilo.
    // Formula: Sp = T1 / Tp  ;  Ep = Sp / p
    std::map<std::string, double> t1_map; // instancia -> T1
    for (auto& s : all_stats)
        if (s.variante == "sequential" && s.hilos == 1)
            t1_map[s.instancia] = s.avg_time;
    for (auto& s : all_stats)
        if (s.variante == "parallel" && s.hilos == 1 && !t1_map.count(s.instancia))
            t1_map[s.instancia] = s.avg_time;

    for (auto& s : all_stats) {
        auto it = t1_map.find(s.instancia);
        if (it != t1_map.end() && s.avg_time > 0.0) {
            s.speedup    = it->second / s.avg_time;
            s.eficiencia = s.speedup / s.hilos;
        }
    }

    // Exportar metricas.csv al mismo directorio que resultados.csv
    std::string dir;
    {
        auto pos = results_path.find_last_of("/\\");
        dir = (pos != std::string::npos) ? results_path.substr(0, pos + 1) : "";
    }
    std::string metrics_path = dir + "metricas.csv";
    {
        std::ofstream f(metrics_path);
        f << "variante,hilos,instancia,n,avg_time_ms,std_time_ms,"
             "avg_valor,std_valor,max_fitness,max_valor_factible,pct_factibles,speedup,eficiencia\n";
        for (auto& s : all_stats)
            f << s.variante << "," << s.hilos << "," << s.instancia << ","
              << s.n << ","
              << std::fixed << std::setprecision(4)
              << s.avg_time << "," << s.std_time << ","
              << s.avg_valor << "," << s.std_valor << ","
              << s.max_fitness << "," << s.max_valor_factible << ","
              << std::setprecision(1) << s.pct_factibles << ","
              << std::setprecision(4) << s.speedup << ","
              << s.eficiencia << "\n";
    }

    // Imprimir tabla en consola
    const int C1=12, C2=7, C3=14, C4=12, C5=12, C6=16, C7=16, C8=12, C9=10, C10=10;
    std::cout << "\n=== METRICAS EXPERIMENTALES ===\n"
              << std::left
              << std::setw(C1) << "Variante"
              << std::setw(C2) << "Hilos"
              << std::setw(C3) << "Instancia"
              << std::setw(C4) << "Avg(ms)"
              << std::setw(C5) << "Std(ms)"
              << std::setw(C6) << "AvgValor"
              << std::setw(C7) << "StdValor"
              << std::setw(C8) << "% Factible"
              << std::setw(C9) << "Speed-up"
              << std::setw(C10) << "Efic." << "\n"
              << std::string(C1+C2+C3+C4+C5+C6+C7+C8+C9+C10, '-') << "\n";
    for (auto& s : all_stats)
        std::cout << std::left
                  << std::setw(C1) << s.variante
                  << std::setw(C2) << s.hilos
                  << std::setw(C3) << s.instancia
                  << std::fixed << std::setprecision(2)
                  << std::setw(C4) << s.avg_time
                  << std::setw(C5) << s.std_time
                  << std::setw(C6) << s.avg_valor
                  << std::setw(C7) << s.std_valor
                  << std::setprecision(1)
                  << std::setw(C8) << s.pct_factibles
                  << std::setprecision(3)
                  << std::setw(C9) << s.speedup
                  << std::setw(C10) << s.eficiencia << "\n";

    std::cout << "\nMetricas exportadas a: " << metrics_path << "\n";
}

// ===========================================================================
// main
// ===========================================================================

int main(int argc, char* argv[]) {
    // Modo --analyze: calcular metricas desde CSV de resultados
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--analyze") {
            std::string rpath = "results/resultados.csv";
            for (int j = i + 1; j < argc - 1; ++j)
                if (std::string(argv[j]) == "--results") rpath = argv[j + 1];
            analyzeResults(rpath);
            return 0;
        }
    }

    if (argc < 2) { printUsage(argv[0]); return 1; }

    RunConfig cfg = parseArgs(argc, argv);

    // -----------------------------------------------------------------------
    // Modo: generar instancias
    // -----------------------------------------------------------------------
    if (cfg.mode == "generate") {
        struct InstSpec { std::string name; int n; int cats; int incomp; int dep; };
        std::map<std::string, InstSpec> specs = {
            { "small",  { "data/small",  100,    5,   10,   5  } },
            { "medium", { "data/medium", 1000,  10,   50,  25  } },
            { "large",  { "data/large",  10000, 20,  200, 100  } },
        };
        auto it = specs.find(cfg.gen_size);
        if (it == specs.end()) {
            std::cerr << "Tamano invalido. Use: small | medium | large\n";
            return 1;
        }
        const auto& sp = it->second;
        generateInstance(sp.name, sp.n, sp.cats, sp.incomp, sp.dep,
                         cfg.seed, cfg.pct_w, cfg.pct_v);
        return 0;
    }

    if (cfg.mode == "help") { printUsage(argv[0]); return 0; }

    // -----------------------------------------------------------------------
    // Modo: ejecutar algoritmo
    // -----------------------------------------------------------------------

    // Cargar instancia
    KnapsackInstance inst;
    try {
        auto [W, V] = readConfig(cfg.instance);
        inst = InstanceLoader::load(cfg.instance, W, V);
    } catch (const std::exception& e) {
        std::cerr << "Error al cargar instancia: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Instancia cargada: " << inst.items.size() << " items, "
              << "W=" << inst.W << ", V=" << inst.V << "\n";

    // Calibrar penalizaciones
    PenaltyParams penalty = PenaltyParams::fromInstance(inst);

    // Configurar GA
    GAParams ga;
    ga.pop_size       = cfg.pop_size;
    ga.max_gen        = cfg.max_gen;
    ga.mutation_rate  = cfg.mut_rate;
    ga.crossover_rate = cfg.cross_rate;
    ga.elite_size     = cfg.elite;
    ga.tournament_size= cfg.tournament;
    ga.seed           = (int)cfg.seed;
    ga.num_threads    = cfg.threads;

    // -----------------------------------------------------------------------
    // Ejecutar variante
    // -----------------------------------------------------------------------
    std::vector<int> result;
    bool found_feasible = false;
    double best_fitness = 0.0, best_feasible_value = 0.0;

    auto t_start = std::chrono::high_resolution_clock::now();

    if (cfg.variant == "sequential" || cfg.variant == "parallel") {
        // Variante 1 (sequential: num_threads=1) o Variante 2 (parallel: num_threads>1)
        // La diferencia esta en ga.num_threads que activa #pragma omp en evaluatePopulation
        if (cfg.variant == "sequential") ga.num_threads = 1;

        KnapsackGA solver(inst, ga, penalty);
        result            = solver.run();
        found_feasible    = solver.foundFeasible();
        best_fitness      = solver.getBestFitnessFound();
        best_feasible_value = solver.getBestFeasibleValue();

    } else if (cfg.variant == "islands") {
        IslandParams ip;
        ip.num_islands        = cfg.islands;
        ip.migration_interval = cfg.mig_int;
        ip.migrants_count     = cfg.migrants;
        ip.total_generations  = cfg.max_gen;

        IslandModel solver(inst, ga, penalty, ip);
        result              = solver.run();
        found_feasible      = solver.foundFeasible();
        best_feasible_value = solver.getBestFeasibleValue();
        if (!result.empty())
            best_fitness = computeFitness(result, inst, penalty);

    } else {
        std::cerr << "Variante desconocida: " << cfg.variant << "\n";
        return 1;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // -----------------------------------------------------------------------
    // Reportar resultado
    // -----------------------------------------------------------------------
    printSolution(result, inst, cfg.variant + " (seed=" + std::to_string(cfg.seed) + ")");
    std::cout << "  Tiempo           : " << std::fixed << std::setprecision(2)
              << time_ms << " ms\n";

    // Crear directorio de resultados si no existe
    {
        auto pos = cfg.results_csv.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string dir = cfg.results_csv.substr(0, pos);
            fs::create_directories(dir);
        }
    }

    exportResult(cfg.results_csv, cfg.variant, cfg.threads, cfg.seed,
                 cfg.instance, time_ms, best_fitness,
                 (found_feasible ? best_feasible_value : 0.0), found_feasible);

    std::cout << "  Resultado exportado a: " << cfg.results_csv << "\n";

    return 0;
}
