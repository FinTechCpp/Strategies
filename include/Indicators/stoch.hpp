#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <utility>

/**
 * Stochastic Oscillator calculated incrementally
 * Optimized version with true incremental updates
 */
class STOCH : public IncrementalIndicator<std::pair<double, double>> {
private:
    const int fastk_period;
    const int slowk_period;
    const int slowd_period;
    
    // Buffer de bougies pour le calcul du %K (seulement fastk_period nécessaire)
    std::deque<BasicCandle> candle_buffer;
    
    // Buffer des valeurs brutes %K pour le lissage
    std::deque<double> raw_k_values;
    
    // Buffer des valeurs %K lissées pour le calcul de %D
    std::deque<double> k_values;
    
    // Sommes cumulatives pour calculs incrémentaux O(1)
    double sum_raw_k = 0.0;  // Somme des raw_k_period dernières valeurs
    double sum_k = 0.0;      // Somme des slowd_period dernières valeurs de %K
    
    double current_k = 0.0;
    double current_d = 0.0;
    
    // Méthode helper pour calculer le raw %K à partir d'un buffer de bougies
    double calculate_raw_k(const std::deque<BasicCandle>& buffer) const;
    
public:
    STOCH(filter::StochasticParams params)
        : IncrementalIndicator<std::pair<double, double>>(
              "STOCH_" + std::to_string(params.fastK) + "_" + 
              std::to_string(params.slowK) + "_" + std::to_string(params.slowD), 
              params.fastK + params.slowK + params.slowD - 2),
          fastk_period(params.fastK),
          slowk_period(params.slowK),
          slowd_period(params.slowD) {}

    std::optional<std::pair<double, double>> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<std::pair<double, double>> update(const BasicCandle& candle) override;
    std::optional<std::pair<double, double>> get_value() const override;
};

// Méthode helper pour calculer le raw %K
inline double STOCH::calculate_raw_k(const std::deque<BasicCandle>& buffer) const {
    if (buffer.size() < static_cast<size_t>(fastk_period)) {
        return 0.0;
    }
    
    // Trouver le plus haut et le plus bas sur la période
    double period_high = buffer[0].high;
    double period_low = buffer[0].low;
    
    for (size_t i = 1; i < fastk_period; ++i) {
        period_high = std::max(period_high, buffer[i].high);
        period_low = std::min(period_low, buffer[i].low);
    }
    
    double close = buffer[fastk_period - 1].close;
    
    // Calculer %K brut
    if (period_high > period_low) {
        return 100.0 * ((close - period_low) / (period_high - period_low));
    }
    return 0.0;
}

inline std::optional<std::pair<double, double>> STOCH::initialize_with_history(const std::vector<BasicCandle>& history) {
    const size_t min_required = fastk_period + slowk_period + slowd_period - 2;
    
    if (history.size() < min_required) {
        return std::nullopt;
    }

    // Réinitialiser tous les buffers
    candle_buffer.clear();
    raw_k_values.clear();
    k_values.clear();
    sum_raw_k = 0.0;
    sum_k = 0.0;

    // Phase 1: Calculer les raw %K pour avoir assez de données pour lisser
    const size_t num_raw_k_needed = slowk_period + slowd_period - 1;
    
    for (size_t i = 0; i < num_raw_k_needed; ++i) {
        // Remplir le buffer avec fastk_period bougies
        candle_buffer.clear();
        for (int j = 0; j < fastk_period; ++j) {
            candle_buffer.push_back(history[i + j]);
        }
        
        double raw_k = calculate_raw_k(candle_buffer);
        raw_k_values.push_back(raw_k);
    }

    // Phase 2: Calculer les premiers %K lissés (SMA des raw_k sur slowk_period)
    for (size_t i = 0; i <= num_raw_k_needed - slowk_period; ++i) {
        double sum = 0.0;
        for (int j = 0; j < slowk_period; ++j) {
            sum += raw_k_values[i + j];
        }
        double smooth_k = sum / slowk_period;
        k_values.push_back(smooth_k);
    }

    // Phase 3: Calculer le premier %D (SMA des %K sur slowd_period)
    if (k_values.size() >= static_cast<size_t>(slowd_period)) {
        sum_k = 0.0;
        for (int i = 0; i < slowd_period; ++i) {
            sum_k += k_values[k_values.size() - slowd_period + i];
        }
        current_d = sum_k / slowd_period;
        current_k = k_values.back();
    }

    // Préparer le buffer de bougies avec les dernières fastk_period bougies
    candle_buffer.clear();
    for (int i = 0; i < fastk_period; ++i) {
        candle_buffer.push_back(history[history.size() - fastk_period + i]);
    }

    // Préparer sum_raw_k avec les dernières slowk_period valeurs
    sum_raw_k = 0.0;
    for (int i = 0; i < slowk_period; ++i) {
        sum_raw_k += raw_k_values[raw_k_values.size() - slowk_period + i];
    }

    // Garder seulement les buffers nécessaires pour les calculs futurs
    while (raw_k_values.size() > static_cast<size_t>(slowk_period)) {
        raw_k_values.pop_front();
    }
    
    while (k_values.size() > static_cast<size_t>(slowd_period)) {
        k_values.pop_front();
    }

    is_initialized = true;
    return std::make_pair(current_k, current_d);
}

inline std::optional<std::pair<double, double>> STOCH::update(const BasicCandle& candle) {
    if (!is_initialized) {
        return std::nullopt;
    }

    // ===== Étape 1: Mise à jour incrémentale du buffer de bougies =====
    candle_buffer.push_back(candle);
    if (candle_buffer.size() > static_cast<size_t>(fastk_period)) {
        candle_buffer.pop_front();
    }

    // ===== Étape 2: Calcul du nouveau raw %K =====
    double new_raw_k = calculate_raw_k(candle_buffer);

    // ===== Étape 3: Mise à jour incrémentale de %K lissé (SMA) =====
    // Ajouter le nouveau raw_k
    raw_k_values.push_back(new_raw_k);
    sum_raw_k += new_raw_k;
    
    // Retirer l'ancien raw_k si le buffer est plein
    if (raw_k_values.size() > static_cast<size_t>(slowk_period)) {
        sum_raw_k -= raw_k_values.front();
        raw_k_values.pop_front();
    }
    
    // Calculer le nouveau %K lissé (moyenne mobile simple incrémentale O(1))
    double new_k = sum_raw_k / slowk_period;
    current_k = new_k;

    // ===== Étape 4: Mise à jour incrémentale de %D (SMA de %K) =====
    // Ajouter le nouveau %K
    k_values.push_back(new_k);
    sum_k += new_k;
    
    // Retirer l'ancien %K si le buffer est plein
    if (k_values.size() > static_cast<size_t>(slowd_period)) {
        sum_k -= k_values.front();
        k_values.pop_front();
    }
    
    // Calculer le nouveau %D (moyenne mobile simple incrémentale O(1))
    current_d = sum_k / slowd_period;

    return std::make_pair(current_k, current_d);
}

inline std::optional<std::pair<double, double>> STOCH::get_value() const {
    if (!is_initialized) {
        return std::nullopt;
    }
    return std::make_pair(current_k, current_d);
}