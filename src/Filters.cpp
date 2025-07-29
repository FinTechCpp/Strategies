#include "Filters.h"


// Filtre pour prix > EMA
bool Filters::priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, true);
}

// Filtre pour prix < EMA
bool Filters::priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, false);
}

bool Filters::priceCompareEMA(double price, double ema_value, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (ema_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Valeur EMA non calculée (0.0)");
        return false;
    }
    
    bool pass = passIfSuperior ? (price > ema_value) : (price < ema_value);
    logger->log_filter_result(name, pass);

    // Choisir le bon opérateur de comparaison en fonction du résultat
    std::string comparisonOp;
    if (passIfSuperior)
        comparisonOp = pass ? " >" : " <=";
    else
        comparisonOp = pass ? " <" : " >=";

    logger->log_filter_detail("Prix: " + logger->fast_double_to_string(price) + comparisonOp + " EMA: " + logger->fast_double_to_string(ema_value));

    return pass;
}

// Filtre Stochastique sous un seuil (avec historique)
bool Filters::stochInfThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger) {
    return stochCompareThreshold(kd_values, threshold, name, logger, false);
}

bool Filters::stochAboveThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string &name, ILogger *logger)
{
    return stochCompareThreshold(kd_values, threshold, name, logger, true);
}

bool Filters::stochCompareThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (kd_values.empty() || kd_values[0].first == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Valeur K non calculée (0.0)");
        return false;
    }

    // Vérifier chaque valeur dans l'historique
    bool pass = false;
    
    for (size_t i = 0; i < kd_values.size(); ++i) {
        bool result_check = passIfSuperior ? (kd_values[i].first > threshold) : (kd_values[i].first < threshold);
        if (kd_values[i].first > 0.0 && result_check) {
            pass = true;
            logger->log_filter_result(name, true);
            std::string period_name = (i == 0) ? "actuel" : 
                                      (i == 1) ? "précédent" : "précédent-" + std::to_string(i);
            std::string comparisonOp = passIfSuperior ? " >" : " <=";
            logger->log_filter_detail(name + " (K) " + period_name + ": " + logger->fast_double_to_string(kd_values[i].first) + comparisonOp + " Seuil: " + std::to_string(threshold));
            break;
        }
    }
    

    if (!pass) {
        logger->log_filter_result(name, false);
        std::string details = "Toutes les valeurs K sont" + std::string((passIfSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_detail(details);
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

bool Filters::rsiCompareThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (rsi_values.empty() || rsi_values[0] == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Valeur RSI non calculée (0.0)");
        return false;
    }

    // Vérifier chaque valeur dans l'historique
    bool pass = false;
    
    for (size_t i = 0; i < rsi_values.size(); ++i) {
        bool result_check = passIfSuperior ? (rsi_values[i] > threshold) : (rsi_values[i] < threshold);
        if (rsi_values[i] > 0.0 && result_check) {
            pass = true;
            logger->log_filter_result(name, true);
            std::string period_name = (i == 0) ? "actuel" : 
                                      (i == 1) ? "précédent" : "antérieur-" + std::to_string(i - 1);
            std::string comparisonOp = passIfSuperior ? " >" : " <=";
            std::string detail = name + " " + period_name + ": " + logger->fast_double_to_string(rsi_values[i]) + comparisonOp + " Seuil: " + std::to_string(threshold);
            logger->log_filter_detail(detail);
            break;
        }
    }


    if (!pass) {
        logger->log_filter_result(name, false);
        std::string details = "Toutes les valeurs RSI sont" + std::string((passIfSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_detail(details);
    }

    return pass;
}

// Filtre pour vérifier si n bougies Heikin-Ashi précédentes sont rouges
bool Filters::previousHACandlesRed(const CandleManager& candleManager, int n_previous_candles, const std::string& name, ILogger* logger) {
    // Vérifier s'il y a assez de bougies disponibles
    if (candleManager.size() < n_previous_candles + 2) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Pas assez d'historique (min " + std::to_string(n_previous_candles + 2) + " bougies)");
        return false;
    }
    
    // Récupérer les bougies HA
    auto ha_candles = candleManager.get_last_heikin_ashi_candles(n_previous_candles + 1);
    if (ha_candles.size() < n_previous_candles + 1) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Pas assez de bougies HA");
        return false;
    }

    // Vérifier si toutes les bougies dans la fenêtre sont rouges
    for (size_t i = 0; i < n_previous_candles; ++i) {
        const BasicCandle& ha_candle = ha_candles[i];
        if (ha_candle.close >= ha_candle.open) {
            logger->log_filter_result(name, false);
            logger->log_filter_detail("Bougie HA " + std::to_string(i + 1) + " VERTE (open=" + 
                logger->fast_double_to_string(ha_candle.open) + 
                ", close=" + logger->fast_double_to_string(ha_candle.close) + ")");
            return false;
        }
    }
    
    logger->log_filter_result(name, true);
    logger->log_filter_detail("Toutes les bougies HA précédentes (" + std::to_string(n_previous_candles) + ") sont ROUGES");
    return true;
}

bool Filters::previousHACandlesGreen(const CandleManager &candleManager, int n_previous_candles, const std::string &name, ILogger *logger)
{
    // Vérifier s'il y a assez de bougies disponibles
    if (candleManager.size() < n_previous_candles + 2) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Pas assez d'historique (min " + std::to_string(n_previous_candles + 2) + " bougies)");
        return false;
    }
    
    // Récupérer les bougies HA
    auto ha_candles = candleManager.get_last_heikin_ashi_candles(n_previous_candles + 1);
    if (ha_candles.size() < n_previous_candles + 1) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Pas assez de bougies HA");
        return false;
    }

    // Vérifier si toutes les bougies dans la fenêtre sont vertes
    for (size_t i = 0; i < n_previous_candles; ++i) {
        const BasicCandle& ha_candle = ha_candles[i];
        if (ha_candle.close <= ha_candle.open) {
            logger->log_filter_result(name, false);
            logger->log_filter_detail(
                "Bougie HA " + std::to_string(i + 1) + " VERTE (open=" + 
                logger->fast_double_to_string(ha_candle.open) + 
                ", close=" + logger->fast_double_to_string(ha_candle.close) + ")");
            return false;
        }
    }
    
    logger->log_filter_result(name, true);
    logger->log_filter_detail("Toutes les bougies HA précédentes (" + std::to_string(n_previous_candles) + ") sont VERTES");
    return true;
}

