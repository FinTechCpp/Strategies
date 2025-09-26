#pragma once

#include "Managers/CandleManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/LoggerManager.hpp"

#include <memory>
#include <functional>

class FilterEvaluator {
private:
    // Obtenir la valeur d'une source
    static double getSourceValue(const ValueSource& source, int additionalOffset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        int offset = source.historicalOffset + additionalOffset;

        switch (source.category) {
            case ValueCategory::PRICE: {
                // Récupérer la bougie correspondante
                if (candleManager.size() <= offset) {
                    if (logger) logger->log_general("Pas assez d'historique pour obtenir la valeur de prix", LogLevel::WARNING);
                    return 0.0;
                }

                std::vector<BasicCandle> candles = candleManager.get_last_candles(offset + 1);
                BasicCandle candle = candles[0];

                // Extraire la valeur de prix selon le type
                switch (source.priceType) {
                    case PriceType::CLOSE: return candle.close;
                    case PriceType::OPEN: return candle.open;
                    case PriceType::HIGH: return candle.high;
                    case PriceType::LOW: return candle.low;
                    case PriceType::TYPICAL: return (candle.high + candle.low + candle.close) / 3.0;
                    case PriceType::MEDIAN: return (candle.high + candle.low) / 2.0;
                }
                break;
            }
            
            case ValueCategory::CONSTANT: {
                return source.constantValue;
            }
            
            case ValueCategory::INDICATOR: {
                // Utiliser directement les objets de paramètres pour accéder aux indicateurs
                switch (source.indicatorType) {
                    case IndicatorType::EMA:
                        return indicatorManager.getEMAValue(source.emaParams, offset);
                        
                    case IndicatorType::RSI:
                        return indicatorManager.getRSIValue(source.rsiParams, offset);
                        
                    case IndicatorType::STOCHASTIC_K:
                        // Pour Stochastic K, on veut la première valeur de la paire
                        return indicatorManager.getStochasticValue(source.stochParams, offset).first;
                        
                    case IndicatorType::STOCHASTIC_D:
                        // Pour Stochastic D, on veut la deuxième valeur de la paire
                        return indicatorManager.getStochasticValue(source.stochParams, offset).second;
                        
                    case IndicatorType::ATR:
                        return indicatorManager.getATRValue(source.atrParams, offset);

                    case IndicatorType::SUPERTREND_VALUE:
                        // Pour SuperTrend Value, on veut la première valeur de la paire
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).first;

                    case IndicatorType::SUPERTREND_DIRECTION:
                        // Pour SuperTrend Direction, on veut la deuxième valeur de la paire
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).second;

                    default:
                        if (logger) logger->log_general("Type d'indicateur non supporté", LogLevel::ERROR);
                        return 0.0;
                }
            }
            
