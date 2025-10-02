#pragma once

#include "Managers/CandleManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/LoggerManager.hpp"

#include <memory>
#include <functional>

class FilterEvaluator {
private:
    // Obtenir la valeur d'une source
    static double getSourceValue(const filter::ValueSource& source, int additionalOffset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        int offset = source.historicalOffset + additionalOffset;
        BasicCandle candle = candleManager.get_last_candles(offset + 1)[0];

        switch (source.category) {
            case filter::ValueCategory::PRICE: {
                // Récupérer la bougie correspondante
                if (candleManager.size() <= offset) {
                    if (logger) logger->log_general("Pas assez d'historique pour obtenir la valeur de prix", LogLevel::WARNING);
                    return 0.0;
                }

                // Extraire la valeur de prix selon le type
                switch (source.priceType) {
                    case filter::PriceType::CLOSE: return candle.close;
                    case filter::PriceType::OPEN: return candle.open;
                    case filter::PriceType::HIGH: return candle.high;
                    case filter::PriceType::LOW: return candle.low;
                    case filter::PriceType::TYPICAL: return (candle.high + candle.low + candle.close) / 3.0;
                    case filter::PriceType::MEDIAN: return (candle.high + candle.low) / 2.0;
                }
                break;
            }
            
            case filter::ValueCategory::CONSTANT: {
                return source.constantValue;
            }
            
            case filter::ValueCategory::INDICATOR: {
                // Utiliser directement les objets de paramètres pour accéder aux indicateurs
                switch (source.indicatorType) {
                    case filter::IndicatorType::EMA:
                        return indicatorManager.getEMAValue(source.emaParams, offset);
                        
                    case filter::IndicatorType::RSI:
                        return indicatorManager.getRSIValue(source.rsiParams, offset);
                        
                    case filter::IndicatorType::STOCHASTIC_K:
                        // Pour Stochastic K, on veut la première valeur de la paire
                        return indicatorManager.getStochasticValue(source.stochParams, offset).first;
                        
                    case filter::IndicatorType::STOCHASTIC_D:
                        // Pour Stochastic D, on veut la deuxième valeur de la paire
                        return indicatorManager.getStochasticValue(source.stochParams, offset).second;
                        
                    case filter::IndicatorType::ATR:
                        return indicatorManager.getATRValue(source.atrParams, offset);

                    case filter::IndicatorType::SUPERTREND_VALUE:
                        // Pour SuperTrend Value, on veut la première valeur de la paire
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).first;

                    case filter::IndicatorType::SUPERTREND_DIRECTION:
                        // Pour SuperTrend Direction, on veut la deuxième valeur de la paire
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).second;

                    default:
                        if (logger) logger->log_general("Type d'indicateur non supporté", LogLevel::ERROR);
                        return 0.0;
                }
            }
            
