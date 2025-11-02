#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <array>
#include <functional>
#include <optional>


#ifdef DISABLE_LOGGING
    // In no-logging mode, all calls are replaced by no-ops
    #define STRATEGY_LOG(logger_ptr, method, ...) ((void)0)
    #define STRATEGY_LOG_VOID(logger_ptr, method) ((void)0)
#else
    // In logging mode, calls are normal
    #define STRATEGY_LOG(logger_ptr, method, ...) (logger_ptr)->method(__VA_ARGS__)
    #define STRATEGY_LOG_VOID(logger_ptr, method) (logger_ptr)->method()
#endif


namespace filter {
    // Types of values available for comparison
    enum class ValueCategory {
        PRICE,              // Candle price
        INDICATOR,          // Technical indicator
        CONSTANT,           // Constant value
        CANDLE_PROPERTY     // Specific candle property
    };

    // Available price types
    enum class PriceType {
        CLOSE,
        OPEN,
        HIGH,
        LOW,
        TYPICAL,    // (High + Low + Close) / 3
        MEDIAN      // (High + Low) / 2
    };

    // Available indicator types
    enum class IndicatorType {
        EMA,
        RSI,
        STOCHASTIC_K,
        STOCHASTIC_D,
        ATR,
        SUPERTREND_VALUE,
        SUPERTREND_DIRECTION,
        CCI,
        MACD_HISTOGRAM,
        MACD_LINE,
        MACD_SIGNAL,
        PIVOT_POINT,
        BB_UPPER,
        BB_LOWER,
        BB_PERCENT_B,
        TIME_SIN,           // Cyclical time encoding (sin)
        TIME_COS            // Cyclical time encoding (cos)
    };

    // Transformations that can be applied to indicator values
    enum class TransformType {
        NONE,
        LOG,
        EXP,
        DERIVATIVE
    };

    // Candle properties
    enum class CandlePropertyType {
        HEIKIN_ASHI_IS_GREEN,
        HEIKIN_ASHI_IS_RED,
        IS_GREEN,
        IS_RED,
        BODY_SIZE,
        UPPER_SHADOW_SIZE,
        LOWER_SHADOW_SIZE,
        RANGE            // High - Low
    };

    // Types of comparison operators
    enum class ComparisonOperator {
        GREATER_THAN,          // >
        LESS_THAN,             // <
        GREATER_OR_EQUAL,      // >=
        LESS_OR_EQUAL,         // <=
        EQUAL,                 // ==
        NOT_EQUAL,             // !=
        DISTANCE_LESS,         // |left - right| < threshold
        DISTANCE_GREATER,      // |left - right| > threshold
        CROSSES_ABOVE,         // Crosses above (current vs previous period)
        CROSSES_BELOW,         // Crosses below (current vs previous period)
        TRUE,                  // Shortcut for == Constant 1.0
        FALSE                  // Shortcut for == Constant 0.0
    };

    // Type of temporal logic
    enum class TemporalLogic {
        ANY_OF,           // At least one period (OR)
        ALL_OF,           // All periods (AND)
    };


    // TODO: maybe add an ordering relation so they can be put into a map
    // Parameters for EMA
    struct EMAParams {
        int period;

        explicit EMAParams(int p) : period(p) {}
        
        bool operator==(const EMAParams& other) const {
            return period == other.period;
        }

        bool operator<(const EMAParams& other) const {
            return period < other.period;
        }
    };

    // Parameters for RSI
    struct RSIParams {
        int period;

        explicit RSIParams(int p) : period(p) {}

        bool operator==(const RSIParams& other) const {
            return period == other.period;
        }

        bool operator<(const RSIParams& other) const {
            return period < other.period;
        }
    };

    // Parameters for Stochastic
    struct StochasticParams {
        int fastK;
        int slowK;
        int slowD;

        explicit StochasticParams(int fK, int sK, int sD) : fastK(fK), slowK(sK), slowD(sD) {}

        bool operator==(const StochasticParams& other) const {
            return fastK == other.fastK && 
                slowK == other.slowK && 
                slowD == other.slowD;
        }

        bool operator<(const StochasticParams& other) const {
            if (fastK != other.fastK) return fastK < other.fastK;
            if (slowK != other.slowK) return slowK < other.slowK;
            return slowD < other.slowD;
        }
    };

    // Parameters for ATR
    struct ATRParams {
        int period;
        bool useLog = false;

        explicit ATRParams(int p, bool log = false) : period(p), useLog(log) {}

        bool operator==(const ATRParams& other) const {
            return period == other.period && useLog == other.useLog;
        }

        bool operator<(const ATRParams& other) const {
            if (period != other.period) return period < other.period;
            return useLog < other.useLog;
        }
    };

    // Parameters for SuperTrend
    struct SuperTrendParams {
        int atrPeriod;
        double multiplier;

        explicit SuperTrendParams(int p, double m) : atrPeriod(p), multiplier(m) {}
        
