#pragma once

#include <string>
#include <sstream>
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
        CROSSES_BELOW          // Croisement à la baisse (période actuelle vs précédente)
    };

    // Type de logique temporelle
    enum class TemporalLogic {
        CURRENT,          // Période courante uniquement
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
        {}

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
        
        // Pour propriétés de bougie
        static ValueSource CandleProperty(CandlePropertyType type, int offset = 0) {
            ValueSource source;
            source.category = ValueCategory::CANDLE_PROPERTY;
            source.candlePropertyType = type;
            source.historicalOffset = offset;
            return source;
        }

        // Méthode pour obtenir une description humaine lisible de la source
        std::string getDescription() const {
            std::string desc;
            
            switch (category) {
                case ValueCategory::PRICE:
                    desc = "Prix ";
                    switch (priceType) {
                        case PriceType::CLOSE: desc += "Clôture"; break;
                        case PriceType::OPEN: desc += "Ouverture"; break;
                        case PriceType::HIGH: desc += "Haut"; break;
                        case PriceType::LOW: desc += "Bas"; break;
                        case PriceType::TYPICAL: desc += "Typique"; break;
                        case PriceType::MEDIAN: desc += "Médian"; break;
                    }
                    break;
                    
                case ValueCategory::CONSTANT:
                    desc = std::to_string(constantValue);
                    break;
                    
                case ValueCategory::INDICATOR:
                    switch (indicatorType) {
                        case IndicatorType::EMA: 
                            desc = "EMA(" + std::to_string(emaParams.period) + ")"; 
                            break;
                        case IndicatorType::RSI: 
                            desc = "RSI(" + std::to_string(rsiParams.period) + ")"; 
                            break;
                        case IndicatorType::STOCHASTIC_K: 
                            desc = "Stochastique K(" + std::to_string(stochParams.fastK) + "," + 
                                std::to_string(stochParams.slowK) + "," + 
                                std::to_string(stochParams.slowD) + ")";
                            break;
                        case IndicatorType::STOCHASTIC_D: 
                            desc = "Stochastique D(" + std::to_string(stochParams.fastK) + "," + 
                                std::to_string(stochParams.slowK) + "," + 
                                std::to_string(stochParams.slowD) + ")";
                            break;
                        case IndicatorType::ATR: 
                            desc = std::string(atrParams.useLog ? "ATRLOG(" : "ATR(") + 
                                std::to_string(atrParams.period) + ")";
                            break;
                        case IndicatorType::SUPERTREND_VALUE:
                            desc = "SuperTrend(" + std::to_string(supertrendParams.atrPeriod) + "," +
                                std::to_string(supertrendParams.multiplier) + ")";
                            break;
                        case IndicatorType::SUPERTREND_DIRECTION:
                            desc = "SuperTrend Direction(" + std::to_string(supertrendParams.atrPeriod) + "," +
                                std::to_string(supertrendParams.multiplier) + ")";
                            break;
                        case IndicatorType::PIVOT_POINT:
                            desc = "Pivot Point";
                            break;
                    }
                    break;
                    
                case ValueCategory::CANDLE_PROPERTY:
                    desc = "Bougie ";
                    switch (candlePropertyType) {
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
            
            if (historicalOffset > 0) {
                desc += " [T-" + std::to_string(historicalOffset) + "]";
            }
            
            return desc;
        }
    };

    // Structure pour un filtre complet
    struct GenericFilter {
        ValueSource leftValue;
        ValueSource rightValue;
        ComparisonOperator op;
        TemporalLogic temporalLogic = TemporalLogic::CURRENT;
        int lookbackPeriods = 1;
        bool enabled = true;
        std::string description;
        
        GenericFilter() = default;
        
        // Constructeur pratique pour les cas courants
        GenericFilter(ValueSource left, 
                    ComparisonOperator comp, 
                    ValueSource right,
                    TemporalLogic logic = TemporalLogic::CURRENT,
                    int periods = 1,
                    const std::string& desc = "") 
            : leftValue(left), 
            rightValue(right), 
            op(comp), 
            temporalLogic(logic), 
            lookbackPeriods(periods),
            description(desc) {
            
            // Générer une description automatique si aucune n'est fournie
            if (description.empty()) {
                description = autoGenerateDescription();
            }
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
                case ComparisonOperator::CROSSES_ABOVE: opStr = "croise au-dessus"; break;
                case ComparisonOperator::CROSSES_BELOW: opStr = "croise en-dessous"; break;
            }
            
            std::string timeLogicStr;
            switch (temporalLogic) {
                case TemporalLogic::CURRENT: 
                    timeLogicStr = ""; 
                    break;
                case TemporalLogic::ANY_OF: 
                    timeLogicStr = " (sur au moins 1 des " + std::to_string(lookbackPeriods) + " dernières périodes)"; 
                    break;
                case TemporalLogic::ALL_OF: 
                    timeLogicStr = " (sur toutes les " + std::to_string(lookbackPeriods) + " dernières périodes)"; 
                    break;
            }
            
            return leftValue.getDescription() + " " + opStr + " " + rightValue.getDescription() + timeLogicStr;
        }
    };
}

