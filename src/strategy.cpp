#include "strategy.h"

// Utilitary function to parse ISO 8601 date strings
DateTime parse_iso_datetime(const std::string& iso_date) {
    DateTime result;
    
    // Minimal validation for ISO 8601 format
    if (iso_date.size() < 19) {
        return result;  // Return invalid date
    }
    
    std::tm tm = {};
    std::istringstream ss(iso_date.substr(0, 19));
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (ss.fail()) {
        return result;  // Return invalid date
    }
    
    result.year = tm.tm_year + 1900;  // tm_year is years since 1900
    result.month = tm.tm_mon + 1;     // tm_mon is 0-11
    result.day = tm.tm_mday;
    result.time.hour = tm.tm_hour;
    result.time.minute = tm.tm_min;
    result.time.second = tm.tm_sec;
    
    return result;
}

// Function to get the day of the week (0=Monday, 6=Sunday)
int get_day_of_week(const DateTime& date) {
    // Formula to calculate the day of the week
    std::tm timeinfo = {};
    timeinfo.tm_year = date.year - 1900;
    timeinfo.tm_mon = date.month - 1;
    timeinfo.tm_mday = date.day;
    
    std::time_t time = std::mktime(&timeinfo);
    std::tm* local_tm = std::localtime(&time);
    int weekday = local_tm->tm_wday;
    // Convert from Sunday=0 to Sunday=6
    return (weekday == 0) ? 6 : weekday - 1;
}

void Strategy::registerFiltersIndicators() {
    // Fonction helper pour enregistrer un indicateur une seule fois
    auto registerIfNeeded = [this](const filter::ValueSource& source) {
        if (source.category != filter::ValueCategory::INDICATOR) {
            return;
        }
        
        switch (source.indicatorType) {
            case filter::IndicatorType::EMA:
                indicator_manager->registerEMA(source.emaParams);
                break;
            case filter::IndicatorType::RSI:
                indicator_manager->registerRSI(source.rsiParams);
                break;
            case filter::IndicatorType::ATR:
                indicator_manager->registerATR(source.atrParams);
                break;
            case filter::IndicatorType::STOCHASTIC_K:
            case filter::IndicatorType::STOCHASTIC_D:
                indicator_manager->registerStochastic(source.stochParams);
                break;
            case filter::IndicatorType::SUPERTREND_VALUE:
            case filter::IndicatorType::SUPERTREND_DIRECTION:
                indicator_manager->registerSuperTrend(source.supertrendParams);
                break;
            case filter::IndicatorType::CCI:
                indicator_manager->registerCCI(source.cciParams);
                break;
            case filter::IndicatorType::MACD_HISTOGRAM:
            case filter::IndicatorType::MACD_LINE:
            case filter::IndicatorType::MACD_SIGNAL:
                indicator_manager->registerMACD(source.macdParams);
                break;
            case filter::IndicatorType::BB_UPPER:
            case filter::IndicatorType::BB_LOWER:
            case filter::IndicatorType::BB_PERCENT_B:
                indicator_manager->registerBB(source.bbParams);
                break;
            default:
                break;
        }
    };

    // Parcourir tous les filtres et enregistrer directement les indicateurs nécessaires
    for (const auto& filter : buyFilters) {
        registerIfNeeded(filter.leftValue);
        registerIfNeeded(filter.rightValue);
    }
    for (const auto& filter : sellFilters) {
        registerIfNeeded(filter.leftValue);
        registerIfNeeded(filter.rightValue);
    }

    for (const auto& filter : resaleFilters) {
        registerIfNeeded(filter.leftValue);
        registerIfNeeded(filter.rightValue);
    }
    for (const auto& filter : rebuyFilters) {
        registerIfNeeded(filter.leftValue);
        registerIfNeeded(filter.rightValue);
    }
}