            case filter::ValueCategory::CANDLE_PROPERTY: {
                if (candleManager.size() <= offset) {
                    if (logger) logger->log_general("Pas assez d'historique pour obtenir la propriété de bougie", LogLevel::WARNING);
                    return 0.0;
                }
                
                BasicCandle heikinAshiCandle = candleManager.get_last_heikin_ashi_candles(offset + 1)[0];
                
                switch (source.candlePropertyType) {
                    case filter::CandlePropertyType::HEIKIN_ASHI_IS_GREEN:
                        return heikinAshiCandle.close > heikinAshiCandle.open ? 1.0 : 0.0;

                    case filter::CandlePropertyType::HEIKIN_ASHI_IS_RED:
                        return heikinAshiCandle.close < heikinAshiCandle.open ? 1.0 : 0.0;

                    case filter::CandlePropertyType::IS_GREEN:
                        return candle.close > candle.open ? 1.0 : 0.0;
                        
                    case filter::CandlePropertyType::IS_RED:
                        return candle.close < candle.open ? 1.0 : 0.0;

                    case filter::CandlePropertyType::BODY_SIZE:
                        return std::abs(candle.close - candle.open);
                        
                    case filter::CandlePropertyType::UPPER_SHADOW_SIZE:
                        return candle.high - std::max(candle.open, candle.close);
                        
                    case filter::CandlePropertyType::LOWER_SHADOW_SIZE:
                        return std::min(candle.open, candle.close) - candle.low;
                        
                    case filter::CandlePropertyType::RANGE:
                        return candle.high - candle.low;
                }
                break;
            }
        }
        
        return 0.0;
    }

    // Comparer deux valeurs selon l'opérateur spécifié
    static bool compareValues(double left, double right, filter::ComparisonOperator op) {
        switch (op) {
            case filter::ComparisonOperator::GREATER_THAN:
                return left > right;
                
            case filter::ComparisonOperator::LESS_THAN:
                return left < right;
                
            case filter::ComparisonOperator::GREATER_OR_EQUAL:
                return left >= right;
                
            case filter::ComparisonOperator::LESS_OR_EQUAL:
                return left <= right;
                
            case filter::ComparisonOperator::EQUAL:
                // Comparaison à epsilon près pour les flottants
                return std::abs(left - right) < 0.00001;
                
            case filter::ComparisonOperator::NOT_EQUAL:
                return std::abs(left - right) >= 0.00001;
            
            case filter::ComparisonOperator::CROSSES_ABOVE:
            case filter::ComparisonOperator::CROSSES_BELOW:
                return false;
                
            case filter::ComparisonOperator::TRUE:
                return std::abs(left - 1.0) < 0.00001;
    
            case filter::ComparisonOperator::FALSE:
                return std::abs(left) < 0.00001;
        }
        return false;
    }

    // Évaluer une condition de filtre à un offset donné
    static bool evaluateCondition(const filter::GenericFilter& filter, int offset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        // Traitement spécial pour les croisements en les décomposant en filtres unitaires
        if (filter.op == filter::ComparisonOperator::CROSSES_ABOVE || filter.op == filter::ComparisonOperator::CROSSES_BELOW) {
            // Créer deux filtres unitaires
            filter::GenericFilter currentFilter = filter;
            filter::GenericFilter previousFilter = filter;
            
            if (filter.op == filter::ComparisonOperator::CROSSES_ABOVE) {
                // Pour CROSSES_ABOVE:
                // 1. Filtre actuel: left > right
                currentFilter.op = filter::ComparisonOperator::GREATER_THAN;
                
                // 2. Filtre précédent: left <= right
                previousFilter.op = filter::ComparisonOperator::LESS_OR_EQUAL;
                previousFilter.leftValue.historicalOffset += 1;
                previousFilter.rightValue.historicalOffset += 1;
            } 
            else { // CROSSES_BELOW
                // Pour CROSSES_BELOW:
                // 1. Filtre actuel: left < right
                currentFilter.op = filter::ComparisonOperator::LESS_THAN;
                
                // 2. Filtre précédent: left >= right
                previousFilter.op = filter::ComparisonOperator::GREATER_OR_EQUAL;
                previousFilter.leftValue.historicalOffset += 1;
                previousFilter.rightValue.historicalOffset += 1;
            }

            // Évaluer les deux filtres
            bool currentResult = evaluateCondition(currentFilter, offset, candleManager, indicatorManager, logger);
            bool previousResult = evaluateCondition(previousFilter, offset, candleManager, indicatorManager, logger);
            
            // Un croisement nécessite que les deux conditions soient vraies
            bool result = currentResult && previousResult;
            
            return result;
        }

        double leftValue = getSourceValue(filter.leftValue, offset, candleManager, indicatorManager, logger);
        double rightValue = getSourceValue(filter.rightValue, offset, candleManager, indicatorManager, logger);
        
        bool result = compareValues(leftValue, rightValue, filter.op);

        if (logger) {
            logger->log_filter_result(filter, leftValue, rightValue, result, offset, LogLevel::DEBUG);
        }
        
        return result;
    }

public:
    // Évaluer un filtre complet avec sa logique temporelle
    static bool evaluate(const filter::GenericFilter& filter, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        if (!filter.enabled) return true;
        
        switch (filter.temporalLogic) {
            case filter::TemporalLogic::ANY_OF: {
                for (int i = 0; i < filter.lookbackPeriods; ++i) {
                    if (evaluateCondition(filter, i, candleManager, indicatorManager, logger)) {
                        return true;
                    }
                }
                return false;
            }
                
            case filter::TemporalLogic::ALL_OF: {
                for (int i = 0; i < filter.lookbackPeriods; ++i) {
                    if (!evaluateCondition(filter, i, candleManager, indicatorManager, logger)) {
                        return false;
                    }
                }
                return true;
            }
        }
        
        return false;
    }
};