#include "instance_loader.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <numeric>

// ---------------------------------------------------------------------------
// Helpers internos
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> splitCSV(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ','))
        tokens.push_back(trim(token));
    return tokens;
}

// ---------------------------------------------------------------------------
// InstanceLoader::loadItems
// ---------------------------------------------------------------------------

void InstanceLoader::loadItems(const std::string& path, KnapsackInstance& inst) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("No se puede abrir: " + path);

    std::string line;
    std::getline(f, line); // saltar cabecera

    while (std::getline(f, line)) {
        if (trim(line).empty()) continue;
        auto tok = splitCSV(line);
        if (tok.size() < 5) continue;

        Item item;
        item.id       = std::stoi(tok[0]);
        item.valor    = std::stod(tok[1]);
        item.peso     = std::stod(tok[2]);
        item.volumen  = std::stod(tok[3]);
        item.categoria = tok[4];
        inst.items.push_back(item);
    }
}

// ---------------------------------------------------------------------------
// InstanceLoader::loadCategoryRules
// ---------------------------------------------------------------------------

void InstanceLoader::loadCategoryRules(const std::string& path, KnapsackInstance& inst) {
    std::ifstream f(path);
    if (!f.is_open()) return; // opcional

    std::string line;
    std::getline(f, line); // cabecera

    while (std::getline(f, line)) {
        if (trim(line).empty()) continue;
        auto tok = splitCSV(line);
        if (tok.size() < 3) continue;

        CategoryRule rule;
        rule.categoria = tok[0];
        rule.minimo    = std::stoi(tok[1]);
        rule.maximo    = std::stoi(tok[2]);
        inst.cat_rules.push_back(rule);
    }
}

// ---------------------------------------------------------------------------
// InstanceLoader::loadIncompat
// ---------------------------------------------------------------------------

void InstanceLoader::loadIncompat(const std::string& path, KnapsackInstance& inst) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    std::getline(f, line); // cabecera

    while (std::getline(f, line)) {
        if (trim(line).empty()) continue;
        auto tok = splitCSV(line);
        if (tok.size() < 2) continue;

        int a = std::stoi(tok[0]);
        int b = std::stoi(tok[1]);
        if (a > b) std::swap(a, b);
        inst.incompatibilities.insert({a, b});
    }
}

// ---------------------------------------------------------------------------
// InstanceLoader::loadDependencies
// ---------------------------------------------------------------------------

void InstanceLoader::loadDependencies(const std::string& path, KnapsackInstance& inst) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    std::getline(f, line); // cabecera

    while (std::getline(f, line)) {
        if (trim(line).empty()) continue;
        auto tok = splitCSV(line);
        if (tok.size() < 2) continue;

        int item_id     = std::stoi(tok[0]);
        int required_id = std::stoi(tok[1]);
        inst.dependencies[item_id] = required_id;
    }
}

// ---------------------------------------------------------------------------
// InstanceLoader::computeStats
// ---------------------------------------------------------------------------

void InstanceLoader::computeStats(KnapsackInstance& inst) {
    inst.total_peso    = 0.0;
    inst.total_volumen = 0.0;
    inst.max_valor     = 0.0;
    double sum_valor   = 0.0;

    for (int i = 0; i < (int)inst.items.size(); ++i) {
        const auto& item = inst.items[i];
        inst.id_to_index[item.id] = i;
        inst.total_peso    += item.peso;
        inst.total_volumen += item.volumen;
        sum_valor          += item.valor;
        if (item.valor > inst.max_valor)
            inst.max_valor = item.valor;
    }

    inst.avg_valor = inst.items.empty() ? 0.0
                                        : sum_valor / inst.items.size();
}

// ---------------------------------------------------------------------------
// InstanceLoader::load  (W y V dados explicitamente)
// ---------------------------------------------------------------------------

KnapsackInstance InstanceLoader::load(const std::string& basePath,
                                      double W,
                                      double V) {
    KnapsackInstance inst;
    inst.W = W;
    inst.V = V;

    loadItems         (basePath + "/items.csv",             inst);
    loadCategoryRules (basePath + "/category_rules.csv",    inst);
    loadIncompat      (basePath + "/incompatibilities.csv", inst);
    loadDependencies  (basePath + "/dependencies.csv",      inst);
    computeStats(inst);

    return inst;
}

// ---------------------------------------------------------------------------
// InstanceLoader::loadWithPercent  (W/V = pct * total)
// ---------------------------------------------------------------------------

KnapsackInstance InstanceLoader::loadWithPercent(const std::string& basePath,
                                                  double pct_w,
                                                  double pct_v) {
    KnapsackInstance inst;
    inst.W = 0.0;
    inst.V = 0.0;

    loadItems         (basePath + "/items.csv",             inst);
    loadCategoryRules (basePath + "/category_rules.csv",    inst);
    loadIncompat      (basePath + "/incompatibilities.csv", inst);
    loadDependencies  (basePath + "/dependencies.csv",      inst);
    computeStats(inst);

    inst.W = pct_w * inst.total_peso;
    inst.V = pct_v * inst.total_volumen;

    return inst;
}