        bool operator==(const SuperTrendParams& other) const {
            return atrPeriod == other.atrPeriod && 
                std::abs(multiplier - other.multiplier) < 0.0001;
        }

        bool operator<(const SuperTrendParams& other) const {
            if (atrPeriod != other.atrPeriod) return atrPeriod < other.atrPeriod;
            return multiplier < other.multiplier;
        }
    };

    // Parameters for CCI
    struct CCIParams {
        int period;
        
        explicit CCIParams(int p = 20) : period(p) {}
        
        bool operator<(const CCIParams& other) const {
            return period < other.period;
        }
        
        bool operator==(const CCIParams& other) const {
            return period == other.period;
        }
    };



    // Parameters for MACD
    enum class MAType {
        EMA,
        SMA
    };

    enum class MACDSignalType {
        MACD_LINE,
        SIGNAL_LINE,
        HISTOGRAM
    };

    // Result struct for MACD to be shared across modules
    struct MACDResult {
        double macdLine = 0.0;
        double signalLine = 0.0;
        double histogram = 0.0;

        MACDResult() = default;
        MACDResult(double m, double s, double h) : macdLine(m), signalLine(s), histogram(h) {}
    };

    struct MACDParams {
        // Required
        int fast;   // fast period 
        int slow;   // slow period 
        int signal; // signal period 

        PriceType source;

        MAType osc_ma_type;
        MAType signal_ma_type;

        int signal_smoothing;

        MACDParams(int fastPeriod = 12,
                   int slowPeriod = 26,
                   int signalPeriod = 9,
                   PriceType src = PriceType::CLOSE,
                   MAType oscType = MAType::EMA,
                   MAType sigType = MAType::EMA,
                   int sigSmoothing = 0)
            : fast(fastPeriod),
              slow(slowPeriod),
              signal(signalPeriod),
              source(std::move(src)),
              osc_ma_type(std::move(oscType)),
              signal_ma_type(std::move(sigType)),
              signal_smoothing(sigSmoothing)
        {}

        bool operator==(const MACDParams& other) const {
            return fast == other.fast &&
                   slow == other.slow &&
                   signal == other.signal &&
                   source == other.source &&
                   osc_ma_type == other.osc_ma_type &&
                   signal_ma_type == other.signal_ma_type &&
                   signal_smoothing == other.signal_smoothing;
        }

        bool operator<(const MACDParams& other) const {
            if (fast != other.fast) return fast < other.fast;
            if (slow != other.slow) return slow < other.slow;
            if (signal != other.signal) return signal < other.signal;
            if (source != other.source) return source < other.source;
            if (osc_ma_type != other.osc_ma_type) return osc_ma_type < other.osc_ma_type;
            if (signal_ma_type != other.signal_ma_type) return signal_ma_type < other.signal_ma_type;
            return signal_smoothing < other.signal_smoothing;
        }
    };

    // Parameters for BB
    struct BBResult {
        double middle;
        double upper;
        double lower;
        double percentB; // (price - lower) / (upper - lower)
        BBResult() : middle(0.0), upper(0.0), lower(0.0), percentB(0.0) {}
        BBResult(double m, double u, double l, double p) : middle(m), upper(u), lower(l), percentB(p) {}
    };

    struct BBParams {
        int period;
        double stddev_multiplier;
        int offset = 0; // offset to get historical value (0 = current)

        // Additional options
        int source = 3;      // 0=Open, 1=High, 2=Low, 3=Close
        int ma_type = 0;     // 0=SMA, 1=EMA

        BBParams(int p = 20, double m = 2.0, int off = 0)
            : period(p), stddev_multiplier(m), offset(off) {}

        bool operator==(const BBParams& other) const {
            return period == other.period &&
                   std::abs(stddev_multiplier - other.stddev_multiplier) < 0.0001 &&
                   offset == other.offset &&
                   source == other.source &&
                   ma_type == other.ma_type;
        }

        bool operator<(const BBParams& other) const {
            if (period != other.period) return period < other.period;
            if (std::abs(stddev_multiplier - other.stddev_multiplier) >= 0.0001)
                return stddev_multiplier < other.stddev_multiplier;
            if (offset != other.offset) return offset < other.offset;
            if (source != other.source) return source < other.source;
            return ma_type < other.ma_type;
        }
    };

    // Parameters for TimeCyclic (cyclical time encoding)
    struct TimeCyclicParams {
        // No parameters needed for now
        // Calculation is based only on candle timestamp
        
        TimeCyclicParams() = default;
        
        bool operator==(const TimeCyclicParams& other) const {
            (void)other; // Avoid unused parameter warning
            return true; // All TimeCyclic instances are identical
        }
        
        bool operator<(const TimeCyclicParams& other) const {
            (void)other; // Avoid unused parameter warning
            return false; // No order between identical instances
        }
    };

    // Unified structure for a value source
    struct ValueSource {
        ValueCategory category;

        // Specific subtypes per category
        union {
            PriceType priceType;
            IndicatorType indicatorType;
            CandlePropertyType candlePropertyType;
        };