            case ValueCategory::CANDLE_PROPERTY: {
                if (candleManager.size() <= offset) {
                    if (logger) logger->log_general("Pas assez d'historique pour obtenir la propriété de bougie", LogLevel::WARNING);
                    return 0.0;
                }

                const BasicCandle& candle = candleManager.get_last_candles(offset + 1)[0];
                const BasicCandle& heikinAshiCandle = candleManager.get_last_heikin_ashi_candles(offset + 1)[0];
                
                switch (source.candlePropertyType) {
                    case CandlePropertyType::HEIKIN_ASHI_IS_GREEN:
                        return heikinAshiCandle.close > heikinAshiCandle.open ? 1.0 : 0.0;

                    case CandlePropertyType::HEIKIN_ASHI_IS_RED:
                        return heikinAshiCandle.close < heikinAshiCandle.open ? 1.0 : 0.0;

                    case CandlePropertyType::IS_GREEN:
                        return candle.close > candle.open ? 1.0 : 0.0;
                        
                    case CandlePropertyType::IS_RED:
                        return candle.close < candle.open ? 1.0 : 0.0;
                        
                    case CandlePropertyType::BODY_SIZE:
                        return std::abs(candle.close - candle.open);
                        
                    case CandlePropertyType::UPPER_SHADOW_SIZE:
                        return candle.high - std::max(candle.open, candle.close);
                        
                    case CandlePropertyType::LOWER_SHADOW_SIZE:
                        return std::min(candle.open, candle.close) - candle.low;
                        
                    case CandlePropertyType::RANGE:
                        return candle.high - candle.low;
                }
                break;
            }
        }
        
        return 0.0;
    }

    // Comparer deux valeurs selon l'opérateur spécifié
    static bool compareValues(double left, double right, ComparisonOperator op) {
        switch (op) {
            case ComparisonOperator::GREATER_THAN:
                return left > right;
                
            case ComparisonOperator::LESS_THAN:
                return left < right;
                
            case ComparisonOperator::GREATER_OR_EQUAL:
                return left >= right;
                
            case ComparisonOperator::LESS_OR_EQUAL:
                return left <= right;
                
            case ComparisonOperator::EQUAL:
                // Comparaison à epsilon près pour les flottants
                return std::abs(left - right) < 0.00001;
                
            case ComparisonOperator::NOT_EQUAL:
                return std::abs(left - right) >= 0.00001;
                
            // Il faut interpreter les croisements en faisant une duplication du filtre avec un offset de 1 et en liant les deux filtre par une condition de ET. ainsi par exmeple un croisement stochastique a la hausse sera vrai si le stochastique K est superieur au stochastique D sur la bougie courante ET que le stochastique K est inferieur ou egal au stochastique D sur la bougie precedente. Cela est en realité deux filtres : 
            // - Stochastique K > Stochastique D (temporal logic = CURRENT)
            // - Stochastique K <= Stochastique D (temporal logic = CURRENT, historical offset = 1)
            // et la condition entre les deux est un ET logique
            case ComparisonOperator::CROSSES_ABOVE: {
                // Vérifier si left a croisé right vers le haut
                // double prevLeft = getSourceValue(ValueSource::Price(PriceType::CLOSE), 1);
                // double prevRight = getSourceValue(ValueSource::Price(PriceType::CLOSE), 1);
                double prevLeft = 0.0;
                double prevRight = 0.0;
                return left > right && prevLeft <= prevRight;
            }
                
            case ComparisonOperator::CROSSES_BELOW: {
                // Vérifier si left a croisé right vers le bas
                // double prevLeft = getSourceValue(ValueSource::Price(PriceType::CLOSE), 1);
                // double prevRight = getSourceValue(ValueSource::Price(PriceType::CLOSE), 1);
                double prevLeft = 0.0;
                double prevRight = 0.0;
                return left < right && prevLeft >= prevRight;
            }
        }
        return false;
    }

    // Évaluer une condition de filtre à un offset donné
    static bool evaluateCondition(const GenericFilter& filter, int offset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        double leftValue = getSourceValue(filter.leftValue, offset, candleManager, indicatorManager, logger);
        double rightValue = getSourceValue(filter.rightValue, offset, candleManager, indicatorManager, logger);
        
        bool result;

        // Traitement spécial pour les croisements
        if (filter.op == ComparisonOperator::CROSSES_ABOVE || filter.op == ComparisonOperator::CROSSES_BELOW) {
            // Pour les croisements, nous devons comparer les valeurs actuelles et précédentes
            ValueSource prevLeftSource = filter.leftValue;
            ValueSource prevRightSource = filter.rightValue;
            prevLeftSource.historicalOffset += 1 + offset;
            prevRightSource.historicalOffset += 1 + offset;

            double prevLeftValue = getSourceValue(prevLeftSource, 0, candleManager, indicatorManager, logger);
            double prevRightValue = getSourceValue(prevRightSource, 0, candleManager, indicatorManager, logger);

            if (filter.op == ComparisonOperator::CROSSES_ABOVE) {
                result = (leftValue > rightValue) && (prevLeftValue <= prevRightValue);
                if (logger) {
                    logger->log_general("CROSSES_ABOVE check: left=" + std::to_string(leftValue) + 
                                        ", right=" + std::to_string(rightValue) + 
                                        ", prevLeft=" + std::to_string(prevLeftValue) + 
                                        ", prevRight=" + std::to_string(prevRightValue) + 
                                        ", result=" + (result ? "true" : "false"), LogLevel::DEBUG);
                }
            } else {
                result = (leftValue < rightValue) && (prevLeftValue >= prevRightValue);
                if (logger) {
                    logger->log_general("CROSSES_BELOW check: left=" + std::to_string(leftValue) + 
                                        ", right=" + std::to_string(rightValue) + 
                                        ", prevLeft=" + std::to_string(prevLeftValue) + 
                                        ", prevRight=" + std::to_string(prevRightValue) + 
                                        ", result=" + (result ? "true" : "false"), LogLevel::DEBUG);
                }
            }
        } else {
            // Comparaison normale
            result = compareValues(leftValue, rightValue, filter.op);
        }

        if (logger) {
            logger->log_general(
                "Évaluation filtre [T-" + std::to_string(offset) + "]: " +
                filter.leftValue.getDescription() + " (" + std::to_string(leftValue) + ") " +
                getOperatorString(filter.op) + " " +
                filter.rightValue.getDescription() + " (" + std::to_string(rightValue) + ") = " +
                (result ? "VRAI" : "FAUX"),
                LogLevel::DEBUG
            );
        }
        
        return result;
    }

public:
    // Évaluer un filtre complet avec sa logique temporelle
    static bool evaluate(const GenericFilter& filter, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger = nullptr) {
        if (!filter.enabled) return true;
        
        switch (filter.temporalLogic) {
            case TemporalLogic::CURRENT:
                return evaluateCondition(filter, 0, candleManager, indicatorManager, logger);
                
            case TemporalLogic::ANY_OF: {
                for (int i = 0; i < filter.lookbackPeriods; ++i) {
                    if (evaluateCondition(filter, i, candleManager, indicatorManager, logger)) {
                        return true;
                    }
                }
                return false;
            }
                
            case TemporalLogic::ALL_OF: {
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

private:
    // Obtenir une représentation textuelle de l'opérateur
    static std::string getOperatorString(ComparisonOperator op) {
        switch (op) {
            case ComparisonOperator::GREATER_THAN: return ">";
            case ComparisonOperator::LESS_THAN: return "<";
            case ComparisonOperator::GREATER_OR_EQUAL: return ">=";
            case ComparisonOperator::LESS_OR_EQUAL: return "<=";
            case ComparisonOperator::EQUAL: return "=";
            case ComparisonOperator::NOT_EQUAL: return "!=";
            case ComparisonOperator::CROSSES_ABOVE: return "^";
            case ComparisonOperator::CROSSES_BELOW: return "v";
        }
        return "?";
    }
};