#include "fitness.hpp"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// PenaltyParams::fromInstance
// ---------------------------------------------------------------------------

PenaltyParams PenaltyParams::fromInstance(const KnapsackInstance& inst) {
    PenaltyParams p;
    int n = (int)inst.items.size();
    // Restricciones DURAS: penalizacion alta para garantizar factibilidad
    // Cualquier violacion supera el maximo beneficio teorico posible
    double hard_penalty = 10.0 * inst.max_valor * (n > 0 ? n : 1);
    p.alpha     = hard_penalty;
    p.beta      = hard_penalty;
    p.delta     = hard_penalty;
    p.epsilon   = hard_penalty;
    // Restriccion BLANDA: penalizacion moderada (no descarta el individuo)
    p.gamma_cat = inst.avg_valor / 2.0;
    return p;
}

// ---------------------------------------------------------------------------
// totalValue
// ---------------------------------------------------------------------------

double totalValue(const std::vector<int>& chromo,
                  const KnapsackInstance& inst) {
    double val = 0.0;
    int n = (int)std::min(chromo.size(), inst.items.size());
    for (int i = 0; i < n; ++i)
        if (chromo[i]) val += inst.items[i].valor;
    return val;
}

// ---------------------------------------------------------------------------
// excesoPeso
// ---------------------------------------------------------------------------

double excesoPeso(const std::vector<int>& chromo,
                  const KnapsackInstance& inst) {
    double total = 0.0;
    int n = (int)std::min(chromo.size(), inst.items.size());
    for (int i = 0; i < n; ++i)
        if (chromo[i]) total += inst.items[i].peso;
    return std::max(0.0, total - inst.W);
}

// ---------------------------------------------------------------------------
// excesoVolumen
// ---------------------------------------------------------------------------

double excesoVolumen(const std::vector<int>& chromo,
                     const KnapsackInstance& inst) {
    double total = 0.0;
    int n = (int)std::min(chromo.size(), inst.items.size());
    for (int i = 0; i < n; ++i)
        if (chromo[i]) total += inst.items[i].volumen;
    return std::max(0.0, total - inst.V);
}

// ---------------------------------------------------------------------------
// countIncompatibilidades
// ---------------------------------------------------------------------------

int countIncompatibilidades(const std::vector<int>& chromo,
                             const KnapsackInstance& inst) {
    int count = 0;
    for (const auto& [a, b] : inst.incompatibilities) {
        auto it_a = inst.id_to_index.find(a);
        auto it_b = inst.id_to_index.find(b);
        if (it_a == inst.id_to_index.end() ||
            it_b == inst.id_to_index.end()) continue;

        int idx_a = it_a->second;
        int idx_b = it_b->second;
        if (idx_a < (int)chromo.size() && idx_b < (int)chromo.size())
            if (chromo[idx_a] && chromo[idx_b]) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// countDependencias
// ---------------------------------------------------------------------------

int countDependencias(const std::vector<int>& chromo,
                      const KnapsackInstance& inst) {
    int count = 0;
    for (const auto& [item_id, required_id] : inst.dependencies) {
        auto it_item = inst.id_to_index.find(item_id);
        auto it_req  = inst.id_to_index.find(required_id);
        if (it_item == inst.id_to_index.end() ||
            it_req  == inst.id_to_index.end()) continue;

        int idx_item = it_item->second;
        int idx_req  = it_req->second;
        if (idx_item < (int)chromo.size() && idx_req < (int)chromo.size())
            // Si el item esta seleccionado pero su requerido NO -> violacion
            if (chromo[idx_item] && !chromo[idx_req]) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// violacionesCategoria  [BLANDA]
// ---------------------------------------------------------------------------

int violacionesCategoria(const std::vector<int>& chromo,
                         const KnapsackInstance& inst) {
    if (inst.cat_rules.empty()) return 0;

    // Contar items seleccionados por categoria
    std::map<std::string, int> cat_count;
    int n = (int)std::min(chromo.size(), inst.items.size());
    for (int i = 0; i < n; ++i)
        if (chromo[i]) cat_count[inst.items[i].categoria]++;

    int violations = 0;
    for (const auto& rule : inst.cat_rules) {
        int count = 0;
        auto it = cat_count.find(rule.categoria);
        if (it != cat_count.end()) count = it->second;

        if (rule.minimo >= 0 && count < rule.minimo)
            violations += (rule.minimo - count);
        if (rule.maximo >= 0 && count > rule.maximo)
            violations += (count - rule.maximo);
    }
    return violations;
}

// ---------------------------------------------------------------------------
// computeFitness
// ---------------------------------------------------------------------------

double computeFitness(const std::vector<int>& chromo,
                      const KnapsackInstance& inst,
                      const PenaltyParams&    params) {
    double valor = totalValue(chromo, inst);

    double pen_peso   = params.alpha     * excesoPeso(chromo, inst);
    double pen_vol    = params.beta      * excesoVolumen(chromo, inst);
    double pen_cat    = params.gamma_cat * violacionesCategoria(chromo, inst);
    double pen_incomp = params.delta     * countIncompatibilidades(chromo, inst);
    double pen_dep    = params.epsilon   * countDependencias(chromo, inst);

    return valor - pen_peso - pen_vol - pen_cat - pen_incomp - pen_dep;
}

// ---------------------------------------------------------------------------
// isFeasible  (solo restricciones DURAS)
// ---------------------------------------------------------------------------

bool isFeasible(const std::vector<int>& chromo,
                const KnapsackInstance& inst) {
    return excesoPeso(chromo, inst)          == 0.0 &&
           excesoVolumen(chromo, inst)       == 0.0 &&
           countIncompatibilidades(chromo, inst) == 0 &&
           countDependencias(chromo, inst)   == 0;
}