        // Indicator-specific parameters
        union {
            EMAParams emaParams;
            RSIParams rsiParams;
            StochasticParams stochParams;
            ATRParams atrParams;
            SuperTrendParams supertrendParams;
            CCIParams cciParams;
            MACDParams macdParams;
            BBParams bbParams;
            TimeCyclicParams timeCyclicParams;
        };

        // Constant value if category is CONSTANT
        double constantValue = 0.0;

        // Offset for historical values
        int historicalOffset = 0;

        // Transform applied to the value (default none)
        TransformType transform = TransformType::NONE;

        // Explicit default constructor to initialize unions safely
        ValueSource()
            : category(ValueCategory::PRICE),
            priceType(PriceType::CLOSE),
            emaParams(0),
            constantValue(0.0),
            historicalOffset(0),
            transform(TransformType::NONE)
        {
        }

        // Specific constructors for each category
        // TODO replace constructors with params structures
        
        // For price
        static ValueSource Price(PriceType type, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::PRICE;
            source.priceType = type;
            source.historicalOffset = offset;
            return source;
        }
        
        // For a constant
        static ValueSource Constant(double value) {
            ValueSource source;
            source.category = ValueCategory::CONSTANT;
            source.constantValue = value;
            return source;
        }
        
        // For EMA
        static ValueSource EMA(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::EMA;
            source.emaParams.period = period;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        
        // For RSI
        static ValueSource RSI(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::RSI;
            source.rsiParams.period = period;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        
        // For Stochastic K
        static ValueSource StochasticK(int fastK, int slowK, int slowD, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::STOCHASTIC_K;
            source.stochParams.fastK = fastK;
            source.stochParams.slowK = slowK;
            source.stochParams.slowD = slowD;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        
        // For Stochastic D
        static ValueSource StochasticD(int fastK, int slowK, int slowD, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::STOCHASTIC_D;
            source.stochParams.fastK = fastK;
            source.stochParams.slowK = slowK;
            source.stochParams.slowD = slowD;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        
        // For ATR
        static ValueSource ATR(int period, bool useLog = true, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::ATR;
            source.atrParams.period = period;
            source.atrParams.useLog = useLog;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        
        // For SuperTrend (value)
        static ValueSource SuperTrend(int atrPeriod, double multiplier, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::SUPERTREND_VALUE;
            source.supertrendParams.atrPeriod = atrPeriod;
            source.supertrendParams.multiplier = multiplier;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }
        // For CCI (value)
        static ValueSource CCI(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::CCI;
            source.cciParams.period = period;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For MACD (histogram)
        static ValueSource MACDHistogram(int fast, int slow, int signal, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::MACD_HISTOGRAM;
            source.macdParams.fast = fast;
            source.macdParams.slow = slow;
            source.macdParams.signal = signal;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For MACD (line)
        static ValueSource MACDLine(int fast, int slow, int signal, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::MACD_LINE;
            source.macdParams.fast = fast;
            source.macdParams.slow = slow;
            source.macdParams.signal = signal;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For MACD (signal line)
        static ValueSource MACDSignal(int fast, int slow, int signal, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::MACD_SIGNAL;
            source.macdParams.fast = fast;
            source.macdParams.slow = slow;
            source.macdParams.signal = signal;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For Bollinger Bands (UPPER)
        static ValueSource BollingerUpper(int period, double stdDevMultiplier, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::BB_UPPER;
            source.bbParams.period = period;
            source.bbParams.stddev_multiplier = stdDevMultiplier;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For Bollinger Bands (LOWER)
        static ValueSource BollingerLower(int period, double stdDevMultiplier, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::BB_LOWER;
            source.bbParams.period = period;
            source.bbParams.stddev_multiplier = stdDevMultiplier;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For Bollinger Bands (%B)
        static ValueSource BollingerPercentB(int period, double stdDevMultiplier, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::BB_PERCENT_B;
            source.bbParams.period = period;
            source.bbParams.stddev_multiplier = stdDevMultiplier;
            source.historicalOffset = offset;
            source.transform = TransformType::NONE;
            return source;
        }

        // For candle properties
        static ValueSource CandleProperty(CandlePropertyType type, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::CANDLE_PROPERTY;
            source.candlePropertyType = type;
            source.historicalOffset = offset;
            return source;
        }

        // Method to get a human-readable description of the source
        static std::string description(const ValueSource& source) {
            std::string desc;
            // transformPrefix may be used for indicator descriptions; declare it
            // here (outside the switch) to avoid crossing initialization when
            // the compiler jumps between case labels.
            std::string transformPrefix;

            switch (source.category) {
                case ValueCategory::PRICE:
                    desc = "Price ";
                    switch (source.priceType) {
                        case PriceType::CLOSE: desc += "Close"; break;
                        case PriceType::OPEN: desc += "Open"; break;
                        case PriceType::HIGH: desc += "High"; break;
                        case PriceType::LOW: desc += "Low"; break;
                        case PriceType::TYPICAL: desc += "Typical"; break;
                        case PriceType::MEDIAN: desc += "Median"; break;
                    }
                    break;
                    
                case ValueCategory::CONSTANT:
                    desc = std::to_string(source.constantValue);
                    break;
                    
                case ValueCategory::INDICATOR: {
                    // If a transform is set, set the prefix to wrap the indicator
                    // description. transformPrefix is declared above to avoid
                    // switching initialization issues inside a switch-case.
                    switch (source.transform) {
                        case TransformType::NONE: transformPrefix = ""; break;
                        case TransformType::LOG: transformPrefix = "log("; break;
                        case TransformType::EXP: transformPrefix = "exp("; break;
                        case TransformType::DERIVATIVE: transformPrefix = "derivative("; break;
                    }

                    switch (source.indicatorType) {
                        case IndicatorType::EMA: 
                            desc = "EMA(" + std::to_string(source.emaParams.period) + ")"; 
                            break;
                        case IndicatorType::RSI: 
                            desc = "RSI(" + std::to_string(source.rsiParams.period) + ")"; 
                            break;
                        case IndicatorType::STOCHASTIC_K: 
                            desc = "Stochastic K(" + std::to_string(source.stochParams.fastK) + "," + 
                                std::to_string(source.stochParams.slowK) + "," + 
                                std::to_string(source.stochParams.slowD) + ")";
                            break;
                        case IndicatorType::STOCHASTIC_D: 
                            desc = "Stochastic D(" + std::to_string(source.stochParams.fastK) + "," + 
                                std::to_string(source.stochParams.slowK) + "," + 
                                std::to_string(source.stochParams.slowD) + ")";
                            break;
                        case IndicatorType::ATR: 
                            desc = std::string(source.atrParams.useLog ? "ATRLOG(" : "ATR(") + 
                                std::to_string(source.atrParams.period) + ")";
                            break;
                        case IndicatorType::SUPERTREND_VALUE:
                            desc = "SuperTrend(" + std::to_string(source.supertrendParams.atrPeriod) + "," +
                                std::to_string(source.supertrendParams.multiplier) + ")";
                            break;
                        case IndicatorType::SUPERTREND_DIRECTION:
                            desc = "SuperTrend Direction(" + std::to_string(source.supertrendParams.atrPeriod) + "," +
                                std::to_string(source.supertrendParams.multiplier) + ")";
                            break;
                        case IndicatorType::CCI:
                            desc = "CCI(" + std::to_string(source.cciParams.period) + ")";
                            break;
                        case IndicatorType::PIVOT_POINT:
                            desc = "Pivot Point";
                            break;
                        case IndicatorType::MACD_HISTOGRAM: 
                            desc = "MACD Histogram(" + std::to_string(source.macdParams.fast) + "," +
                                std::to_string(source.macdParams.slow) + "," +
                                std::to_string(source.macdParams.signal) + ")";
                            break;
                        case IndicatorType::MACD_LINE: 
                            desc = "MACD Line(" + std::to_string(source.macdParams.fast) + "," +
                                std::to_string(source.macdParams.slow) + "," +
                                std::to_string(source.macdParams.signal) + ")";
                            break;
                        case IndicatorType::MACD_SIGNAL: 
                            desc = "MACD Signal(" + std::to_string(source.macdParams.fast) + "," +
                                std::to_string(source.macdParams.slow) + "," +
                                std::to_string(source.macdParams.signal) + ")";
                            break;
                        case IndicatorType::BB_UPPER:
                            desc = "Bollinger Bands Upper(" + std::to_string(source.bbParams.period) + "," +
                                std::to_string(source.bbParams.stddev_multiplier) + ")";
                            break;
                        case IndicatorType::BB_LOWER:
                            desc = "Bollinger Bands Lower(" + std::to_string(source.bbParams.period) + "," +
                                std::to_string(source.bbParams.stddev_multiplier) + ")";
                            break;
                        case IndicatorType::BB_PERCENT_B:   
                            desc = "Bollinger Bands %B(" + std::to_string(source.bbParams.period) + "," +
                                std::to_string(source.bbParams.stddev_multiplier) + ")"; 
                            break;                        
                        default:
                            break;
                        }
                    // If we added a transformPrefix, wrap the description accordingly
                    if (!transformPrefix.empty()) 
                        desc = transformPrefix + desc + ")";
                    }
                    break;
                    
                case ValueCategory::CANDLE_PROPERTY:
                    desc = "Candle ";
                    switch (source.candlePropertyType) {
                        case CandlePropertyType::HEIKIN_ASHI_IS_GREEN: desc += "Heikin-Ashi Green"; break;
                        case CandlePropertyType::HEIKIN_ASHI_IS_RED: desc += "Heikin-Ashi Red"; break;
                        case CandlePropertyType::IS_GREEN: desc += "Is Green"; break;
                        case CandlePropertyType::IS_RED: desc += "Is Red"; break;
                        case CandlePropertyType::BODY_SIZE: desc += "Body Size"; break;
                        case CandlePropertyType::UPPER_SHADOW_SIZE: desc += "Upper Shadow Size"; break;
                        case CandlePropertyType::LOWER_SHADOW_SIZE: desc += "Lower Shadow Size"; break;
                        case CandlePropertyType::RANGE: desc += "Range"; break;
                    }
                    break;
            }

            if (source.historicalOffset > 0) 
                desc += " [T-" + std::to_string(source.historicalOffset) + "]";
            
            return desc;
            }
    };

    // Structure for a complete filter
    struct GenericFilter {
        ValueSource leftValue;
        ValueSource rightValue;
        ComparisonOperator op;
        double offset = 0.0;
        TemporalLogic temporalLogic = TemporalLogic::ALL_OF;
        int lookbackPeriods = 1;
        bool enabled = true;
        std::string description;

        GenericFilter() = default;
                
        // Convenience constructor for common cases
        GenericFilter(ValueSource left, 
                    ComparisonOperator comp, 
                    ValueSource right,
                    TemporalLogic logic,
                    int periods = 1) 
            : leftValue(left), 
            rightValue(right), 
            op(comp), 
            temporalLogic(logic), 
            lookbackPeriods(periods),
            enabled(true) {
            
            // Generate an automatic description if none is provided
            description = autoGenerateDescription();
        }
        
        // Generate a human-readable description of the filter
        std::string autoGenerateDescription() const {
            std::string opStr;
            switch (op) {
                case ComparisonOperator::GREATER_THAN: opStr = ">"; break;
                case ComparisonOperator::LESS_THAN: opStr = "<"; break;
                case ComparisonOperator::GREATER_OR_EQUAL: opStr = ">="; break;
                case ComparisonOperator::LESS_OR_EQUAL: opStr = "<="; break;
                case ComparisonOperator::EQUAL: opStr = "="; break;
                case ComparisonOperator::NOT_EQUAL: opStr = "≠"; break;
                case ComparisonOperator::CROSSES_ABOVE: opStr = "crosses above"; break;
                case ComparisonOperator::CROSSES_BELOW: opStr = "crosses below"; break;
                case ComparisonOperator::DISTANCE_LESS: opStr = "distance <"; break;
                case ComparisonOperator::DISTANCE_GREATER: opStr = "distance >"; break;
                case ComparisonOperator::TRUE: opStr = "is true"; break;
                case ComparisonOperator::FALSE: opStr = "is false"; break;
            }

            // Describe temporal logic
            std::string timeLogicStr;
            if (temporalLogic == TemporalLogic::ANY_OF) {
                timeLogicStr = " (at least one period)";
            } else {
                timeLogicStr = " (all periods)";
            }
            if (lookbackPeriods > 1) {
                timeLogicStr += " over " + std::to_string(lookbackPeriods) + " periods";
            }

            // Build left/right descriptions
            std::string leftDesc = ValueSource::description(leftValue);
            std::string rightDesc;
            if (op == ComparisonOperator::TRUE) {
                rightDesc = "TRUE";
            } else if (op == ComparisonOperator::FALSE) {
                rightDesc = "FALSE";
            } else {
                rightDesc = ValueSource::description(rightValue);
            }

            return leftDesc + " " + opStr + " " + rightDesc + timeLogicStr;
        }
    };
}

enum class SignalType {
    NONE,
    BUY,
    SELL,
    // REBUY,
    // RESALE,
    MOVE_SL,
    LIQUIDATE
};

 // TODO use std::optional
struct Signal {
    SignalType type = SignalType::NONE;
    // For BUY/SELL and REBUY/RESALE
    double quantity = 0.0;
    // This is to define the desired execution price (limit/stop/market but should be more explicit)
    double price = 0.0;
    // For BUY/SELL only
    double take_profit = 0.0;
    double stop_loss = 0.0;
    // For MOVE_SL only
    double new_sl = 0.0;
};

struct Time {
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool operator<(const Time& other) const;
    bool operator<=(const Time& other) const;
    bool operator==(const Time& other) const;
    bool operator!=(const Time& other) const;

    Time() = default;

    Time(int h, int m, int s = 0)
        : hour(h), minute(m), second(s) {}
};

struct DateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    Time time;

    bool is_valid() const;
    bool operator==(const DateTime& other) const;
    bool operator!=(const DateTime& other) const;
    bool operator<(const DateTime& other) const;
    bool operator<=(const DateTime& other) const;
    bool operator>(const DateTime& other) const;
    bool operator>=(const DateTime& other) const;
    std::string to_string() const;
};

// Logging levels for the strategy
enum LogLevel {
    CRITICAL = 50,
    FATAL = CRITICAL,
    ERROR = 40,
    WARNING = 30,
    WARN = WARNING,
    INFO = 20,
    DEBUG = 10,
    NOTSET = 0
};

// Overload the << operator for LogLevel to print it as a string
inline std::ostream& operator<<(std::ostream& os, const LogLevel& level) {
    switch (level) {
        case LogLevel::CRITICAL:
            return os << "CRITICAL";
        case LogLevel::ERROR:
            return os << "ERROR";
        case LogLevel::WARNING:
            return os << "WARNING";
        case LogLevel::INFO:
            return os << "INFO";
        case LogLevel::DEBUG:
            return os << "DEBUG";
        case LogLevel::NOTSET:
            return os << "NOTSET";
        default:
            return os << "UNKNOWN(" << static_cast<int>(level) << ")";
    }
}

struct BasicCandle {
    DateTime date;
    double open;
    double high;
    double low;
    double close;

    BasicCandle() = default;

    BasicCandle(const DateTime& dt, double o, double h, double l, double c)
        : date(dt), open(o), high(h), low(l), close(c) {}
};

// Structure for position/trading data
struct PositionInfo {
    // For break-even, if the strategy does not use break-even, not necessary
    double entry_price = 0.0;
    double take_profit_price = 0.0;

    // For daily maximum loss, if the strategy does not use daily maximum loss, not necessary
    double closed_trade_pnl = 0.0;

    PositionInfo() = default;
};

// Composition rather than inheritance for the structure used in strategies
struct Candle {
    BasicCandle ohlc;
    PositionInfo position;

    Candle() = default;

    // Convenient constructor for OHLC data
    Candle(const DateTime& dt, double o, double h, double l, double c)
        : ohlc(dt, o, h, l, c) {}

    // Complete constructor
    Candle(const BasicCandle& basic, const PositionInfo& pos)
        : ohlc(basic), position(pos) {}

    // Convenient accessors to avoid writing candle.ohlc.xxx
    double open() const { return ohlc.open; }
    double high() const { return ohlc.high; }
    double low() const { return ohlc.low; }
    double close() const { return ohlc.close; }
    const DateTime& date() const { return ohlc.date; }
};

enum class StopLossMethod {
    Unset = -1,
    Fixed = 0,
    ATR = 1,
    MinMax = 2
};

enum class TakeProfitMethod {
    Unset = -1,
    Fixed = 0,
    ATR = 1,
    SLRatio = 2,
    SuperTrend = 3,
    RL = 4,
    NthHeikinAshi = 5
};

enum class TradeDirection {
    LONG,
    SHORT
};

// We could use unions to separate parameters of different SL and TP methods
struct StrategyConfig {
    std::string name;

    // Logging
    bool enable_logging = true; // Enable or disable logging
    LogLevel logLevel = LogLevel::DEBUG;

    // Filters that, when true, should trigger a buy/sell signal (opening a position)
    std::vector<filter::GenericFilter> buyFilters;
    std::vector<filter::GenericFilter> sellFilters;

    // Filters that, when true, should trigger a resale/rebuy signal (closing a position)
    std::vector<filter::GenericFilter> resaleFilters;
    std::vector<filter::GenericFilter> rebuyFilters;

    // Time settings
    Time trading_from;
    Time trading_to;
    bool trading_days_array[7]; // (0 = Monday, 6 = Sunday)

    // Fixed SL/TP values
    double take_profit_distance;
    double stop_loss_distance;

    // SL/TP Methods
    StopLossMethod sl_method = StopLossMethod::Unset;
    TakeProfitMethod tp_method = TakeProfitMethod::Unset;

    // ATR parameters for SL and TP
    int atr_period;
    double stop_loss_atr_multiplier;
    double take_profit_atr_multiplier;
    double min_stop_loss_distance;
    double min_take_profit_distance;

    // New Min/Max parameters for SL
    int sl_minmax_periods;
    double sl_minmax_delta_coef_atr;

    // New parameter for TP based on SL
    double tp_sl_ratio;

    // New parameters for TP based on ML/RL
    std::string rl_model_path; // Path to the ML model
    int rl_lookback_periods; // Number of historical candles to include in features
    double rl_tp_max_multiplier; // Maximum TP distance as multiple of SL distance
    double rl_tp_min_multiplier; // Minimum TP distance as multiple of SL distance

    // Risk management
    bool use_risk_based_sizing;
    double risk_percentage;
    double cash;
    double leverage_limit;
    
    // Capital allocation
    double cash_allocation_percentage = 100.0; // Percentage of broker cash allocated to this strategy (default: 100%)

    // Break-even parameters
    bool use_break_even;
    double break_even_threshold;
    double break_even_offset_per_mille; // Per mille of entry price to move BE relative to entry price

    // Daily maximum loss
    bool use_daily_max_loss;
    double daily_max_loss_percentage;

    // Daily maximum profit
    bool use_daily_max_profit;
    double daily_max_profit_percentage;

    // Daily maximum drawdown
    bool use_daily_max_drawdown;
    double daily_max_drawdown_percentage;

    // Machine Learning parameters for entry signals
    bool use_ml_entry = false; // Use ML model instead of filters for entry signals
    std::string ml_entry_model_path; // Path to the entry ML model
    int ml_entry_lookback_periods; // Number of historical candles for ML features
    float ml_entry_threshold; // Probability threshold for signal generation
    bool ml_entry_normalize; // Normalize features (Z-score)

    // ML Feature configuration - list of indicators to use as features
    struct MLFeatureConfig {
        filter::IndicatorType type;
        filter::TransformType transform = filter::TransformType::NONE;
        std::string custom_name; // Optional custom name for the feature
        
        // Indicator-specific parameters (union-like approach - only relevant fields are used)
        struct IndicatorParams {
            // Generic single period (EMA, RSI, ATR, CCI)
            int period;
            
            // For indicators with multiplier (SuperTrend, Bollinger Bands)
            double multiplier;
            
            // Stochastic-specific
            int k_period;
            int d_period;
            int smooth;
            
            // MACD-specific
            int fast_period;
            int slow_period ;
            int signal_period;
            
            IndicatorParams() = default;
        } params;
        
        // Composite feature configuration (for features like BB_WIDTH = distance(BB_UPPER, BB_LOWER))
        bool is_composite = false;
        filter::ComparisonOperator composite_operation = filter::ComparisonOperator::GREATER_THAN;
        filter::IndicatorType composite_right_type = filter::IndicatorType::EMA;
        IndicatorParams composite_right_params;
        
        MLFeatureConfig() : type(filter::IndicatorType::EMA) {}
        
        bool operator==(const MLFeatureConfig& other) const {
            if (type != other.type || transform != other.transform || custom_name != other.custom_name)
                return false;
            if (is_composite != other.is_composite)
                return false;
            if (is_composite) {
                return composite_operation == other.composite_operation &&
                       composite_right_type == other.composite_right_type;
            }
            // For simple features, compare relevant params based on indicator type
            return params.period == other.params.period &&
                   params.multiplier == other.params.multiplier &&
                   params.k_period == other.params.k_period &&
                   params.d_period == other.params.d_period &&
                   params.smooth == other.params.smooth &&
                   params.fast_period == other.params.fast_period &&
                   params.slow_period == other.params.slow_period &&
                   params.signal_period == other.params.signal_period;
        }
    };
    std::vector<MLFeatureConfig> ml_entry_features; // List of features to compute for ML
};


// Overload the << operator for StopLossMethod
inline std::ostream& operator<<(std::ostream& os, const StopLossMethod& method) {
    switch (method) {
        case StopLossMethod::Unset:
            return os << "Unset";
        case StopLossMethod::Fixed:
            return os << "Fixed";
        case StopLossMethod::ATR:
            return os << "ATR";
        case StopLossMethod::MinMax:
            return os << "MinMax";
        default:
            return os << "Unknown(" << static_cast<int>(method) << ")";
    }
}

// Overload the << operator for TakeProfitMethod
inline std::ostream& operator<<(std::ostream& os, const TakeProfitMethod& method) {
    switch (method) {
        case TakeProfitMethod::Unset:
            return os << "Unset";
        case TakeProfitMethod::Fixed:
            return os << "Fixed";
        case TakeProfitMethod::ATR:
            return os << "ATR";
        case TakeProfitMethod::SLRatio:
            return os << "SLRatio";
        case TakeProfitMethod::RL:
            return os << "RL";
        default:
            return os << "Unknown(" << static_cast<int>(method) << ")";
    }
}

// Overload of the stream operator for StrategyConfig
inline std::ostream& operator<<(std::ostream& os, const StrategyConfig& config) {
    os << "\n╔══════════════════════════════════════════════════════╗\n";
    os << "║              STRATEGY CONFIGURATION                  ║\n";
    os << "╚══════════════════════════════════════════════════════╝\n";
    
    os << "GENERAL\n";
    os << "  Name: " << config.name << "\n";
    os << "  Log level: " << config.logLevel << "\n";
    os << "  Logging enabled: " << (config.enable_logging ? "Yes" : "No") << "\n";
    
    os << "\nBUY FILTERS\n";
    if (config.buyFilters.empty()) {
        os << "  No filters\n";
    } else {
        for (const auto& filter : config.buyFilters)
            os << "  • " << filter.description << "\n";
    }

    os << "\nSELL FILTERS\n";
    if (config.sellFilters.empty()) {
        os << "  No filters\n";
    } else {
        for (const auto& filter : config.sellFilters)
            os << "  • " << filter.description << "\n";
    }

    os << "\nRESALE FILTERS\n";
    if (config.resaleFilters.empty()) {
        os << "  No filters\n";
    } else {
        for (const auto& filter : config.resaleFilters)
            os << "  • " << filter.description << "\n";
    }

    os << "\nREBUY FILTERS\n";
    if (config.rebuyFilters.empty()) {
        os << "  No filters\n";
    } else {
        for (const auto& filter : config.rebuyFilters)
            os << "  • " << filter.description << "\n";
    }

    os << "\nTRADING HOURS\n";
    os << "  Time range: " 
       << std::setfill('0') << std::setw(2) << config.trading_from.hour << ":" 
       << std::setfill('0') << std::setw(2) << config.trading_from.minute 
       << " - " 
       << std::setfill('0') << std::setw(2) << config.trading_to.hour << ":" 
       << std::setfill('0') << std::setw(2) << config.trading_to.minute << "\n";
    
    static const char* day_names[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    os << "  Active days: ";
    bool first = true;
    for (size_t i = 0; i < 7; ++i) {
        if (config.trading_days_array[i]) {
            if (!first) os << ", ";
            os << day_names[i];
            first = false;
        }
    }
    if (first) os << "None";
    os << "\n";
    
    os << "\nSTOP LOSS & TAKE PROFIT\n";
    os << "  SL Method: " << config.sl_method << "\n";
    os << "  TP Method: " << config.tp_method << "\n";
    
    if (config.sl_method == StopLossMethod::Fixed) {
        os << "  Fixed SL distance: " << config.stop_loss_distance << "\n";
    } else if (config.sl_method == StopLossMethod::ATR) {
        os << "  ATR period: " << config.atr_period << "\n";
        os << "  ATR multiplier (SL): " << config.stop_loss_atr_multiplier << "\n";
        os << "  Minimum SL distance: " << config.min_stop_loss_distance << "\n";
    } else if (config.sl_method == StopLossMethod::MinMax) {
        os << "  Min/Max periods: " << config.sl_minmax_periods << "\n";
        os << "  ATR delta coefficient: " << config.sl_minmax_delta_coef_atr << "\n";
    }
    
    if (config.tp_method == TakeProfitMethod::Fixed) {
        os << "  Fixed TP distance: " << config.take_profit_distance << "\n";
    } else if (config.tp_method == TakeProfitMethod::ATR) {
        os << "  ATR multiplier (TP): " << config.take_profit_atr_multiplier << "\n";
        os << "  Minimum TP distance: " << config.min_take_profit_distance << "\n";
    } else if (config.tp_method == TakeProfitMethod::SLRatio) {
        os << "  TP/SL Ratio: " << config.tp_sl_ratio << "\n";
    }
    
    os << "\nRISK MANAGEMENT\n";
    os << "  Risk-based sizing: " << (config.use_risk_based_sizing ? "Yes" : "No") << "\n";
    if (config.use_risk_based_sizing) {
        os << "  Risk per trade: " << config.risk_percentage << "%\n";
    }
    os << "  Capital: " << config.cash << "\n";
    os << "  Max leverage: " << config.leverage_limit << "\n";
    
    os << "\nBREAK-EVEN\n";
    os << "  Enabled: " << (config.use_break_even ? "Yes" : "No") << "\n";
    if (config.use_break_even) {
        os << "  Threshold: " << (config.break_even_threshold * 100) << "% of TP\n";
        os << "  Offset: " << config.break_even_offset_per_mille << "‰\n";
    }
    
    os << "\nDAILY LIMITS\n";
    os << "  Daily max loss: " << (config.use_daily_max_loss ? "Yes" : "No");
    if (config.use_daily_max_loss) {
        os << " (" << config.daily_max_loss_percentage << "%)";
    }
    os << "\n";
    
    os << "  Daily max profit: " << (config.use_daily_max_profit ? "Yes" : "No");
    if (config.use_daily_max_profit) {
        os << " (" << config.daily_max_profit_percentage << "%)";
    }
    os << "\n";
    
    os << "  Daily max drawdown: " << (config.use_daily_max_drawdown ? "Yes" : "No");
    if (config.use_daily_max_drawdown) {
        os << " (" << config.daily_max_drawdown_percentage << "%)";
    }
    os << "\n";
    
    os << "\nMACHINE LEARNING\n";
    os << "  ML model enabled: " << (config.use_ml_entry ? "Yes" : "No") << "\n";
    if (config.use_ml_entry) {
        os << "  Model path: " << config.ml_entry_model_path << "\n";
        os << "  Lookback periods: " << config.ml_entry_lookback_periods << "\n";
        os << "  Prediction threshold: ±" << config.ml_entry_threshold << "\n";
        os << "  Normalization: " << (config.ml_entry_normalize ? "Yes" : "No") << "\n";
        if (!config.ml_entry_features.empty()) {
            os << "  Configured features: " << config.ml_entry_features.size() << "\n";
            for (size_t i = 0; i < config.ml_entry_features.size(); ++i) {
                const auto& feature = config.ml_entry_features[i];
                os << "    " << (i + 1) << ". ";
                
                // Display custom name if available
                if (!feature.custom_name.empty()) {
                    os << feature.custom_name;
                } else {
                    os << "Indicator type " << static_cast<int>(feature.type);
                }
                
                // Display transform if not NONE
                if (feature.transform != filter::TransformType::NONE) {
                    os << " [transform=" << static_cast<int>(feature.transform) << "]";
                }
                
                os << "\n";
            }
        } else {
            os << "  Features: default OHLC\n";
        }
    }
    
    os << "════════════════════════════════════════════════════════";
    return os;
}