// Filtre SuperTrend pour vérifier si le prix est au-dessus de la bande SuperTrend
bool Filters::priceSupSupertrend(double price, double supertrend_value, int supertrend_direction, const std::string& name, ILogger* logger) {
    return priceCompareSupertrend(price, supertrend_value, supertrend_direction, name, logger, true);
}

// Filtre SuperTrend pour vérifier si le prix est en dessous de la bande SuperTrend
bool Filters::priceInfSupertrend(double price, double supertrend_value, int supertrend_direction, const std::string& name, ILogger* logger) {
    return priceCompareSupertrend(price, supertrend_value, supertrend_direction, name, logger, false);
}

bool Filters::priceCompareSupertrend(double price, double supertrend_value, int supertrend_direction, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (supertrend_value == 0.0) {
        logger->log_filter_result(name, false);
        logger->log_filter_detail("Valeur Supertrend non calculée (0.0)");
        return false;
    }
    
    // Filter is true if price is below Supertrend band (downtrend)
    bool pass = passIfSuperior ? (price > supertrend_value) : (price < supertrend_value);

    logger->log_filter_result(name, pass);

    // Choisir le bon opérateur de comparaison en fonction du résultat
    std::string comparisonOp;
    if (passIfSuperior)
        comparisonOp = pass ? " >" : " <=";
    else
        comparisonOp = pass ? " <" : " >=";

    // Additional logging for trend direction
    std::string trend_direction = (supertrend_direction == 1)  ? "UPTREND" : 
                                  (supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
    logger->log_filter_detail("Direction: " + trend_direction + ", Prix: " + logger->fast_double_to_string(price) + comparisonOp + name + ": " + logger->fast_double_to_string(supertrend_value));

    return pass;
}
