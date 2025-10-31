#pragma once

#include "Indicators/IncrementalIndicator.hpp"
#include "common.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Encodage cyclique du temps (heure du jour) en sin/cos
 * 
 * Transforme l'heure du jour en coordonnées polaires pour capturer
 * la nature cyclique du temps (0h = 24h).
 * 
 * Formule:
 *   - sin_value = sin(2π * hour / 24)
 *   - cos_value = cos(2π * hour / 24)
 * 
 * Exemple d'utilisation en ML:
 *   - Permet au modèle de comprendre que 23h et 1h sont proches
 *   - Capture les patterns horaires (volatilité, volume, etc.)
 */
class TIMECYCLIC : public IncrementalIndicator<std::pair<double, double>> {
private:
    filter::TimeCyclicParams m_params;
    std::pair<double, double> m_currentValue; // {sin, cos}

    /**
     * @brief Extrait l'heure en décimal depuis un DateTime
     * @param dt DateTime de la bougie
     * @return Heure du jour en décimal (0.0 - 23.999...)
     */
    double getHourOfDay(const DateTime& dt) const {
        double hour = dt.time.hour;
        double minute = dt.time.minute;
        double second = dt.time.second;
        
        return hour + (minute / 60.0) + (second / 3600.0);
    }

    /**
     * @brief Calcule les valeurs sin/cos pour une heure donnée
     * @param hour Heure du jour (0-24)
     * @return {sin_value, cos_value}
     */
    std::pair<double, double> calculateCyclic(double hour) const {
        double angle = 2.0 * M_PI * hour / 24.0;
        return {std::sin(angle), std::cos(angle)};
    }

public:
    explicit TIMECYCLIC(const filter::TimeCyclicParams& params = filter::TimeCyclicParams())
        : IncrementalIndicator<std::pair<double, double>>("TIMECYCLIC", 1)
        , m_params(params)
        , m_currentValue({0.0, 0.0})
    {}

    // Implémentation des méthodes virtuelles pures de IncrementalIndicator
    std::optional<std::pair<double, double>> initialize_with_history(const std::vector<BasicCandle>& history) override {
        if (history.empty()) {
            return std::nullopt;
        }

        // Initialiser avec la dernière bougie
        const auto& lastCandle = history.back();
        double hour = getHourOfDay(lastCandle.date);
        m_currentValue = calculateCyclic(hour);
        is_initialized = true;

        return m_currentValue;
    }

    std::optional<std::pair<double, double>> update(const BasicCandle& candle) override {
        double hour = getHourOfDay(candle.date);
        m_currentValue = calculateCyclic(hour);
        is_initialized = true;
        return m_currentValue;
    }

    std::optional<std::pair<double, double>> get_value() const override {
        if (!is_initialized) {
            return std::nullopt;
        }
        return m_currentValue;
    }

    // Méthodes d'accès individuelles pour faciliter l'utilisation
    double getSinValue() const {
        return m_currentValue.first;
    }

    double getCosValue() const {
        return m_currentValue.second;
    }
};