enum class SignalType {
    BUY,
    SELL,
    MOVE_SL,
    LIQUIDATE
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

    // New parameter for TP based on SuperTrend
    int tp_supertrend_atr_period;
    double tp_supertrend_multiplier;

    // New parameters for TP based on ML/RL
    std::string rl_model_path = "./models/general_tp_model_lookback_150.onnx"; // Path to the ML model
    int rl_lookback_periods; // Number of historical candles to include in features
    double rl_tp_max_multiplier; // Maximum TP distance as multiple of SL distance
    double rl_tp_min_multiplier; // Minimum TP distance as multiple of SL distance

    // New parameter for nth Heikin-Ashi take profit
    int nth_heikin_ashi_count; // Number of opposite Heikin-Ashi candles to wait for

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
        case TakeProfitMethod::SuperTrend:
            return os << "SuperTrend";
        case TakeProfitMethod::RL:
            return os << "RL";
        case TakeProfitMethod::NthHeikinAshi:
            return os << "NthHeikinAshi";
        default:
            return os << "Unknown(" << static_cast<int>(method) << ")";
    }
}

// Overload of the stream operator for StrategyConfig
inline std::ostream& operator<<(std::ostream& os, const StrategyConfig& config) {
    os << "StrategyConfig {\n";

    os << "  Name: " << config.name << "\n";

    // Log level
    os << "  Log level: " << config.logLevel << "\n";
    os << "  Enable logging: " << (config.enable_logging ? "Yes" : "No") << "\n";

    os << "  Go Direction: " << (config.tradeDirection == TradeDirection::NOTSET ? "Not Set" : (config.tradeDirection == TradeDirection::LONG ? "LONG" : "SHORT")) << "\n";
    for (const auto& filter : config.filters)
        os << "  Filter: " << filter.description << "\n";
    
    // Time settings
    os << "  Trading hours: " << config.trading_from.hour << ":" << config.trading_from.minute 
       << " - " << config.trading_to.hour << ":" << config.trading_to.minute << "\n";
    
    static const char* day_names[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    os << "  Trading days: ";
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

    os << "  Stop loss method: " << config.sl_method << "\n";
    os << "  Take profit method: " << config.tp_method << "\n";
    
    // SL/TP values
    os << "  Take profit distance: " << config.take_profit_distance << "\n";
    os << "  Stop loss distance: " << config.stop_loss_distance << "\n";
    
    // ATR parameters
    os << "  ATR period: " << config.atr_period << "\n";
    os << "  SL ATR multiplier: " << config.stop_loss_atr_multiplier << "\n";
    os << "  TP ATR multiplier: " << config.take_profit_atr_multiplier << "\n";
    os << "  Min SL distance: " << config.min_stop_loss_distance << "\n";
    os << "  Min TP distance: " << config.min_take_profit_distance << "\n";
    
    // Min/Max parameters
    os << "  SL Min/Max periods: " << config.sl_minmax_periods << "\n";
    os << "  SL Min/Max delta coef ATR: " << config.sl_minmax_delta_coef_atr << "\n";
    
    // SL ratio for TP
    os << "  TP = SL * ratio: " << config.tp_sl_ratio << "\n";

    // SuperTrend parameters for TP
    os << "  TP SuperTrend ATR period: " << config.tp_supertrend_atr_period << "\n";
    os << "  TP SuperTrend multiplier: " << config.tp_supertrend_multiplier << "\n";

    // Nth Heikin-Ashi parameters for TP
    os << "  Nth Heikin-Ashi count: " << config.nth_heikin_ashi_count << "\n";
    
    // Risk management
    os << "  Use risk-based sizing: " << (config.use_risk_based_sizing ? "Yes" : "No") << "\n";
    os << "  Risk percentage: " << config.risk_percentage << "%\n";
    os << "  Leverage limit: " << config.leverage_limit << "\n";
    os << "  Cash: " << config.cash << "\n";
    
    // Break-even parameters
    os << "  Use break-even: " << (config.use_break_even ? "Yes" : "No") << "\n";
    os << "  Break-even threshold: " << config.break_even_threshold << "\n";
    os << "  Break-even offset percentage: " << config.break_even_offset_per_mille << "%\n";
    
    // Daily maximum loss
    os << "  Use daily max loss: " << (config.use_daily_max_loss ? "Yes" : "No") << "\n";
    os << "  Daily max loss %: " << config.daily_max_loss_percentage << "%\n";
    // os << "  Daily max loss amount: " << config.daily_max_loss_amount << "\n";
    
    // Daily maximum profit
    os << "  Use daily max profit: " << (config.use_daily_max_profit ? "Yes" : "No") << "\n";
    os << "  Daily max profit %: " << config.daily_max_profit_percentage << "%\n";
    // os << "  Daily max profit amount: " << config.daily_max_profit_amount << "\n";
    
    // Daily maximum drawdown
    os << "  Use daily max drawdown: " << (config.use_daily_max_drawdown ? "Yes" : "No") << "\n";
    os << "  Daily max drawdown %: " << config.daily_max_drawdown_percentage << "%\n";
    // os << "  Daily max drawdown amount: " << config.daily_max_drawdown_amount << "\n";
    
    os << "}";
    return os;
}