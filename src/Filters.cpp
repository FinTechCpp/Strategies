#include "Filters.h"


// Filtre pour prix > EMA
bool Filters::priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    if (ema_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur EMA non calculée (0.0)");
        return false;
    }
    
    bool result = price > ema_value;
    logger->log_filter_result(name, result);
    logger->log_filter_comparison(name, price, ema_value, 
                                result ? ">" : "<=", result);
    return result;
}

// Filtre pour prix < EMA
bool Filters::priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    if (ema_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur EMA non calculée (0.0)");
        return false;
    }
    
    bool result = price < ema_value;
    logger->log_filter_result(name, result);
    logger->log_filter_comparison(name, price, ema_value, 
                                result ? "<" : ">=", result);
    return result;
}

// Filtre Stochastique sous un seuil (avec historique)
bool Filters::stochInfThreshold(double current_k, double k_previous, 
                            double k_previous_2, double k_previous_3, 
                            int threshold, const std::string& name, 
                            ILogger* logger) {
    if (current_k == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur K non calculée (0.0)");
        return false;
    }
    
    bool current_below = current_k < threshold;
    bool previous_below = k_previous > 0.0 && k_previous < threshold;
    bool before_previous_below = k_previous_2 > 0.0 && k_previous_2 < threshold;
    bool before_before_previous_below = k_previous_3 > 0.0 && k_previous_3 < threshold;
    bool result = current_below || previous_below || before_previous_below || before_before_previous_below;
    
    logger->log_filter_result(name, result);

    if (current_below) {
        logger->log_filter_comparison(name + " K", current_k, threshold, "<", true);
    } 
    else if (previous_below) {
        logger->log_filter_comparison(name + " K précédent", k_previous, threshold, "<", true);
    } 
    else if (before_previous_below) {
        logger->log_filter_comparison(name + " K précédent-2", k_previous_2, threshold, "<", true);
    } 
    else if (before_before_previous_below) {
        logger->log_filter_comparison(name + " K précédent-3", k_previous_3, threshold, "<", true);
    } 
    else {
        logger->log_filter_detail(name, 
            "K actuel: " + logger->fast_double_to_string(current_k) + 
            ", K-1: " + logger->fast_double_to_string(k_previous) + 
            ", K-2: " + logger->fast_double_to_string(k_previous_2) + 
            ", K-3: " + logger->fast_double_to_string(k_previous_3) + 
            " - Tous au-dessus du seuil " + std::to_string(threshold));
    }
    
    return result;
}

bool Filters::stochAboveThreshold(double current_k, double k_previous, double k_previous_2, double k_previous_3, int threshold, const std::string &name, ILogger *logger)
{
    if (current_k == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur K non calculée (0.0)");
        return false;
    }

    bool current_above = current_k > threshold;
    bool previous_above = k_previous > 0.0 && k_previous > threshold;
    bool before_previous_above = k_previous_2 > 0.0 && k_previous_2 > threshold;
    bool before_before_previous_above = k_previous_3 > 0.0 && k_previous_3 > threshold;
    bool result = current_above || previous_above || before_previous_above || before_before_previous_above;

    logger->log_filter_result(name, result);

    if (current_above) {
        logger->log_filter_comparison(name + " K", current_k, threshold, ">", true);
    } 
    else if (previous_above) {
        logger->log_filter_comparison(name + " K précédent", k_previous, threshold, ">", true);
    } 
    else if (before_previous_above) {
        logger->log_filter_comparison(name + " K précédent-2", k_previous_2, threshold, ">", true);
    } 
    else if (before_before_previous_above) {
        logger->log_filter_comparison(name + " K précédent-3", k_previous_3, threshold, ">", true);
    } 
    else {
        logger->log_filter_detail(name, 
            "K actuel: " + logger->fast_double_to_string(current_k) + 
            ", K-1: " + logger->fast_double_to_string(k_previous) + 
            ", K-2: " + logger->fast_double_to_string(k_previous_2) + 
            ", K-3: " + logger->fast_double_to_string(k_previous_3) + 
            " - Tous au-dessus du seuil " + logger->fast_double_to_string(threshold));
    }

    return result;
}

