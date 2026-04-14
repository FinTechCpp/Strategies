#include "strategy.h"

// Utility function to parse ISO 8601 date strings
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
    std::tm local_tm = {};
#ifdef _WIN32
    localtime_s(&local_tm, &time);  // Windows secure version
#else
    localtime_r(&time, &local_tm);  // POSIX secure version
#endif
    int weekday = local_tm.tm_wday;
    // Convert from Sunday=0 to Sunday=6
    return (weekday == 0) ? 6 : weekday - 1;
}

void Strategy::registerFiltersIndicators() {
    // Helper function to register an indicator only once
    auto registerIfNeeded = [this](const filter::ValueSource& source) {
        if (source.category != filter::ValueCategory::INDICATOR) 
            return;
        
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

    // Iterate all filters and register required indicators directly
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
    STRATEGY_LOG(logger, log_general, "Preparing an entry signal", LogLevel::INFO);

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
        STRATEGY_LOG(logger, log_general, "candle_manager empty, cannot update indicators", LogLevel::WARNING);
        return false;
    }
    
    if (indicator_manager->needsInitialization()) {
        // Automatically obtain the maximum required period
        int max_period = indicator_manager->getMaxRequiredPeriods();
        
        // Also consider the period for Stop Loss if needed
        if (base_config.sl_method == StopLossMethod::MinMax) 
            max_period = std::max(max_period, base_config.sl_minmax_periods);

        // Use the dynamic history value requested by the Lua script (default 200)
        if (base_config.use_lua_script && lua_script_engine) {
            max_period = std::max(max_period, lua_script_engine->get_required_history());
        }
        
        size_t available_candles;
        std::vector<BasicCandle> candles;
        
        // If reset on new day is enabled and we've reset today, only use candles from today
        if (base_config.reset_indicators_on_new_day && last_indicator_reset_day.is_valid()) {
            // Count candles from the start of the current day
            const DateTime& current_date = candle_manager->get_latest_candle().date;
            size_t candles_today = 0;
            
            // Get all candles and count from today
            auto all_candles = candle_manager->get_last_candles(candle_manager->size());
            for (auto it = all_candles.rbegin(); it != all_candles.rend(); ++it) {
                if (it->date.year == current_date.year &&
                    it->date.month == current_date.month &&
                    it->date.day == current_date.day) {
                    candles_today++;
                } else {
                    break; // Stop when we reach a different day
                }
            }
            
            available_candles = candles_today;
            
            if (available_candles < static_cast<size_t>(max_period)) {
                // Not enough candles yet today - no indicator values
                STRATEGY_LOG(logger, log_general, "Insufficient history for today: " + 
                              logger->fast_int_to_string(static_cast<int>(available_candles)) + 
                              "/" + logger->fast_int_to_string(max_period) + " candles");
                return false;
            }
            
            candles = candle_manager->get_last_candles(available_candles);
        } else {
            // Normal mode: use all available history
            available_candles = candle_manager->size();
            
            if (available_candles < static_cast<size_t>(max_period)) {
                int remaining = max_period - static_cast<int>(available_candles);
                STRATEGY_LOG(logger, log_general, "Insufficient history: " + logger->fast_int_to_string(static_cast<int>(available_candles)) + 
                              "/" + logger->fast_int_to_string(max_period) + " candles (missing " + 
                              logger->fast_int_to_string(remaining) + " candles)");
                return false;
            }
            
            candles = candle_manager->get_last_candles(candle_manager->size());
        }
        
        STRATEGY_LOG(logger, log_general, "Initialization with " + logger->fast_int_to_string(static_cast<int>(candles.size())) + " candles");
        return indicator_manager->initializeAll(candles, logger.get());
    }
    
    // Simple update with the latest candle
    return indicator_manager->updateAll(candle_manager->get_latest_candle(), logger.get());
}

