#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <numeric>
#include <deque>
#include <cmath>

/**
 * Exponential Moving Average (EMA) calculated incrementally
 */
class EMA : public IncrementalIndicator<double> {
private:
    int period;
    double multiplier;
    double current_ema = 0.0;
    std::deque<double> close_history;  // Buffer pour accumulation des valeurs
    
public:
    EMA(filter::EMAParams params) : IncrementalIndicator<double>("EMA_" + std::to_string(params.period), params.period), period(params.period) {
        multiplier = 2.0 / (period + 1.0);
    }

    std::optional<double> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<double> update(const BasicCandle& candle) override;
    std::optional<double> get_value() const override;
};

inline std::optional<double> EMA::initialize_with_history(const std::vector<BasicCandle>& history) {
    if (history.size() < static_cast<size_t>(period)) {
        return std::nullopt;
    }

    std::vector<double> close_prices;
    close_prices.reserve(history.size());
    
    for (const auto& candle : history) {
        close_prices.push_back(candle.close);
    }
    
    // Calculate initial SMA
    double sum = std::accumulate(
        close_prices.end() - period, 
        close_prices.end(), 
        0.0
    );
    current_ema = sum / period;
    
    // Apply EMA formula for remaining prices
    for (size_t i = close_prices.size() - period; i < close_prices.size(); ++i) {
        current_ema = (close_prices[i] - current_ema) * multiplier + current_ema;
    }
    
    is_initialized = true;
    return current_ema;
}

inline std::optional<double> EMA::update(const BasicCandle& candle) {
    // Accumuler les valeurs de clôture
    close_history.push_back(candle.close);
    
    // Si l'indicateur n'est pas encore initialisé
    if (!is_initialized) {
        // Vérifier si nous avons suffisamment de données
        if (close_history.size() < static_cast<size_t>(period))
            return std::nullopt;

        // Convertir le buffer en vecteur de BasicCandle pour l'initialisation
        std::vector<BasicCandle> history;
        history.reserve(close_history.size());
        
        for (double close_price : close_history) {
            BasicCandle c;
            c.close = close_price;
            history.push_back(c);
        }
        
        // Initialiser l'EMA avec les données accumulées
        std::optional<double> result = initialize_with_history(history);

        if (!result.has_value()) {
            return std::nullopt; // Échec de l'initialisation
        }
        
        // Limiter la taille du buffer pour éviter une consommation excessive de mémoire
        while (close_history.size() > static_cast<size_t>(period)) {
            close_history.pop_front();
        }
        
        return result;
    }
    
    // Limiter la taille du buffer
    if (close_history.size() > static_cast<size_t>(period)) {
        close_history.pop_front();
    }
    
    // Calculate new EMA value
    current_ema = (candle.close - current_ema) * multiplier + current_ema;
    return current_ema;
}

inline std::optional<double> EMA::get_value() const {
    return current_ema;
}