// Filtre RSI sous un seuil (avec mise à jour de l'historique)
bool Filters::rsiInfThreshold(double current_rsi, double& previous_rsi, 
                            double& previous_2_rsi, int threshold, 
                            const std::string& name, ILogger* logger) {
    if (current_rsi == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur RSI non calculée (0.0)");
        return false;
    }

    // Vérifier les trois dernières valeurs
    bool current_below = current_rsi < threshold;
    bool prev_below = previous_rsi > 0.0 && previous_rsi < threshold;
    bool prev2_below = previous_2_rsi > 0.0 && previous_2_rsi < threshold;
    
    bool result = current_below || prev_below || prev2_below;
    
    // Journalisation détaillée
    logger->log_filter_result(name, result);
    
    if (current_below) {
        logger->log_filter_comparison(name + " actuel", current_rsi, threshold, "<", true);
    } 
    else if (prev_below) {
        logger->log_filter_comparison(name + " précédent", previous_rsi, threshold, "<", true);
    } 
    else if (prev2_below) {
        logger->log_filter_comparison(name + " antérieur", previous_2_rsi, threshold, "<", true);
    } 
    else {
        logger->log_filter_detail(name, 
            "Actuel: " + logger->fast_double_to_string(current_rsi) + 
            ", Précédent: " + logger->fast_double_to_string(previous_rsi) + 
            ", Antérieur: " + logger->fast_double_to_string(previous_2_rsi) + 
            " - Tous au-dessus du seuil " + logger->fast_int_to_string(threshold));
    }
    
    // Mettre à jour les valeurs historiques
    previous_2_rsi = previous_rsi;
    previous_rsi = current_rsi;
    
    return result;
}

bool Filters::rsiAboveThreshold(double current_rsi, double &previous_rsi, double &previous_2_rsi, int threshold, const std::string &name, ILogger *logger)
{
    if (current_rsi == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur RSI non calculée (0.0)");
        return false;
    }

    // Vérifier les trois dernières valeurs
    bool current_above = current_rsi > threshold;
    bool prev_above = previous_rsi > 0.0 && previous_rsi > threshold;
    bool prev2_above = previous_2_rsi > 0.0 && previous_2_rsi > threshold;

    bool result = current_above || prev_above || prev2_above;

    // Journalisation détaillée
    logger->log_filter_result(name, result);

    if (current_above) {
        logger->log_filter_comparison(name + " actuel", current_rsi, threshold, ">", true);
    } 
    else if (prev_above) {
        logger->log_filter_comparison(name + " précédent", previous_rsi, threshold, ">", true);
    } 
    else if (prev2_above) {
        logger->log_filter_comparison(name + " antérieur", previous_2_rsi, threshold, ">", true);
    } 
    else {
        logger->log_filter_detail(name, 
            "Actuel: " + logger->fast_double_to_string(current_rsi) + 
            ", Précédent: " + logger->fast_double_to_string(previous_rsi) + 
            ", Antérieur: " + logger->fast_double_to_string(previous_2_rsi) + 
            " - Tous en dessous du seuil " + logger->fast_int_to_string(threshold));
    }
    
    // Mettre à jour les valeurs historiques
    previous_2_rsi = previous_rsi;
    previous_rsi = current_rsi;
    
    return result;
}