// Implementation of Strategy class methods
// Calculate the potential risk of a trade in monetary value
double Strategy::calculate_trade_risk(const Signal& signal) {
    double position_value = signal.quantity * signal.price;
    double risk_value = position_value * (signal.stop_loss / signal.price);
    return risk_value;
}

// Check if the trade risk is acceptable based on daily max loss
bool Strategy::is_trade_risk_acceptable(double risk) {
    if (!base_config.use_daily_max_loss) {
        return true;  // If the limit is not enabled, all trades are acceptable
    }

    // Safety check: ensure percentage is valid (not uninitialized garbage)
    if (std::isnan(base_config.daily_max_loss_percentage) || 
        std::isinf(base_config.daily_max_loss_percentage) ||
        base_config.daily_max_loss_percentage < 0.0) {
        return true;  // Invalid value, treat as disabled (all trades acceptable)
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

    // Safety check: ensure percentage is valid (not uninitialized garbage)
    if (std::isnan(base_config.daily_max_profit_percentage) || 
        std::isinf(base_config.daily_max_profit_percentage) ||
        base_config.daily_max_profit_percentage < 0.0) {
        return false;  // Invalid value, treat as disabled
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
    
    // Safety check: ensure percentage is valid (not uninitialized garbage)
    if (std::isnan(base_config.daily_max_drawdown_percentage) || 
        std::isinf(base_config.daily_max_drawdown_percentage) ||
        base_config.daily_max_drawdown_percentage < 0.0) {
        return false;  // Invalid value, treat as disabled
    }
    
    // Calculate the daily drawdown limit
    double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;

    // Calculate current drawdown: difference between max PnL and current PnL
    double current_drawdown = daily_max_pnl - daily_pnl;

    // Check if the drawdown has reached the limit
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
            
            STRATEGY_LOG(logger, log_general, "New trading day: " + current_trading_day.to_string() + 
                              " - Max allowed loss: " + logger->fast_double_to_string(max_loss_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_loss_percentage) + "%)", LogLevel::INFO);
        }

        if (base_config.use_daily_max_profit) {
            // Calculate the maximum allowed profit amount for this day
            double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
            
            STRATEGY_LOG(logger, log_general, "New trading day: " + current_trading_day.to_string() + 
                              " - Max allowed profit: " + logger->fast_double_to_string(max_profit_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        }

        if (base_config.use_daily_max_drawdown) {
            // Calculate the maximum allowed drawdown amount for this day
            double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;
            
            STRATEGY_LOG(logger, log_general, "New trading day: " + current_trading_day.to_string() + 
                              " - Max allowed drawdown: " + logger->fast_double_to_string(max_drawdown_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_drawdown_percentage) + "%)", LogLevel::INFO);
        }
    }
    
    if (last_trade_pnl != 0.0) {
        daily_pnl += last_trade_pnl;
        
        // Update daily maximum PnL if current PnL is higher
        if (daily_pnl > daily_max_pnl) 
            daily_max_pnl = daily_pnl;
        
        
        STRATEGY_LOG(logger, log_general, "Trade P&L: " + logger->fast_double_to_string(last_trade_pnl) + 
                          " - Accumulated daily P&L: " + logger->fast_double_to_string(daily_pnl) + 
                          " - Daily max P&L: " + logger->fast_double_to_string(daily_max_pnl), LogLevel::INFO);

        last_trade_pnl = 0.0;
    }
}

// Method to check and reset indicators at the start of a new trading day
void Strategy::check_and_reset_indicators_for_new_day() {
    if (!base_config.reset_indicators_on_new_day) {
        return;  // Feature disabled
    }

    const DateTime& current_date = candle_manager->get_latest_candle().date;
    
    // Check if we're on a new day compared to last indicator reset
    bool is_new_day = !last_indicator_reset_day.is_valid() ||
                      current_date.year != last_indicator_reset_day.year ||
                      current_date.month != last_indicator_reset_day.month ||
                      current_date.day != last_indicator_reset_day.day;
    
    if (is_new_day) {
        STRATEGY_LOG(logger, log_general, "New trading day detected: " + current_date.to_string() + 
                          " - Resetting all indicators", LogLevel::INFO);
        
        // Reset all indicators
        indicator_manager->resetAll(logger.get());
        
        // Update the last reset day
        last_indicator_reset_day = current_date;
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
        STRATEGY_LOG(logger, log_general, "Buy parameters incorrect", LogLevel::ERROR);
        throw std::runtime_error("Buy parameters not properly set");
    }

    if (signal.quantity == 0.0) {
        STRATEGY_LOG(logger, log_general, "Buy quantity zero, trade cancelled", LogLevel::WARNING);
        return std::nullopt;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(signal);
    STRATEGY_LOG(logger, log_risk_calculation, risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        STRATEGY_LOG(logger, log_general, "LONG trade rejected: excessive risk", LogLevel::WARNING);
        STRATEGY_LOG(logger, log_filter_result, "Risk limit", false, "Calculated risk: " + logger->fast_double_to_string(risk) + 
                              ", Daily PnL: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Max limit: " + logger->fast_double_to_string(-max_loss_amount));
        return std::nullopt;
    }
      
    STRATEGY_LOG(logger, log_signal, signal);
    return signal;
}

std::optional<Signal> Strategy::execute_short() {
    Signal signal = go(TradeDirection::SHORT);
    
    if (signal.quantity < 0.0 || signal.price <= 0.0) {
        STRATEGY_LOG(logger, log_general, "Sell parameters incorrect", LogLevel::ERROR);
        throw std::runtime_error("Sell parameters not properly set");
    }

    if (signal.quantity == 0.0) {
        STRATEGY_LOG(logger, log_general, "Sell quantity zero, trade cancelled", LogLevel::WARNING);
        return std::nullopt;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(signal);
    STRATEGY_LOG(logger, log_risk_calculation, risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        STRATEGY_LOG(logger, log_general, "SHORT trade rejected: excessive risk", LogLevel::WARNING);
        STRATEGY_LOG(logger, log_filter_result, "Risk limit", false, "Calculated risk: " + logger->fast_double_to_string(risk) + 
                              ", Daily PnL: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Max limit: " + logger->fast_double_to_string(-max_loss_amount));
        return std::nullopt;
    }
    
    STRATEGY_LOG(logger, log_signal, signal);
    return signal;
}

std::optional<Signal> Strategy::execute_lua_script() {
    // If Lua script is not enabled or not ready, return no signal
    if (!lua_script_engine || !lua_script_engine->is_ready()) {
        return std::nullopt;
    }

    // Evaluate the Lua script to get a signal
    std::optional<Signal> lua_signal = lua_script_engine->evaluate();
    if (!lua_signal || lua_signal->type == SignalType::NONE) {
        return std::nullopt;
    }

    // If the Lua script returned a BUY or SELL signal, we can enrich it with default values for missing parameters
    if (lua_signal->type == SignalType::BUY || lua_signal->type == SignalType::SELL) {
        TradeDirection direction = (lua_signal->type == SignalType::BUY) ? TradeDirection::LONG : TradeDirection::SHORT;
        Signal base_signal = go(direction);
        
        if (lua_signal->quantity > 0.0) base_signal.quantity = lua_signal->quantity;
        if (lua_signal->price > 0.0) base_signal.price = lua_signal->price;
        if (lua_signal->take_profit > 0.0) base_signal.take_profit = lua_signal->take_profit;
        if (lua_signal->stop_loss > 0.0) base_signal.stop_loss = lua_signal->stop_loss;

        return base_signal;
    }

    // Liquidate signals by default will have quantity 1.0 if not specified, to ensure they are actionable
    if (lua_signal->type == SignalType::LIQUIDATE && lua_signal->quantity <= 0.0) {
        lua_signal->quantity = 1.0;
    }

    return lua_signal;
}

bool Strategy::executeFilters(std::vector<filter::GenericFilter>& filters) {
    // If empty, always false
    if (filters.empty()) 
        return false; // No filters defined, never pass

    // If all filters are disabled, always false
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

    // Check and reset indicators if new trading day (to handle overnight/weekend gaps)
    check_and_reset_indicators_for_new_day();

    bool indicators_ready = update_indicators();
    before();

    // Update indicators
    if (!indicators_ready) {
        STRATEGY_LOG(logger, log_execution_step, "Indicators not ready yet", false);
        return Signal();  // Return empty signal
    }
    
    
    // Check if daily max profit has been reached
    if (is_daily_max_profit_reached()) {
        STRATEGY_LOG(logger, log_execution_step, "Daily max profit reached", false);
        double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
        STRATEGY_LOG(logger, log_general, "Daily maximum profit reached: " + 
                          logger->fast_double_to_string(daily_pnl) + 
                          " >= " + logger->fast_double_to_string(max_profit_amount) + 
                          " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }

    // Check if daily max drawdown has been reached
    if (is_daily_drawdown_reached()) {
        STRATEGY_LOG(logger, log_execution_step, "Daily max drawdown reached", false);
        double max_drawdown_amount = base_config.cash * base_config.daily_max_drawdown_percentage / 100.0;
        double current_drawdown = daily_max_pnl - daily_pnl;
        STRATEGY_LOG(logger, log_general, "Daily maximum drawdown reached: " + 
                          logger->fast_double_to_string(current_drawdown) + 
                          " >= " + logger->fast_double_to_string(max_drawdown_amount) + 
                          " (" + logger->fast_double_to_string(base_config.daily_max_drawdown_percentage) + "%)" +
                          " - Max PnL: " + logger->fast_double_to_string(daily_max_pnl) +
                          " - Current PnL: " + logger->fast_double_to_string(daily_pnl), LogLevel::INFO);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }

    // Time check
    if (!check_time()) {
        STRATEGY_LOG(logger, log_execution_step, "Time check", false);
        
        Signal signal = generate_liquidation_signal();
        STRATEGY_LOG(logger, log_signal, signal);
        return signal;
    }
    STRATEGY_LOG(logger, log_execution_step, "Time check", true);


    // Check for break-even signal before executing strategy
    auto be_signal = check_break_even();
    if (be_signal) {
        STRATEGY_LOG(logger, log_general, "Break-even signal generated: " + logger->fast_double_to_string(be_signal->new_sl));
        return *be_signal;
    }

    auto lua_signal = execute_lua_script();
    if (lua_signal) {
        STRATEGY_LOG(logger, log_execution_step, "Lua script generated signal", true);
        STRATEGY_LOG(logger, log_signal, *lua_signal);
        return *lua_signal;
    }

    if (position_info.entry_price > 0.0) {
        // TODO differentiate normal liquidation signals and rebuy/resale signals
        if (executeFilters(resaleFilters)) {
            STRATEGY_LOG(logger, log_execution_step, "Resale filters passed - generating liquidation signal", true);
            Signal signal = generate_liquidation_signal();
            STRATEGY_LOG(logger, log_signal, signal);
            return signal;
        }

        if (executeFilters(rebuyFilters)) {
            STRATEGY_LOG(logger, log_execution_step, "Rebuy filters passed - generating liquidation signal", true);
            Signal signal = generate_liquidation_signal();
            STRATEGY_LOG(logger, log_signal, signal);
            return signal;
        }

        STRATEGY_LOG(logger, log_execution_step, "Position open - no new signal generated", false);
        return Signal();  // Return empty signal
    }

    // If ML-entry is enabled, ask the ML model first for an entry signal.
    if (base_config.use_ml_entry) {
        int ml_signal = execute_ml_filters();
        if (ml_signal == 1) { // BUY
            STRATEGY_LOG(logger, log_execution_step, "ML signal -> BUY", true);
            auto signal = execute_long();
            return signal.value_or(Signal());
        } else if (ml_signal == 2) { // SELL
            STRATEGY_LOG(logger, log_execution_step, "ML signal -> SELL", true);
            auto signal = execute_short();
            return signal.value_or(Signal());
        }
    }

    if (executeFilters(buyFilters)) {
        STRATEGY_LOG(logger, log_execution_step, "Buy filters passed", true);
        auto signal = execute_long();
        return signal.value_or(Signal());
    } else if (executeFilters(sellFilters)) {
        STRATEGY_LOG(logger, log_execution_step, "Sell filters passed", true);
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

    if (base_config.use_lua_script && !base_config.lua_script.empty()) {
        lua_script_engine = std::make_unique<LuaScriptEngine>(
            base_config.lua_script,
            *candle_manager,
            position_info,
            logger.get());

        if (!lua_script_engine->initialize()) {
            std::string err = lua_script_engine->get_last_error();
            STRATEGY_LOG(logger, log_general, "Lua script disabled after initialization failure: " + err, LogLevel::ERROR);
            throw std::runtime_error("Lua script execution failed at initialization: " + err);
        } else {
            STRATEGY_LOG(logger, log_general, "Lua script enabled", LogLevel::INFO);
        }
    }

    if (base_config.sl_method == StopLossMethod::ATR || base_config.tp_method == TakeProfitMethod::ATR || base_config.sl_method == StopLossMethod::MinMax) 
        indicator_manager->registerATR(filter::ATRParams(base_config.atr_period, true));
    
    // Load ML model if enabled
    if (base_config.use_ml_entry) {
        if (!load_ml_model()) {
            logger->log_general("Failed to load ML model: " + base_config.ml_entry_model_path, LogLevel::ERROR);
            throw std::runtime_error("Failed to load ML model for entry signals");
        }
        logger->log_general("ML model loaded successfully for entry signals", LogLevel::INFO);
    }
    
    // Adjust CandleManager parameters according to the maximum required period
    int max_period = indicator_manager->getMaxRequiredPeriods();
    if (base_config.sl_method == StopLossMethod::MinMax) 
        max_period = std::max(max_period, base_config.sl_minmax_periods);
    if (base_config.use_lua_script && lua_script_engine) {
        // Respect dynamic history requested by Lua script via set_required_history().
        max_period = std::max(max_period, lua_script_engine->get_required_history());
    }
    
    // Configure the CandleManager with the minimal required size
    // The setMinimalBufferSize method automatically handles internal parameters
    // to ensure optimal and safe operation
    candle_manager->setMinimalBufferSize(max_period);
    
    STRATEGY_LOG(logger, log_general, "CandleManager configured with minimal size: " + 
                       std::to_string(max_period) + " candles", LogLevel::INFO);

    // Generate the full configuration
    std::ostringstream config_stream;
    config_stream << base_config;
    std::string config_str = config_stream.str();

    STRATEGY_LOG(logger, log_general, config_str, LogLevel::INFO);
    STRATEGY_LOG_VOID(logger, finalize_and_send_logs);  // Force immediate send
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
        STRATEGY_LOG(logger, log_general, "Closed trade PnL: " + logger->fast_double_to_string(last_trade_pnl));
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

// ============================================================================
// Machine Learning Methods for Entry Signals
// ============================================================================

bool Strategy::load_ml_model() {
    try {
        ml_entry_model = std::make_unique<InferenceModel>();
        bool loaded = ml_entry_model->load(base_config.ml_entry_model_path);
        
        if (!loaded || !ml_entry_model->initialized()) {
            logger->log_general("Failed to load ML model", LogLevel::ERROR);
            return false;
        }
        
        ml_model_loaded = true;
        logger->log_general("ML model loaded: " + base_config.ml_entry_model_path, LogLevel::INFO);
        logger->log_general("Lookback periods: " + std::to_string(base_config.ml_entry_lookback_periods), LogLevel::INFO);
        logger->log_general("Threshold: " + std::to_string(base_config.ml_entry_threshold), LogLevel::INFO);
        logger->log_general("Normalization: " + std::string(base_config.ml_entry_normalize ? "enabled" : "disabled"), LogLevel::INFO);
        
        return true;
    } catch (const std::exception& e) {
        logger->log_general("Exception while loading ML model: " + std::string(e.what()), LogLevel::ERROR);
        return false;
    }
}

std::vector<float> Strategy::prepare_ml_features() {
    std::vector<float> features;
    
    // Check that we have enough historical candles
    if (candle_manager->size() < static_cast<size_t>(base_config.ml_entry_lookback_periods)) {
        logger->log_general(
            "Not enough historical data for ML: " + std::to_string(candle_manager->size()) + 
            " < " + std::to_string(base_config.ml_entry_lookback_periods), 
            LogLevel::DEBUG
        );
        return features;  // Return empty vector
    }
    
    // Get the last N candles
    auto recent_candles = candle_manager->get_last_candles(base_config.ml_entry_lookback_periods);
    
    // Build the feature vector from OHLC data
    for (const auto& candle : recent_candles) {
        if (base_config.ml_entry_normalize) {
            features.push_back(static_cast<float>(candle.open));
            features.push_back(static_cast<float>(candle.high));
            features.push_back(static_cast<float>(candle.low));
            features.push_back(static_cast<float>(candle.close));
        } else {
            features.push_back(static_cast<float>(candle.open));
            features.push_back(static_cast<float>(candle.high));
            features.push_back(static_cast<float>(candle.low));
            features.push_back(static_cast<float>(candle.close));
        }
    }
    
    logger->log_general(
        "ML features prepared: " + std::to_string(features.size()) + " values (" + 
        std::to_string(recent_candles.size()) + " candles × 4)", 
        LogLevel::DEBUG
    );
    
    return features;
}

int Strategy::interpret_ml_prediction(float prediction) {
    // Interpretation of model output
    // prediction > threshold => BUY (1)
    // prediction < -threshold => SELL (2)  
    // otherwise => no signal (0)
    
    if (prediction > base_config.ml_entry_threshold) 
        return 1;  // BUY
    else if (prediction < -base_config.ml_entry_threshold) 
        return 2;  // SELL
    
    return 0;  // No signal
}

int Strategy::execute_ml_filters() {
    if (!ml_model_loaded) {
        logger->log_general("ML model not loaded", LogLevel::ERROR);
        return false;
    }
    
    // Prepare features for inference
    std::vector<float> features = prepare_ml_features();
    
    if (features.empty()) {
        logger->log_general("ML features empty, no inference possible", LogLevel::DEBUG);
        return false;
    }
    
        // Perform inference
            float prediction = ml_entry_model->predict(features);
        
            logger->log_general("ML prediction: " + std::to_string(prediction), LogLevel::DEBUG);
        
            // Interpret prediction
            int signal_type = interpret_ml_prediction(prediction);

            if (signal_type == 1) {
                logger->log_general(
                    "BUY signal detected by ML (prediction: " + std::to_string(prediction) + ")",
                    LogLevel::INFO
                );
                return 1;
            } else if (signal_type == 2) {
                logger->log_general(
                    "SELL signal detected by ML (prediction: " + std::to_string(prediction) + ")",
                    LogLevel::INFO
                );
                return 2;
            }

            // No signal
            logger->log_general(
                "No ML signal (prediction: " + std::to_string(prediction) + 
                ", threshold: ±" + std::to_string(base_config.ml_entry_threshold) + ")",
                LogLevel::DEBUG
            );
            return 0;
}
