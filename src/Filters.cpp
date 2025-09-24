#include "Filters.h"


// Filter for price > EMA
bool Filters::priceSupEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, true);
}

// Filter for price < EMA
bool Filters::priceInfEMA(double price, double ema_value, const std::string& name, ILogger* logger) {
    return priceCompareEMA(price, ema_value, name, logger, false);
}

bool Filters::priceCompareEMA(double price, double ema_value, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (ema_value == 0.0) {
        logger->log_filter_result(name, false, "Valeur EMA non calculée (0.0)");
        return false;
    }
    
    bool pass = passIfSuperior ? (price > ema_value) : (price < ema_value);
    std::string comparisonOp;
    if (passIfSuperior)
        comparisonOp = pass ? " > " : " <= ";
    else
        comparisonOp = pass ? " < " : " >= ";

    logger->log_filter_result(name, pass, "Prix: " + logger->fast_double_to_string(price) + comparisonOp + name + " : " + logger->fast_double_to_string(ema_value));

    return pass;
}

// Filter for Stochastic below a threshold (with history)
bool Filters::stochInfThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger) {
    return stochCompareThreshold(kd_values, threshold, name, logger, false);
}

bool Filters::stochAboveThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string &name, ILogger *logger)
{
    return stochCompareThreshold(kd_values, threshold, name, logger, true);
}

bool Filters::stochCompareThreshold(const std::vector<std::pair<double, double>>& kd_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (kd_values.empty() || kd_values[0].first == 0.0) {
        logger->log_filter_result(name, false, "Valeur K non calculée (0.0)");
        return false;
    }

    // Check each value in history
    bool pass = false;
    
    for (size_t i = 0; i < kd_values.size(); ++i) {
        bool result_check = passIfSuperior ? (kd_values[i].first > threshold) : (kd_values[i].first < threshold);
        if (kd_values[i].first > 0.0 && result_check) {
            pass = true;
            std::string period_name = (i == 0) ? "actuel" : 
                                      (i == 1) ? "précédent" : "précédent-" + std::to_string(i);
            std::string comparisonOp = passIfSuperior ? " >" : " <=";
            logger->log_filter_result(name, true, name + " (K) " + period_name + ": " + logger->fast_double_to_string(kd_values[i].first) + comparisonOp + " Seuil: " + std::to_string(threshold));
            break;
        }
    }
    

    if (!pass) {
        std::string details = "Toutes les valeurs K sont" + std::string((passIfSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_result(name, false, details);
    }

    return pass;
}

// Filter for RSI below a threshold (with history)
bool Filters::rsiInfThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger) {
    return rsiCompareThreshold(rsi_values, threshold, name, logger, false);
}

bool Filters::rsiAboveThreshold(const std::vector<double>& rsi_values, int threshold, const std::string &name, ILogger *logger) {
    return rsiCompareThreshold(rsi_values, threshold, name, logger, true);
}

bool Filters::rsiCompareThreshold(const std::vector<double>& rsi_values, int threshold, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (rsi_values.empty() || rsi_values[0] == 0.0) {
        logger->log_filter_result(name, false, "Valeur RSI non calculée (0.0)");
        return false;
    }

    // Check each value in history
    bool pass = false;
    
    for (size_t i = 0; i < rsi_values.size(); ++i) {
        bool result_check = passIfSuperior ? (rsi_values[i] > threshold) : (rsi_values[i] < threshold);
        if (rsi_values[i] > 0.0 && result_check) {
            pass = true;
            std::string period_name = (i == 0) ? "actuel" : 
            (i == 1) ? "précédent" : "antérieur-" + std::to_string(i - 1);
            std::string comparisonOp = passIfSuperior ? " >" : " <=";
            std::string detail = name + " " + period_name + ": " + logger->fast_double_to_string(rsi_values[i]) + comparisonOp + " Seuil: " + std::to_string(threshold);
            logger->log_filter_result(name, true, detail);
            break;
        }
    }


    if (!pass) {
        std::string details = "Toutes les valeurs RSI sont" + std::string((passIfSuperior ? " en dessous " : " au-dessus ")) + "du seuil " + std::to_string(threshold);
        logger->log_filter_result(name, false, details);
    }

    return pass;
}

bool Filters::previousHACandlesRed(const CandleManager& candleManager, size_t n_previous_candles, size_t offset, const std::string& name, ILogger* logger) {
    return previousHACandles(candleManager, n_previous_candles, offset, name, logger, false);
}

bool Filters::previousHACandlesGreen(const CandleManager &candleManager, size_t n_previous_candles, size_t offset, const std::string &name, ILogger *logger) {
    return previousHACandles(candleManager, n_previous_candles, offset, name, logger, true);
}

bool Filters::previousHACandles(const CandleManager& candleManager, size_t n_previous_candles, size_t offset, const std::string& name, ILogger* logger, bool passIfGreen) {
/*
Role de n_previous_candle et offset:
- n_previous_candles : nombre de bougies HA à vérifier
- offset : décalage pour commencer la vérification

Si n_previous_candles = 3 et offset = 4 :
1110000
      |- dernière bougie (la plus récente)
   ||||- offset 4 : bougie à NE PAS vérifier
|||   - n_previous_candles 3 : nombre de bougies à vérifier
*/

    // Check if there are enough candles available
    if (candleManager.size() < n_previous_candles + offset + 1) {
        logger->log_filter_result(name, false, "Pas assez d'historique (min " + std::to_string(n_previous_candles + offset + 1) + " bougies)");
        return false;
    }
    
    // Get HA candles
    std::vector<BasicCandle> ha_candles = candleManager.get_last_heikin_ashi_candles(n_previous_candles + offset);
    if (ha_candles.size() < n_previous_candles + offset) {
        logger->log_filter_result(name, false, "Pas assez de bougies HA");
        return false;
    }

    // Check if all candles in the window have the expected color
    for (size_t i = 0; i < n_previous_candles; ++i) {
        const BasicCandle& ha_candle = ha_candles[i];

        // Determine the color of the candle
        bool isGreen = ha_candle.close > ha_candle.open;
        bool isEqual = ha_candle.close == ha_candle.open;

        // If looking for green and it's not green
        // Or if looking for red and it's not red
        if ((passIfGreen && !isGreen) || (!passIfGreen && (isGreen || isEqual))) {
            std::string expected_color = passIfGreen ? "VERTE" : "ROUGE";
            std::string actual_color = isGreen ? "VERTE" : (isEqual ? "DOJI" : "ROUGE");
            
            logger->log_filter_result(name, false, "Bougie HA " + std::to_string(i + 1) + " " + actual_color + " (open=" + logger->fast_double_to_string(ha_candle.open) + ", close=" + logger->fast_double_to_string(ha_candle.close) + ")");
            return false;
        }
    }

    std::string color_name = passIfGreen ? "VERTES" : "ROUGES";
    logger->log_filter_result(name, true, "Toutes les bougies HA précédentes (" + std::to_string(n_previous_candles) + ") sont " + color_name);
    return true;
}

// SuperTrend filter to check if the price is above the SuperTrend band
bool Filters::priceSupSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger) {
    return priceCompareSupertrend(price, supertrend_values, name, logger, true);
}

