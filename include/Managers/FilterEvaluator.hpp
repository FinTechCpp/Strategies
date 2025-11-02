#pragma once

#include "Managers/CandleManager.hpp"
#include "Managers/IndicatorManager.hpp"
#include "Managers/LoggerManager.hpp"

#include <memory>
#include <functional>
#include <cmath>

class FilterEvaluator {
private:
    // Get the value of a source
    static double getSourceValue(const filter::ValueSource& source, int additionalOffset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger) {
        int offset = source.historicalOffset + additionalOffset;
        BasicCandle candle = candleManager.get_last_candles(offset + 1)[0];

        switch (source.category) {
            case filter::ValueCategory::PRICE: {
                // Retrieve the corresponding candle
                if (candleManager.size() <= offset) {
                    STRATEGY_LOG(logger, log_general, "Not enough history to get the price value", LogLevel::WARNING);
                    return 0.0;
                }

                // Extract the price value based on the type
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
                // Directly use parameter objects to access indicators
                switch (source.indicatorType) {
                    case filter::IndicatorType::EMA:
                        return indicatorManager.getEMAValue(source.emaParams, offset);
                        
                    case filter::IndicatorType::RSI:
                        return indicatorManager.getRSIValue(source.rsiParams, offset);
                        
                    case filter::IndicatorType::STOCHASTIC_K:
                        // For Stochastic K, we want the first value of the pair
                        return indicatorManager.getStochasticValue(source.stochParams, offset).first;
                        
                    case filter::IndicatorType::STOCHASTIC_D:
                        // For Stochastic D, we want the second value of the pair
                        return indicatorManager.getStochasticValue(source.stochParams, offset).second;
                        
                    case filter::IndicatorType::ATR:
                        return indicatorManager.getATRValue(source.atrParams, offset);

                    case filter::IndicatorType::SUPERTREND_VALUE:
                        // For SuperTrend Value, we want the first value of the pair
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).first;

                    case filter::IndicatorType::SUPERTREND_DIRECTION:
                        // For SuperTrend Direction, we want the second value of the pair
                        return indicatorManager.getSuperTrendValue(source.supertrendParams, offset).second;
                    
                    case filter::IndicatorType::CCI:
                        return indicatorManager.getCCIValue(source.cciParams, offset);
                    case filter::IndicatorType::MACD_HISTOGRAM:
                        return indicatorManager.getMACDValue(source.macdParams, offset).histogram;
                    case filter::IndicatorType::MACD_LINE:
                        return indicatorManager.getMACDValue(source.macdParams, offset).macdLine;
                    case filter::IndicatorType::MACD_SIGNAL:
                        return indicatorManager.getMACDValue(source.macdParams, offset).signalLine;
                    case filter::IndicatorType::BB_UPPER:
                        return indicatorManager.getBBValue(source.bbParams, offset).upper;
                    case filter::IndicatorType::BB_LOWER:
                        return indicatorManager.getBBValue(source.bbParams, offset).lower;
                    case filter::IndicatorType::BB_PERCENT_B:
                        return indicatorManager.getBBValue(source.bbParams, offset).percentB;
                    default:
                        STRATEGY_LOG(logger, log_general, "Unsupported indicator type", LogLevel::ERROR);
                        return 0.0;
                }
            }
            
            case filter::ValueCategory::CANDLE_PROPERTY: {
                if (candleManager.size() <= offset) {
                    STRATEGY_LOG(logger, log_general, "Not enough history to get the candle property", LogLevel::WARNING);
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

    // Compare two values based on the specified operator
    static bool compareValues(double left, double right, filter::ComparisonOperator op, double threshold) {
        double offset = left - right;
        switch (op) {
            case filter::ComparisonOperator::GREATER_THAN:
                // left is greater than right by more than threshold
                return offset > threshold;

            case filter::ComparisonOperator::LESS_THAN:
                // right is greater than left by more than threshold
                return offset < threshold;

            case filter::ComparisonOperator::GREATER_OR_EQUAL:
                // left is greater or equal to right with at least threshold separation
                return offset >= threshold;

            case filter::ComparisonOperator::LESS_OR_EQUAL:
                // left is less or equal to right with at least threshold separation
                return offset <= threshold;

            case filter::ComparisonOperator::EQUAL:
                // Comparison with epsilon for floating-point numbers
                return std::abs(left - right) < 0.00001;
                
            case filter::ComparisonOperator::NOT_EQUAL:
                return std::abs(left - right) >= 0.00001;
            
            case filter::ComparisonOperator::DISTANCE_LESS: {
                // absolute distance less than threshold
                return std::abs(left - right) < threshold;
            }

            case filter::ComparisonOperator::DISTANCE_GREATER: {
                // absolute distance greater than threshold
                return std::abs(left - right) > threshold;
            }
            
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

    // Evaluate a filter condition at a given offset
    static bool evaluateCondition(const filter::GenericFilter& filter, int offset, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger) {
        // Special handling for crossovers by breaking them into unit filters
        if (filter.op == filter::ComparisonOperator::CROSSES_ABOVE || filter.op == filter::ComparisonOperator::CROSSES_BELOW) {
            // Create two unit filters
            filter::GenericFilter currentFilter = filter;
            filter::GenericFilter previousFilter = filter;
            
            if (filter.op == filter::ComparisonOperator::CROSSES_ABOVE) {
                // For CROSSES_ABOVE:
                // 1. Current filter: left > right
                currentFilter.op = filter::ComparisonOperator::GREATER_THAN;
                
                // 2. Previous filter: left <= right
                previousFilter.op = filter::ComparisonOperator::LESS_OR_EQUAL;
                previousFilter.leftValue.historicalOffset += 1;
                previousFilter.rightValue.historicalOffset += 1;
            } 
            else { // CROSSES_BELOW
                // For CROSSES_BELOW:
                // 1. Current filter: left < right
                currentFilter.op = filter::ComparisonOperator::LESS_THAN;
                
                // 2. Previous filter: left >= right
                previousFilter.op = filter::ComparisonOperator::GREATER_OR_EQUAL;
                previousFilter.leftValue.historicalOffset += 1;
                previousFilter.rightValue.historicalOffset += 1;
            }

            // Evaluate both filters
            bool currentResult = evaluateCondition(currentFilter, offset, candleManager, indicatorManager, logger);
            bool previousResult = evaluateCondition(previousFilter, offset, candleManager, indicatorManager, logger);
            
            // A crossover requires both conditions to be true
            bool result = currentResult && previousResult;
            
            return result;
        }

        double leftValue = getSourceValue(filter.leftValue, offset, candleManager, indicatorManager, logger);
        double rightValue = getSourceValue(filter.rightValue, offset, candleManager, indicatorManager, logger);

        // Helper to apply transform to a ValueSource's value
        auto applyTransform = [&](double value, const filter::ValueSource& src, int off) -> double {
            switch (src.transform) {
                case filter::TransformType::NONE:
                    return value;
                case filter::TransformType::LOG:
                    return value > 0 ? std::log(value) : 0.0;
                case filter::TransformType::EXP:
                    return std::exp(value);
                case filter::TransformType::DERIVATIVE: {
                    // Compute finite-difference derivative (slope) over `lag` periods.
                    // We use a backward difference here: (x(t) - x(t-lag)) / lag, which
                    // corresponds to the average per-period change over the interval.
                    int lag = 1; // TODO: make lag configurable
                    double prev = getSourceValue(src, off + lag, candleManager, indicatorManager, logger);
                    return (value - prev) / static_cast<double>(lag);
                }
            }
            return value;
        };

   
        leftValue = applyTransform(leftValue, filter.leftValue, offset);
        rightValue = applyTransform(rightValue, filter.rightValue, offset);
        
        bool result = compareValues(leftValue, rightValue, filter.op, filter.offset);

        STRATEGY_LOG(logger, log_filter_result, filter, leftValue, rightValue, result, offset, LogLevel::DEBUG);
        return result;
    }

public:
    // Evaluate a complete filter with its temporal logic
    static bool evaluate(const filter::GenericFilter& filter, const CandleManager& candleManager, const IndicatorManager& indicatorManager, ILogger* logger) {
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