Signal Strategy::go(TradeDirection direction) {
    STRATEGY_LOG(logger, log_general, "Préparation d'un signal d'entrée", LogLevel::INFO);

    // Calculate Stop Loss
    double stop_loss = PositionManager::calculateStopLoss(
        base_config, 
        price(), 
        indicator_manager->getATRValue(filter::ATRParams(base_config.atr_period, true)), 
        direction,
        *candle_manager, 
        logger.get()
    );

    // Calculate Take Profit
    double take_profit = PositionManager::calculateTakeProfit(
        base_config,
        price(),
        indicator_manager->getATRValue(filter::ATRParams(base_config.atr_period, true)),
        stop_loss,
        *candle_manager,
        logger.get()
    );

    // Calculate position size
    double quantity = PositionManager::calculatePositionSize(
        base_config,
        price(),
        stop_loss,
        logger.get()
    );

    Signal sig;
    sig.type = direction == TradeDirection::LONG ? SignalType::BUY : SignalType::SELL;
    sig.quantity = quantity;
    sig.price = price();
    sig.take_profit = take_profit;
    sig.stop_loss = stop_loss;
    
    return sig;
}

bool Strategy::update_indicators()
{
    if (candle_manager->size() == 0) {
        STRATEGY_LOG(logger, log_general, "candle_manager vide, impossible de mettre à jour les indicateurs", LogLevel::WARNING);
        return false;
    }
    
    if (indicator_manager->needsInitialization()) {
        // Obtenir automatiquement la période maximale nécessaire
        int max_period = indicator_manager->getMaxRequiredPeriods();
        
        // Prendre en compte également la période pour le Stop Loss si nécessaire
        if (base_config.sl_method == StopLossMethod::MinMax) 
            max_period = std::max(max_period, base_config.sl_minmax_periods);
        
        size_t available_candles = candle_manager->size();
        
        if (available_candles < static_cast<size_t>(max_period)) {
            int remaining = max_period - static_cast<int>(available_candles);
            STRATEGY_LOG(logger, log_general, "Historique insuffisant: " + logger->fast_int_to_string(static_cast<int>(available_candles)) + 
                              "/" + logger->fast_int_to_string(max_period) + " bougies (manque " + 
                              logger->fast_int_to_string(remaining) + " bougies)");
            return false;
        }
        
        auto candles = candle_manager->get_last_candles(candle_manager->size());
        STRATEGY_LOG(logger, log_general, "Initialisation avec " + logger->fast_int_to_string(static_cast<int>(candles.size())) + " bougies");
        return indicator_manager->initializeAll(candles, logger.get());
    }
    
    // Mise à jour simple avec la dernière bougie
    return indicator_manager->updateAll(candle_manager->get_latest_candle(), logger.get());
}

// Implementation of Strategy class methods
// Calculate the potential risk of a trade in monetary value
double Strategy::calculate_trade_risk(const Signal& signal) {
    double position_value = signal.quantity * signal.price;
    double risk_value = position_value * (signal.stop_loss / signal.price);
    return risk_value;
}

// Check is if the trade risk is acceptable based on daily max loss
bool Strategy::is_trade_risk_acceptable(double risk) {
    if (!base_config.use_daily_max_loss) {
        return true;  // If the limit is not enabled, all trades are acceptable
    }

    // Calculate the daily loss limit
    double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;

    // Check if the trade would exceed the limit
    // daily_pnl is the cumulative PnL so far, risk is the maximum amount we could lose
    return (daily_pnl - risk) >= -max_loss_amount;
}

// Check if daily max profit has been reached
bool Strategy::is_daily_max_profit_reached() {
    if (!base_config.use_daily_max_profit) {
        return false;  // If the limit is not enabled, profit limit never reached
    }

    // Calculate the daily profit limit
    double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;

    // Check if the daily profit has reached the limit
    return daily_pnl >= max_profit_amount;
}

// Check if daily max drawdown has been reached
bool Strategy::is_daily_drawdown_reached() {
    if (!base_config.use_daily_max_drawdown) 
        return false;  // If the limit is not enabled, drawdown limit never reached
    
    // Calculate the daily drawdown limit
    double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;

    // Calculate current drawdown: difference between max PnL and current PnL
    double current_drawdown = daily_max_pnl - daily_pnl;

    // Check if the current drawdown has reached the limit
    return current_drawdown >= max_drawdown_amount;
}