// Filtre pour vérifier si n bougies Heikin-Ashi précédentes sont rouges
bool Filters::previousHACandlesRed(const CandleManager& candleManager, 
                                int n_previous_candles, 
                                const std::string& name, 
                                ILogger* logger) {
    // Vérifier s'il y a assez de bougies disponibles
    if (candleManager.size() < n_previous_candles + 2) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Pas assez d'historique (min " + 
                                std::to_string(n_previous_candles + 2) + " bougies)");
        return false;
    }
    
    // Récupérer les bougies HA
    auto ha_candles = candleManager.get_last_heikin_ashi_candles(n_previous_candles + 1);
    if (ha_candles.size() < n_previous_candles + 1) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Pas assez de bougies HA");
        return false;
    }

    // Vérifier si toutes les bougies dans la fenêtre sont rouges
    for (size_t i = 0; i < n_previous_candles; ++i) {
        const BasicCandle& ha_candle = ha_candles[i];
        if (ha_candle.close >= ha_candle.open) {
            logger->log_filter_result(name, false);
            logger->log_filter_detail(name, 
                "Bougie HA " + std::to_string(i + 1) + " VERTE (open=" + 
                logger->fast_double_to_string(ha_candle.open) + 
                ", close=" + logger->fast_double_to_string(ha_candle.close) + ")");
            return false;
        }
    }
    
    logger->log_filter_result(name, true);
    logger->log_filter_detail(name, "Toutes les " + std::to_string(n_previous_candles) + 
                            " bougies HA précédentes sont ROUGES");
    return true;
}

bool Filters::previousHACandlesGreen(const CandleManager &candleManager, int n_previous_candles, const std::string &name, ILogger *logger)
{
    // Vérifier s'il y a assez de bougies disponibles
    if (candleManager.size() < n_previous_candles + 2) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Pas assez d'historique (min " + 
                                std::to_string(n_previous_candles + 2) + " bougies)");
        return false;
    }
    
    // Récupérer les bougies HA
    auto ha_candles = candleManager.get_last_heikin_ashi_candles(n_previous_candles + 1);
    if (ha_candles.size() < n_previous_candles + 1) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Pas assez de bougies HA");
        return false;
    }

    // Vérifier si toutes les bougies dans la fenêtre sont vertes
    for (size_t i = 0; i < n_previous_candles; ++i) {
        const BasicCandle& ha_candle = ha_candles[i];
        if (ha_candle.close <= ha_candle.open) {
            logger->log_filter_result(name, false);
            logger->log_filter_detail(name, 
                "Bougie HA " + std::to_string(i + 1) + " VERTE (open=" + 
                logger->fast_double_to_string(ha_candle.open) + 
                ", close=" + logger->fast_double_to_string(ha_candle.close) + ")");
            return false;
        }
    }
    
    logger->log_filter_result(name, true);
    logger->log_filter_detail(name, "Toutes les " + std::to_string(n_previous_candles) + 
                            " bougies HA précédentes sont VERTES");
    return true;
}

// Filtre SuperTrend pour vérifier si le prix est au-dessus de la bande SuperTrend
bool Filters::priceSupSupertrend(double price, double supertrend_value, 
                            int supertrend_direction, const std::string& name, 
                            ILogger* logger) {
    if (supertrend_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur Supertrend non calculée (0.0)");
        return false;
    }
    
    // Filter is true if price is above Supertrend band (uptrend)
    bool result = price > supertrend_value;
    
    logger->log_filter_result(name, result);
    logger->log_filter_comparison(name, price, supertrend_value, 
                                result ? ">" : "<=", result);
    
    // Additional logging for trend direction
    std::string trend_direction = (supertrend_direction == 1) ? "UPTREND" : 
                                    (supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
    logger->log_filter_detail(name, "Direction: " + trend_direction + 
                            ", Supertrend: " + logger->fast_double_to_string(supertrend_value));

    return result;
}

// Filtre SuperTrend pour vérifier si le prix est en dessous de la bande SuperTrend
bool Filters::priceInfSupertrend(double price, double supertrend_value, 
                            int supertrend_direction, const std::string& name, 
                            ILogger* logger) {
    if (supertrend_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur Supertrend non calculée (0.0)");
        return false;
    }
    
    // Filter is true if price is below Supertrend band (downtrend)
    bool result = price < supertrend_value;
    
    logger->log_filter_result(name, result);
    logger->log_filter_comparison(name, price, supertrend_value, 
                                result ? "<" : ">=", result);
    
    // Additional logging for trend direction
    std::string trend_direction = (supertrend_direction == 1) ? "UPTREND" : 
                                    (supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
    logger->log_filter_detail(name, "Direction: " + trend_direction + 
                            ", Supertrend: " + logger->fast_double_to_string(supertrend_value));

    return result;
}