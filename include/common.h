#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <array>
#include <functional>
#include <optional>


namespace filter {
    // Types de valeurs disponibles pour la comparaison
    enum class ValueCategory {
        PRICE,              // Prix de la bougie
        INDICATOR,          // Indicateur technique
        CONSTANT,           // Valeur constante
        CANDLE_PROPERTY     // Propriété spécifique de bougie
    };

    // Types de prix disponibles
    enum class PriceType {
        CLOSE,
        OPEN,
        HIGH,
        LOW,
        TYPICAL,    // (High + Low + Close) / 3
        MEDIAN      // (High + Low) / 2
    };

    // Types d'indicateurs disponibles
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
        PIVOT_POINT
    };

    // Propriétés de bougies
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

    // Types d'opérateurs de comparaison
    enum class ComparisonOperator {
        GREATER_THAN,          // >
        LESS_THAN,             // <
        GREATER_OR_EQUAL,      // >=
        LESS_OR_EQUAL,         // <=
        EQUAL,                 // ==
        NOT_EQUAL,             // !=
        CROSSES_ABOVE,         // Croisement à la hausse (période actuelle vs précédente)
        CROSSES_BELOW,         // Croisement à la baisse (période actuelle vs précédente)
        TRUE,                  // Racourci pour == Constante 1.0
        FALSE                  // Racourci pour == Constante 0.0
    };

    // Type de logique temporelle
    enum class TemporalLogic {
        ANY_OF,           // Au moins une période (OR)
        ALL_OF,           // Toutes les périodes (AND)
    };


    // TODO : il faut peut etre ajouter une relation d'ordre pour pouvoir les mettre dans une map
    // Paramètres pour EMA
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

    // Paramètres pour RSI
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

    // Paramètres pour Stochastique
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

    // Paramètres pour ATR
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

    // Paramètres pour SuperTrend
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

    // Paramètres pour CCI
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



    // Paramètres pour MACD
    enum class MACDSource {
        CLOSE,
        OPEN,
        HIGH,
        LOW
    };

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

        MACDSource source;

        MAType osc_ma_type;
        MAType signal_ma_type;

        int signal_smoothing;

        MACDParams(int fastPeriod = 12,
                   int slowPeriod = 26,
                   int signalPeriod = 9,
                   MACDSource src = MACDSource::CLOSE,
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

    // Structure unifiée pour une source de valeur
    struct ValueSource {
        ValueCategory category;

        // Sous-types spécifiques à la catégorie
        union {
            PriceType priceType;
            IndicatorType indicatorType;
            CandlePropertyType candlePropertyType;
        };

        // Paramètres spécifiques aux indicateurs
        union {
            EMAParams emaParams;
            RSIParams rsiParams;
            StochasticParams stochParams;
            ATRParams atrParams;
            SuperTrendParams supertrendParams;
            CCIParams cciParams;
            MACDParams macdParams;
        };

        // Valeur constante si la catégorie est CONSTANT
        double constantValue = 0.0;

        // Décalage pour les valeurs historiques
        int historicalOffset = 0;

        // Explicit default constructor to initialize unions safely
        ValueSource()
            : category(ValueCategory::PRICE),
            priceType(PriceType::CLOSE),
            emaParams(0),
            constantValue(0.0),
            historicalOffset(0)
        {
        }

        // Constructeurs spécifiques pour chaque catégorie
        // TODO remplacer les constructeur avec les structure de params
        
        // Pour le prix
        static ValueSource Price(PriceType type, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::PRICE;
            source.priceType = type;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour une constante
        static ValueSource Constant(double value) {
            ValueSource source;
            source.category = ValueCategory::CONSTANT;
            source.constantValue = value;
            return source;
        }
        
        // Pour EMA
        static ValueSource EMA(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::EMA;
            source.emaParams.period = period;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour RSI
        static ValueSource RSI(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::RSI;
            source.rsiParams.period = period;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour Stochastique K
        static ValueSource StochasticK(int fastK, int slowK, int slowD, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::STOCHASTIC_K;
            source.stochParams.fastK = fastK;
            source.stochParams.slowK = slowK;
            source.stochParams.slowD = slowD;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour Stochastique D
        static ValueSource StochasticD(int fastK, int slowK, int slowD, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::STOCHASTIC_D;
            source.stochParams.fastK = fastK;
            source.stochParams.slowK = slowK;
            source.stochParams.slowD = slowD;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour ATR
        static ValueSource ATR(int period, bool useLog = true, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::ATR;
            source.atrParams.period = period;
            source.atrParams.useLog = useLog;
            source.historicalOffset = offset;
            return source;
        }
        
        // Pour SuperTrend (valeur)
        static ValueSource SuperTrend(int atrPeriod, double multiplier, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::SUPERTREND_VALUE;
            source.supertrendParams.atrPeriod = atrPeriod;
            source.supertrendParams.multiplier = multiplier;
            source.historicalOffset = offset;
            return source;
        }
        // Pour CCI (valeur)
        static ValueSource CCI(int period, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::INDICATOR;
            source.indicatorType = IndicatorType::CCI;
            source.cciParams.period = period;
            source.historicalOffset = offset;
            return source;
        }

        
        // Pour propriétés de bougie
        static ValueSource CandleProperty(CandlePropertyType type, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::CANDLE_PROPERTY;
            source.candlePropertyType = type;
            source.historicalOffset = offset;
            return source;
        }

        // Méthode pour obtenir une description humaine lisible de la source
        static std::string description(const ValueSource& source) {
            std::string desc;
            
            switch (source.category) {
                case ValueCategory::PRICE:
                    desc = "Prix ";
                    switch (source.priceType) {
                        case PriceType::CLOSE: desc += "Cloture"; break;
                        case PriceType::OPEN: desc += "Ouverture"; break;
                        case PriceType::HIGH: desc += "Haut"; break;
                        case PriceType::LOW: desc += "Bas"; break;
                        case PriceType::TYPICAL: desc += "Typique"; break;
                        case PriceType::MEDIAN: desc += "Médian"; break;
                    }
                    break;
                    
                case ValueCategory::CONSTANT:
                    desc = std::to_string(source.constantValue);
                    break;
                    
                case ValueCategory::INDICATOR:
                    switch (source.indicatorType) {
                        case IndicatorType::EMA: 
                            desc = "EMA(" + std::to_string(source.emaParams.period) + ")"; 
                            break;
                        case IndicatorType::RSI: 
                            desc = "RSI(" + std::to_string(source.rsiParams.period) + ")"; 
                            break;
                        case IndicatorType::STOCHASTIC_K: 
                            desc = "Stochastique K(" + std::to_string(source.stochParams.fastK) + "," + 
                                std::to_string(source.stochParams.slowK) + "," + 
                                std::to_string(source.stochParams.slowD) + ")";
                            break;
                        case IndicatorType::STOCHASTIC_D: 
                            desc = "Stochastique D(" + std::to_string(source.stochParams.fastK) + "," + 
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
                    }
                    break;
                    
                case ValueCategory::CANDLE_PROPERTY:
                    desc = "Bougie ";
                    switch (source.candlePropertyType) {
                        case CandlePropertyType::HEIKIN_ASHI_IS_GREEN: desc += "Heikin-Ashi Verte"; break;
                        case CandlePropertyType::HEIKIN_ASHI_IS_RED: desc += "Heikin-Ashi Rouge"; break;
                        case CandlePropertyType::IS_GREEN: desc += "Est Verte"; break;
                        case CandlePropertyType::IS_RED: desc += "Est Rouge"; break;
                        case CandlePropertyType::BODY_SIZE: desc += "Taille Corps"; break;
                        case CandlePropertyType::UPPER_SHADOW_SIZE: desc += "Ombre Haute"; break;
                        case CandlePropertyType::LOWER_SHADOW_SIZE: desc += "Ombre Basse"; break;
                        case CandlePropertyType::RANGE: desc += "Étendue"; break;
                    }
                    break;
            }

            if (source.historicalOffset > 0) {
                desc += " [T-" + std::to_string(source.historicalOffset) + "]";
            }
            
            return desc;
        }
    };

    // Structure pour un filtre complet
    struct GenericFilter {
        ValueSource leftValue;
        ValueSource rightValue;
        ComparisonOperator op;
        TemporalLogic temporalLogic = TemporalLogic::ALL_OF;
        int lookbackPeriods = 1;
        bool enabled = true;
        std::string description;

        GenericFilter() = default;
                
        // Constructeur pratique pour les cas courants
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
            
            // Générer une description automatique si aucune n'est fournie
            description = autoGenerateDescription();
        }
        
        // Génère une description lisible du filtre
        std::string autoGenerateDescription() const {
            std::string opStr;
            switch (op) {
                case ComparisonOperator::GREATER_THAN: opStr = ">"; break;
                case ComparisonOperator::LESS_THAN: opStr = "<"; break;
                case ComparisonOperator::GREATER_OR_EQUAL: opStr = ">="; break;
                case ComparisonOperator::LESS_OR_EQUAL: opStr = "<="; break;
                case ComparisonOperator::EQUAL: opStr = "="; break;
                case ComparisonOperator::NOT_EQUAL: opStr = "≠"; break;
                case ComparisonOperator::CROSSES_ABOVE: opStr = "croise à la hausse"; break;
                case ComparisonOperator::CROSSES_BELOW: opStr = "croise à la baisse"; break;
                case ComparisonOperator::TRUE: opStr = "est vrai"; break;
                case ComparisonOperator::FALSE: opStr = "est faux"; break;
            }

            // Décrire la logique temporelle
            std::string timeLogicStr;
            if (temporalLogic == TemporalLogic::ANY_OF) {
                timeLogicStr = " (au moins une période)";
            } else {
                timeLogicStr = " (toutes les périodes)";
            }
            if (lookbackPeriods > 1) {
                timeLogicStr += " sur " + std::to_string(lookbackPeriods) + " périodes";
            }

            // Construire les descriptions gauche/droite
            std::string leftDesc = ValueSource::description(leftValue);
            std::string rightDesc;
            if (op == ComparisonOperator::TRUE) {
                rightDesc = "VRAI";
            } else if (op == ComparisonOperator::FALSE) {
                rightDesc = "FAUX";
            } else {
                rightDesc = ValueSource::description(rightValue);
            }

            return leftDesc + " " + opStr + " " + rightDesc + timeLogicStr;
        }
    };
}

enum class SignalType {
    BUY,
    SELL,
    MOVE_SL,
    LIQUIDATE
};

// TODO mettre des std::optional
struct Signal {
    SignalType type;
    double quantity = 0.0;
    double price = 0.0;
    double take_profit = 0.0;
    double stop_loss = 0.0;
    double new_sl = 0.0;  // For MOVE_SL action
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

// TODO: could be shared with the Date class from backtestEngine
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
    NOTSET,
    LONG,
    SHORT
};

// On pourrait utiliser des union pour separer les paramettre des differents methodes de SL et TP
struct StrategyConfig {
    std::string name;
    TradeDirection tradeDirection = TradeDirection::NOTSET;

    // Logging
    bool enable_logging = true; // Enable or disable logging
    LogLevel logLevel = LogLevel::DEBUG;

    // Filters
    std::vector<filter::GenericFilter> filters;
    // Filters that, when true, should trigger a liquidation (resale) of the open position
    std::vector<filter::GenericFilter> resale_filters;

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
    std::string rl_model_path = "./models/general_tp_model_lookback_150.onnx"; // Path to the ML model
    int rl_lookback_periods; // Number of historical candles to include in features
    double rl_tp_max_multiplier; // Maximum TP distance as multiple of SL distance
    double rl_tp_min_multiplier; // Minimum TP distance as multiple of SL distance

    // Risk management
    bool use_risk_based_sizing;
    double risk_percentage;
    double cash;
    double leverage_limit;

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

// Overload the << operator for StopLossMethod
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
    os << "║            CONFIGURATION DE LA STRATÉGIE             ║\n";
    os << "╚══════════════════════════════════════════════════════╝\n";
    
    os << "GÉNÉRAL\n";
    os << "  Nom: " << config.name << "\n";
    os << "  Direction: " << (config.tradeDirection == TradeDirection::NOTSET ? "Non définie" : (config.tradeDirection == TradeDirection::LONG ? "LONG" : "SHORT")) << "\n";
    os << "  Niveau de log: " << config.logLevel << "\n";
    os << "  Logging activé: " << (config.enable_logging ? "Oui" : "Non") << "\n";
    
    os << "\nFILTRES D'ENTRÉE\n";
    if (config.filters.empty()) {
        os << "  Aucun filtre\n";
    } else {
        for (const auto& filter : config.filters)
            os << "  • " << filter.description << "\n";
    }
    
    os << "\nFILTRES DE SORTIE\n";
    if (config.resale_filters.empty()) {
        os << "  Aucun filtre\n";
    } else {
        for (const auto& filter : config.resale_filters)
            os << "  • " << filter.description << "\n";
    }
    
    os << "\nHEURES DE TRADING\n";
    os << "  Plage horaire: " 
       << std::setfill('0') << std::setw(2) << config.trading_from.hour << ":" 
       << std::setfill('0') << std::setw(2) << config.trading_from.minute 
       << " - " 
       << std::setfill('0') << std::setw(2) << config.trading_to.hour << ":" 
       << std::setfill('0') << std::setw(2) << config.trading_to.minute << "\n";
    
    static const char* day_names[7] = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
    os << "  Jours actifs: ";
    bool first = true;
    for (size_t i = 0; i < 7; ++i) {
        if (config.trading_days_array[i]) {
            if (!first) os << ", ";
            os << day_names[i];
            first = false;
        }
    }
    if (first) os << "Aucun";
    os << "\n";
    
    os << "\nSTOP LOSS & TAKE PROFIT\n";
    os << "  Méthode SL: " << config.sl_method << "\n";
    os << "  Méthode TP: " << config.tp_method << "\n";
    
    if (config.sl_method == StopLossMethod::Fixed) {
        os << "  Distance SL fixe: " << config.stop_loss_distance << "\n";
    } else if (config.sl_method == StopLossMethod::ATR) {
        os << "  Période ATR: " << config.atr_period << "\n";
        os << "  Multiplicateur ATR (SL): " << config.stop_loss_atr_multiplier << "\n";
        os << "  Distance SL minimale: " << config.min_stop_loss_distance << "\n";
    } else if (config.sl_method == StopLossMethod::MinMax) {
        os << "  Périodes Min/Max: " << config.sl_minmax_periods << "\n";
        os << "  Coefficient delta ATR: " << config.sl_minmax_delta_coef_atr << "\n";
    }
    
    if (config.tp_method == TakeProfitMethod::Fixed) {
        os << "  Distance TP fixe: " << config.take_profit_distance << "\n";
    } else if (config.tp_method == TakeProfitMethod::ATR) {
        os << "  Multiplicateur ATR (TP): " << config.take_profit_atr_multiplier << "\n";
        os << "  Distance TP minimale: " << config.min_take_profit_distance << "\n";
    } else if (config.tp_method == TakeProfitMethod::SLRatio) {
        os << "  Ratio TP/SL: " << config.tp_sl_ratio << "\n";
    }
    
    os << "\nGESTION DU RISQUE\n";
    os << "  Sizing basé sur le risque: " << (config.use_risk_based_sizing ? "Oui" : "Non") << "\n";
    if (config.use_risk_based_sizing) {
        os << "  Risque par trade: " << config.risk_percentage << "%\n";
    }
    os << "  Capital: " << config.cash << "\n";
    os << "  Levier maximum: " << config.leverage_limit << "\n";
    
    os << "\nBREAK-EVEN\n";
    os << "  Activé: " << (config.use_break_even ? "Oui" : "Non") << "\n";
    if (config.use_break_even) {
        os << "  Seuil: " << (config.break_even_threshold * 100) << "% du TP\n";
        os << "  Offset: " << config.break_even_offset_per_mille << "‰\n";
    }
    
    os << "\nLIMITES JOURNALIÈRES\n";
    os << "  Perte max quotidienne: " << (config.use_daily_max_loss ? "Oui" : "Non");
    if (config.use_daily_max_loss) {
        os << " (" << config.daily_max_loss_percentage << "%)";
    }
    os << "\n";
    
    os << "  Profit max quotidien: " << (config.use_daily_max_profit ? "Oui" : "Non");
    if (config.use_daily_max_profit) {
        os << " (" << config.daily_max_profit_percentage << "%)";
    }
    os << "\n";
    
    os << "  Drawdown max quotidien: " << (config.use_daily_max_drawdown ? "Oui" : "Non");
    if (config.use_daily_max_drawdown) {
        os << " (" << config.daily_max_drawdown_percentage << "%)";
    }
    os << "\n";
    
    os << "════════════════════════════════════════════════════════";
    return os;
}