bool Strategy::is_new_trading_day() {
    if (!candle_manager->get_latest_candle().date.is_valid() || !current_trading_day.is_valid()) {
        return true;
    }
    
    return (candle_manager->get_latest_candle().date.year != current_trading_day.year ||
            candle_manager->get_latest_candle().date.month != current_trading_day.month ||
            candle_manager->get_latest_candle().date.day != current_trading_day.day);
}
    
void Strategy::update_daily_pnl_tracking() {
    if (!base_config.use_daily_max_loss && !base_config.use_daily_max_profit && !base_config.use_daily_max_drawdown) 
        return;
    

    // If it's a new day, reset the counter and reactivate trading
    if (is_new_trading_day()) {
        current_trading_day = candle_manager->get_latest_candle().date;
        daily_pnl = 0.0;
        daily_max_pnl = 0.0;  // Reset daily max PnL for new day

        if (base_config.use_daily_max_loss) {
            // Calculate the maximum allowed loss amount for this day
            double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
            
            STRATEGY_LOG(logger, log_general, "Nouveau jour de trading: " + current_trading_day.to_string() + 
                              " - Perte max autorisée: " + logger->fast_double_to_string(max_loss_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_loss_percentage) + "%)", LogLevel::INFO);
        }

        if (base_config.use_daily_max_profit) {
            // Calculate the maximum allowed profit amount for this day
            double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
            
            STRATEGY_LOG(logger, log_general, "Nouveau jour de trading: " + current_trading_day.to_string() + 
                              " - Profit max autorisé: " + logger->fast_double_to_string(max_profit_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        }

        if (base_config.use_daily_max_drawdown) {
            // Calculate the maximum allowed drawdown amount for this day
            double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;
            
            STRATEGY_LOG(logger, log_general, "Nouveau jour de trading: " + current_trading_day.to_string() + 
                              " - Drawdown max autorisé: " + logger->fast_double_to_string(max_drawdown_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_drawdown_percentage) + "%)", LogLevel::INFO);
        }
    }
    
    if (last_trade_pnl != 0.0) {
        daily_pnl += last_trade_pnl;
        
        // Update daily maximum PnL if current PnL is higher
        if (daily_pnl > daily_max_pnl) 
            daily_max_pnl = daily_pnl;
        
        
        STRATEGY_LOG(logger, log_general, "P&L du trade: " + logger->fast_double_to_string(last_trade_pnl) + 
                          " - P&L journalier cumulé: " + logger->fast_double_to_string(daily_pnl) + 
                          " - P&L max journalier: " + logger->fast_double_to_string(daily_max_pnl), LogLevel::INFO);

        last_trade_pnl = 0.0;
    }
}

// Method to check if we are within trading hours
bool Strategy::check_time() {
    const DateTime& current_date = candle_manager->get_latest_candle().date;
    const Time& current_time = current_date.time;

    // Update the current trading day if it's a new day
    if (current_date.day != last_check_date.day ||
        current_date.month != last_check_date.month ||
        current_date.year != last_check_date.year) {

        last_check_date = current_date;

        weekday = get_day_of_week(current_date);
    }

    // Determine if the allowed trading window spans midnight (e.g. 22:00 -> 06:00)
    bool spans_midnight = !(base_config.trading_from < base_config.trading_to);

    bool after_start = false;
    bool before_end = false;

    if (!spans_midnight) {
        // Normal window within the same day: trading_from < current_time < trading_to
        after_start = (base_config.trading_from < current_time);
        before_end = (current_time < base_config.trading_to);
        time_check = after_start && before_end;
    } else {
        // Window spans midnight: allowed if current_time >= trading_from OR current_time < trading_to
    // Use the inverse of operator< because operator>= may not be defined for Time
    after_start = !(current_time < base_config.trading_from);
        before_end = (current_time < base_config.trading_to);
        time_check = after_start || before_end;
    }

    // When window spans midnight and we're before "trading_to" (i.e. after midnight),
    // the active trading day is the previous calendar day. Use effective_weekday to
    // check trading_days_array accordingly.
    int effective_weekday = weekday;
    if (spans_midnight && before_end) {
        // previous day
        effective_weekday = (weekday + 6) % 7;
    }

    // Decide which weekday to use for permission check:
    // - If the window spans midnight and we are after midnight (before_end==true),
    //   use the calendar weekday (`weekday`) so that a disabled calendar day
    //   (e.g. Saturday) stops trading at midnight.
    // - Otherwise use the effective_weekday (previous day when applicable).
    if (spans_midnight && before_end) {
        weekday_check = base_config.trading_days_array[weekday];
        if (!weekday_check) {
            STRATEGY_LOG(logger, log_time_check, false, weekday, false, current_time, LogLevel::INFO);
            return false;
        }
    } else {
        weekday_check = base_config.trading_days_array[effective_weekday];
        if (!weekday_check) {
            STRATEGY_LOG(logger, log_time_check, false, effective_weekday, false, current_time, LogLevel::INFO);
            return false;
        }
    }

    if (!time_check)
        STRATEGY_LOG(logger, log_time_check, true, 0, false, current_time, LogLevel::INFO);
    else
        STRATEGY_LOG(logger, log_time_check, true, 0, true, current_time, LogLevel::DEBUG);

    return time_check;
}

std::optional<Signal> Strategy::check_break_even() {
    if (!base_config.use_break_even || position_info.entry_price <= 0.0 || position_info.take_profit_price <= 0.0) {
        // Insufficient data to calculate break-even
        return std::nullopt;
    }

    // Get the latest candle
    const BasicCandle& latest_candle = candle_manager->get_latest_candle();
    if (!latest_candle.date.is_valid()) {
        return std::nullopt;  // No valid candle
    }

    // Calculate the threshold price for break-even
    double entry_to_tp_distance = position_info.take_profit_price - position_info.entry_price;
    double break_even_price = position_info.entry_price + (entry_to_tp_distance * base_config.break_even_threshold);

    double position_sign = (position_info.take_profit_price - position_info.entry_price) > 0 ? 1.0 : -1.0;
    double reference_price = position_sign > 0 ? latest_candle.high : latest_candle.low;


    // For a long: check if high >= threshold
    // For a short: check if low <= threshold
    bool threshold_reached = position_sign > 0 ? 
                             reference_price >= break_even_price : 
                             reference_price <= break_even_price;


    if (threshold_reached) {
        STRATEGY_LOG(logger, log_general_BE_activated, position_sign, reference_price, break_even_price, base_config.break_even_threshold);
            
        Signal be_signal;
        be_signal.type = SignalType::MOVE_SL;
        be_signal.new_sl = position_info.entry_price + position_info.entry_price * (base_config.break_even_offset_per_mille / 1000.0);
        be_signal.price = break_even_price;
        return be_signal;
    }

    return std::nullopt;
}

Signal Strategy::generate_liquidation_signal() {
    Signal sig;
    sig.type = SignalType::LIQUIDATE;
    return sig;
}

std::optional<Signal> Strategy::execute_long() {
    Signal signal = go(TradeDirection::LONG);
    
    if (signal.quantity < 0.0 || signal.price <= 0.0) {
        STRATEGY_LOG(logger, log_general, "Paramètres d'achat incorrects", LogLevel::ERROR);
        throw std::runtime_error("Buy parameters not properly set");
    }

    if (signal.quantity == 0.0) {
        STRATEGY_LOG(logger, log_general, "Quantité d'achat nulle, trade annulé", LogLevel::WARNING);
        return std::nullopt;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(signal);
    STRATEGY_LOG(logger, log_risk_calculation, risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        STRATEGY_LOG(logger, log_general, "Trade LONG rejeté: risque excessif", LogLevel::WARNING);
        STRATEGY_LOG(logger, log_filter_result, "Limite de risque", false, "Risque calculé: " + logger->fast_double_to_string(risk) + 
                              ", PnL journalier: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Limite max: " + logger->fast_double_to_string(-max_loss_amount));
        return std::nullopt;
    }
      
    STRATEGY_LOG(logger, log_signal, signal);
    return signal;
}

std::optional<Signal> Strategy::execute_short() {
    Signal signal = go(TradeDirection::SHORT);
    
    if (signal.quantity < 0.0 || signal.price <= 0.0) {
        STRATEGY_LOG(logger, log_general, "Paramètres de vente incorrects", LogLevel::ERROR);
        throw std::runtime_error("Sell parameters not properly set");
    }

    if (signal.quantity == 0.0) {
        STRATEGY_LOG(logger, log_general, "Quantité de vente nulle, trade annulé", LogLevel::WARNING);
        return std::nullopt;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(signal);
    STRATEGY_LOG(logger, log_risk_calculation, risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        STRATEGY_LOG(logger, log_general, "Trade SHORT rejeté: risque excessif", LogLevel::WARNING);
        STRATEGY_LOG(logger, log_filter_result, "Limite de risque", false, "Risque calculé: " + logger->fast_double_to_string(risk) + 
                              ", PnL journalier: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Limite max: " + logger->fast_double_to_string(-max_loss_amount));
        return std::nullopt;
    }
    
    STRATEGY_LOG(logger, log_signal, signal);
    return signal;
}

bool Strategy::executeFilters(std::vector<filter::GenericFilter>& filters) {
    // Si empty soit toujours false soit toujours true
    if (filters.empty()) 
        return false; // No filters defined, never pass

    // Si tous les filtres sont desactivés, toujours false
    bool all_disabled = true;
    for (const auto& filter : filters) {
        if (filter.enabled) {
            all_disabled = false;
            break;
        }
    }
    if (all_disabled)
        return false;

    for (const auto& filter : filters) 
        if (!FilterEvaluator::evaluate(filter, *candle_manager, *indicator_manager, logger.get())) 
            return false;
    return true;
}

Signal Strategy::execute() {
    // Update daily PnL tracking
    update_daily_pnl_tracking();

    bool indicators_ready = update_indicators();
    before();

    // Update indicators
    if (!indicators_ready) {
        STRATEGY_LOG(logger, log_execution_step, "Indicateurs pas encore prêts", false);
        return Signal();  // Return empty signal
    }
    
    
    // Check if daily max profit has been reached
    if (is_daily_max_profit_reached()) {
        STRATEGY_LOG(logger, log_execution_step, "Profit max journalier atteint", false);
        double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
        STRATEGY_LOG(logger, log_general, "Profit maximum journalier atteint: " + 
                          logger->fast_double_to_string(daily_pnl) + 
                          " >= " + logger->fast_double_to_string(max_profit_amount) + 
                          " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }

    // Check if daily max drawdown has been reached
    if (is_daily_drawdown_reached()) {
        STRATEGY_LOG(logger, log_execution_step, "Drawdown max journalier atteint", false);
        double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;
        double current_drawdown = daily_max_pnl - daily_pnl;
        STRATEGY_LOG(logger, log_general, "Drawdown maximum journalier atteint: " + 
                          logger->fast_double_to_string(current_drawdown) + 
                          " >= " + logger->fast_double_to_string(max_drawdown_amount) + 
                          " (" + logger->fast_double_to_string(base_config.daily_max_drawdown_percentage) + "%)" +
                          " - PnL max: " + logger->fast_double_to_string(daily_max_pnl) +
                          " - PnL actuel: " + logger->fast_double_to_string(daily_pnl), LogLevel::INFO);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }

    // Time check
    if (!check_time()) {
        STRATEGY_LOG(logger, log_execution_step, "Vérification horaires", false);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }
    STRATEGY_LOG(logger, log_execution_step, "Vérification horaires", true);


    // Check for break-even signal before executing strategy
    auto be_signal = check_break_even();
    if (be_signal) {
        STRATEGY_LOG(logger, log_general, "Signal de break-even généré: " + logger->fast_double_to_string(be_signal->new_sl));
        return *be_signal;
    }

    if (position_info.entry_price > 0.0) {
        // TODO il faut différencier les signaux de liquidation normaux et les signaux de rachat/revente
        if (executeFilters(resaleFilters)) {
            STRATEGY_LOG(logger, log_execution_step, "Filtres de revente passés - Génération du signal de liquidation", true);
            Signal signal = generate_liquidation_signal();
            STRATEGY_LOG(logger, log_signal, signal);
            return signal;
        }

        if (executeFilters(rebuyFilters)) {
            STRATEGY_LOG(logger, log_execution_step, "Filtres de rachat passés - Génération du signal de liquidation", true);
            Signal signal = generate_liquidation_signal();
            STRATEGY_LOG(logger, log_signal, signal);
            return signal;
        }

        STRATEGY_LOG(logger, log_execution_step, "Position ouverte - Pas de nouveau signal généré", false);
        return Signal();  // Return empty signal
    }

    if (executeFilters(buyFilters)) {
        STRATEGY_LOG(logger, log_execution_step, "Filtres d'achat passés", true);
        auto signal = execute_long();
        return signal.value_or(Signal());
    } else if (executeFilters(sellFilters)) {
        STRATEGY_LOG(logger, log_execution_step, "Filtres de vente passés", true);
        auto signal = execute_short();
        return signal.value_or(Signal());
    }

    return Signal();  // Return empty signal
}

Strategy::Strategy(const StrategyConfig& config, std::function<void(const std::string&)> log_callback) 
    : base_config(config), 
    logger(std::make_unique<NullLogger>()),
    indicator_manager(std::make_unique<IndicatorManager>()),
    candle_manager(std::make_unique<CandleManager>()),
    buyFilters(config.buyFilters),
    sellFilters(config.sellFilters),
    resaleFilters(config.resaleFilters),
    rebuyFilters(config.rebuyFilters)
{
    if (base_config.enable_logging) {
        logger = std::make_unique<LoggerManager>(log_callback);
        logger->set_verbosity(static_cast<int>(base_config.logLevel));
    }

    registerFiltersIndicators();

    if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR || base_config.sl_method == StopLossMethod::MinMax) 
        indicator_manager->registerATR(filter::ATRParams(base_config.atr_period, true));
    
    // Ajuster les paramètres du CandleManager en fonction de la période maximale requise
    int max_period = indicator_manager->getMaxRequiredPeriods();
    if (base_config.sl_method == StopLossMethod::MinMax) 
        max_period = std::max(max_period, base_config.sl_minmax_periods);
    
    // Configurer le CandleManager avec la taille minimale requise
    // La méthode setMinimalBufferSize gère automatiquement les paramètres internes
    // pour garantir un fonctionnement optimal et sûr
    candle_manager->setMinimalBufferSize(max_period);
    
    STRATEGY_LOG(logger, log_general, "CandleManager configuré avec taille minimale: " + 
                       std::to_string(max_period) + " bougies", LogLevel::INFO);

    // Générer la configuration complète
    std::ostringstream config_stream;
    config_stream << base_config;
    std::string config_str = config_stream.str();

    STRATEGY_LOG(logger, log_general, config_str, LogLevel::INFO);
    STRATEGY_LOG_VOID(logger, finalize_and_send_logs);  // Forcer l'envoi immédiat
}

// Main update method
Signal Strategy::update_candle(const Candle& candle) {    
    STRATEGY_LOG_VOID(logger, start_chrono);

    position_info = candle.position;

    // Update logger with the current candle
    STRATEGY_LOG(logger, set_current_candle, candle);
    STRATEGY_LOG_VOID(logger, clear);  // Clear previous logs

    STRATEGY_LOG(logger, log_general, "OHLC: " + 
        logger->fast_double_to_string(candle.ohlc.open) + "/" + 
        logger->fast_double_to_string(candle.ohlc.high) + "/" + 
        logger->fast_double_to_string(candle.ohlc.low) + "/" + 
        logger->fast_double_to_string(candle.ohlc.close));

    // Store the last trade P&L if provided in candle
    if (position_info.closed_trade_pnl != 0.0) {
        last_trade_pnl = position_info.closed_trade_pnl;
        STRATEGY_LOG(logger, log_general, "PnL du trade fermé: " + logger->fast_double_to_string(last_trade_pnl));
    }

    // Add to buffer for historical calculations
    candle_manager->add_candle(candle.ohlc);
    
    // Execute strategy and get signal
    Signal signal = execute();
    after();

    STRATEGY_LOG_VOID(logger, finalize_and_send_logs);
    
    return signal;
}

// Properties
double Strategy::price() const {
    return candle_manager->get_latest_candle().close;
}