// SuperTrend filter to check if the price is below the SuperTrend band
bool Filters::priceInfSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger) {
    return priceCompareSupertrend(price, supertrend_values, name, logger, false);
}

bool Filters::priceCompareSupertrend(double price, std::pair<double, int> supertrend_values, const std::string& name, ILogger* logger, bool passIfSuperior) {
    double supertrend_value = supertrend_values.first;
    int supertrend_direction = supertrend_values.second;

    if (supertrend_value == 0.0) {
        logger->log_filter_result(name, false, "Valeur Supertrend non calculée (0.0)");
        return false;
    }

    // Filter is true if price is below Supertrend band (downtrend)
    bool pass = passIfSuperior ? (price > supertrend_value) : (price < supertrend_value);


    // Choose the right comparison operator based on the result
    std::string comparisonOp;
    if (passIfSuperior)
    comparisonOp = pass ? " > " : " <= ";
    else
    comparisonOp = pass ? " < " : " >= ";
    
    // Additional logging for trend direction
    std::string trend_direction = (supertrend_direction == 1)  ? "UPTREND" : 
    (supertrend_direction == -1) ? "DOWNTREND" : "NEUTRAL";
    logger->log_filter_result(name, pass, "Direction: " + trend_direction + ", Prix: " + logger->fast_double_to_string(price) + comparisonOp + name + ": " + logger->fast_double_to_string(supertrend_value));

    return pass;
}

// ATR filter to check if the ATR is above a threshold (with history)
bool Filters::atrAboveThreshold(const std::vector<double>& atr_values, double threshold, const std::string& name, ILogger* logger) {
    return atrCompareThreshold(atr_values, threshold, name, logger, true);
}

bool Filters::atrCompareThreshold(const std::vector<double>& atr_values, double threshold, const std::string& name, ILogger* logger, bool passIfSuperior) {
    if (atr_values.empty() || atr_values[0] == 0.0) {
        logger->log_filter_result(name, false, "Valeur ATR non calculée (0.0)");
        return false;
    }

    // Check that all values in the history meet the condition
    int valid_count = 0;
    int required_count = static_cast<int>(atr_values.size());
    
    for (size_t i = 0; i < atr_values.size(); ++i) {
        if (atr_values[i] > 0.0) {
            bool result_check = passIfSuperior ? (atr_values[i] > threshold) : (atr_values[i] < threshold);
            if (result_check) {
                valid_count++;
            }
        }
    }
    
    bool pass = (valid_count == required_count);
    
    if (pass) {
        std::string comparisonOp = passIfSuperior ? " > " : " <= ";
        logger->log_filter_result(name, true, "ATR actuel: " + logger->fast_double_to_string(atr_values[0]) + comparisonOp + "Seuil: " + logger->fast_double_to_string(threshold) + " (Toutes les " + std::to_string(required_count) + " valeurs validées)");
    } else {
        std::string comparisonOp = passIfSuperior ? " en dessous " : " au-dessus ";
        logger->log_filter_result(name, false, std::to_string(valid_count) + "/" + std::to_string(required_count) + " valeurs ATR" + comparisonOp + "du seuil " + logger->fast_double_to_string(threshold));
    }

    return pass;
}
