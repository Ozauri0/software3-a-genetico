#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>

// ---------------------------------------------------------------------------
// Estructuras de datos del problema
// ---------------------------------------------------------------------------

struct Item {
    int         id;
    double      valor;
    double      peso;
    double      volumen;
    std::string categoria;
};

struct CategoryRule {
    std::string categoria;
    int         minimo;   // -1 = sin restriccion de minimo
    int         maximo;   // -1 = sin restriccion de maximo
};

struct KnapsackInstance {
    std::vector<Item>                         items;
    double                                    W;          // capacidad de peso
    double                                    V;          // capacidad de volumen
    std::vector<CategoryRule>                 cat_rules;
    std::set<std::pair<int,int>>              incompatibilities;  // pares (a<b)
    std::map<int,int>                         dependencies;       // si item a, requiere item b
    std::map<int,int>                         id_to_index;        // id -> posicion en items[]

    // Totales precalculados (utiles para generar instancias y validar W/V)
    double total_peso    = 0.0;
    double total_volumen = 0.0;
    double max_valor     = 0.0;
    double avg_valor     = 0.0;
};

// ---------------------------------------------------------------------------
// Cargador de instancias desde archivos CSV
// ---------------------------------------------------------------------------

class InstanceLoader {
public:
    // Carga los 4 CSVs desde basePath/:
    //   items.csv, category_rules.csv, incompatibilities.csv, dependencies.csv
    // W y V se pasan explicitamente (calculados fuera segun porcentaje)
    static KnapsackInstance load(const std::string& basePath,
                                 double W,
                                 double V);

    // Carga solo los items y calcula W = pct_w * total_peso, V = pct_v * total_vol
    static KnapsackInstance loadWithPercent(const std::string& basePath,
                                            double pct_w = 0.40,
                                            double pct_v = 0.40);

private:
    static void loadItems         (const std::string& path, KnapsackInstance& inst);
    static void loadCategoryRules (const std::string& path, KnapsackInstance& inst);
    static void loadIncompat      (const std::string& path, KnapsackInstance& inst);
    static void loadDependencies  (const std::string& path, KnapsackInstance& inst);
    static void computeStats      (KnapsackInstance& inst);
};
