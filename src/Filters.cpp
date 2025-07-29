#include "Filters.h"


// Filtre pour prix > EMA
bool Filters::priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, true);
}

// Filtre pour prix < EMA
bool Filters::priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, false);
}

bool Filters::priceCompareEMA(double price, double ema_value, const std::string& name, ILogger* logger, bool checkSuperior) {
    if (ema_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur EMA non calculée (0.0)");
        return false;
    }
    
    bool result = checkSuperior ? (price > ema_value) : (price < ema_value);
    logger->log_filter_result(name, result);

    // Choisir le bon opérateur de comparaison en fonction du résultat
    std::string comparisonOp;
    if (checkSuperior)
        comparisonOp = result ? ">" : "<="; // Pour les tests supérieurs
    else
        comparisonOp = result ? "<" : ">="; // Pour les tests inférieurs
    
    logger->log_filter_comparison(name, price, ema_value, comparisonOp, result);
    return result;
}

// Filtre Stochastique sous un seuil (avec historique)
bool Filters::stochInfThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger) {
    return stochCompareThreshold(kd_values, threshold, name, logger, false);
}

bool Filters::stochAboveThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string &name, ILogger *logger)
{
    return stochCompareThreshold(kd_values, threshold, name, logger, true);
}

bool Filters::stochCompareThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger, bool checkSuperior) {
    if (kd_values.empty() || kd_values[0].first == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur K non calculée (0.0)");
        return false;
    }

    // Vérifier chaque valeur dans l'historique
    bool pass = false;
    
    for (size_t i = 0; i < kd_values.size(); ++i) {
        bool result_check = checkSuperior ? (kd_values[i].first > threshold) : (kd_values[i].first < threshold);
        if (kd_values[i].first > 0.0 && result_check) {
            pass = true;
            std::string period_name = (i == 0) ? "actuel" : 
                                      (i == 1) ? "précédent" : "précédent-" + std::to_string(i);
            logger->log_filter_comparison(name + " (K) " + period_name, kd_values[i].first, threshold, (checkSuperior ? ">" : "<"), true);
            break;
        }
    }
    
    logger->log_filter_result(name, pass);

    if (!pass) {
        std::string details = "Toutes les valeurs K sont" + std::string((checkSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_detail(name, details);
    }
    
    return pass;
}

// Filtre RSI sous un seuil (avec mise à jour de l'historique)
bool Filters::rsiInfThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger) {
    return rsiCompareThreshold(rsi_values, threshold, name, logger, false);
}

bool Filters::rsiAboveThreshold(const std::vector<double>& rsi_values, int threshold, const std::string &name, ILogger *logger) {
    return rsiCompareThreshold(rsi_values, threshold, name, logger, true);
}

bool Filters::rsiCompareThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger, bool checkSuperior) {
    if (rsi_values.empty() || rsi_values[0] == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail(name, "Valeur RSI non calculée (0.0)");
        return false;
    }

    // Vérifier chaque valeur dans l'historique
    bool pass = false;
    
    for (size_t i = 0; i < rsi_values.size(); ++i) {
        bool result_check = checkSuperior ? (rsi_values[i] > threshold) : (rsi_values[i] < threshold);
        if (rsi_values[i] > 0.0 && result_check) {
            pass = true;
            std::string period_name = (i == 0) ? "actuel" : 
                                      (i == 1) ? "précédent" : "antérieur-" + std::to_string(i - 1);

            logger->log_filter_comparison(name + " " + period_name, rsi_values[i], threshold, (checkSuperior ? ">" : "<"), true);
            break;
        }
    }

    logger->log_filter_result(name, pass);

    if (!pass) {
        std::string details = "Toutes les valeurs RSI sont" + std::string((checkSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_detail(name, details);
    }

    return pass;
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