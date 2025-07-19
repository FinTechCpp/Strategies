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
    

// Implementation of Strategy class methods
// Calculate the potential risk of a trade in monetary value
double Strategy::calculate_trade_risk(bool is_long) {
    double position_value;
    double risk_value;
    
    if (is_long) {
        position_value = buy_quantity * buy_price;
        risk_value = position_value * (stop_loss_distance / buy_price);
    } else {
        position_value = sell_quantity * sell_price;
        risk_value = position_value * (stop_loss_distance / sell_price);
    }
    
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

bool Strategy::is_new_trading_day() {
    if (!candle_manager.get_latest_candle().date.is_valid() || !current_trading_day.is_valid()) {
        return true;
    }
    
    return (candle_manager.get_latest_candle().date.year != current_trading_day.year ||
            candle_manager.get_latest_candle().date.month != current_trading_day.month ||
            candle_manager.get_latest_candle().date.day != current_trading_day.day);
}
    
void Strategy::update_daily_pnl_tracking() {
    if (!base_config.use_daily_max_loss && !base_config.use_daily_max_profit) {
        return;
    }

    // If it's a new day, reset the counter and reactivate trading
    if (is_new_trading_day()) {
        current_trading_day = candle_manager.get_latest_candle().date;
        daily_pnl = 0.0;

        if (base_config.use_daily_max_loss) {
            // Calculate the maximum allowed loss amount for this day
            double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
            
            logger->log_general("Nouveau jour de trading: " + current_trading_day.to_string() + 
                              " - Perte max autorisée: " + logger->fast_double_to_string(max_loss_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_loss_percentage) + "%)", LogLevel::INFO);
        }

        if (base_config.use_daily_max_profit) {
            // Calculate the maximum allowed profit amount for this day
            double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
            
            logger->log_general("Nouveau jour de trading: " + current_trading_day.to_string() + 
                              " - Profit max autorisé: " + logger->fast_double_to_string(max_profit_amount) + 
                              " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        }
    }
    
    if (last_trade_pnl != 0.0) {
        daily_pnl += last_trade_pnl;
        
        logger->log_general("P&L du trade: " + logger->fast_double_to_string(last_trade_pnl) + 
                          " - P&L journalier cumulé: " + logger->fast_double_to_string(daily_pnl), LogLevel::INFO);

        last_trade_pnl = 0.0;
    }
}

// Method to check if we are within trading hours
bool Strategy::check_time() {
    if (!candle_manager.get_latest_candle().date.is_valid()) {
        logger->log_time_check(false, "Date de bougie invalide", LogLevel::WARNING);
        return false;
    }

    const DateTime& current_date = candle_manager.get_latest_candle().date;
    const Time& current_time = current_date.time;

    // Update the current trading day if it's a new day
    if (current_date.day != last_check_date.day ||
        current_date.month != last_check_date.month ||
        current_date.year != last_check_date.year) {

        last_check_date = current_date;

        int weekday = get_day_of_week(current_date);
        weekday_check = std::find(base_config.trading_days.begin(),
                                  base_config.trading_days.end(),
                                  weekday) != base_config.trading_days.end();

        if (!weekday_check) {
            logger->log_time_check(false, "Jour non autorisé pour le trading: " +
                                 current_date.to_string(), LogLevel::INFO);
            return false;
        }
    }

    // Check trading hours on each candle
    bool after_start = (base_config.trading_from < current_time);

    bool before_end = (current_time < base_config.trading_to);

    time_check = after_start && before_end;

    if (!weekday_check) {
        logger->log_time_check(false, "Jour non autorisé pour le trading: " +
                             current_date.to_string(), LogLevel::INFO);
        return false;
    }

    if (!time_check) {
        logger->log_time_check(false,
                             std::to_string(current_time.hour) + ":" +
                             std::to_string(current_time.minute), LogLevel::INFO);
    } else {
        logger->log_time_check(true,
                             std::to_string(current_time.hour) + ":" +
                             std::to_string(current_time.minute), LogLevel::DEBUG);
    }

    return time_check;
}

std::unique_ptr<Signal> Strategy::check_break_even() {
    if (!base_config.use_break_even || position_info.entry_price <= 0.0 || position_info.take_profit_price <= 0.0) {
        // Insufficient data to calculate break-even
        return nullptr;
    }

    // Get the latest candle
    const BasicCandle& latest_candle = candle_manager.get_latest_candle();
    if (!latest_candle.date.is_valid()) {
        return nullptr;  // No valid candle
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
        logger->log_general("Activation break-even: " + 
                          std::string(position_sign > 0 ? "High" : "Low") + "=" + 
                          logger->fast_double_to_string(reference_price) + 
                          " " + std::string(position_sign > 0 ? ">=" : "<=") + 
                          " seuil (" + logger->fast_double_to_string(break_even_price) + 
                          "), " + logger->fast_double_to_string(base_config.break_even_threshold * 100) + 
                          "% du chemin vers TP", LogLevel::INFO);

        auto be_signal = std::make_unique<Signal>();
        be_signal->action = "MOVE_SL";
        be_signal->new_sl = position_info.entry_price;
        be_signal->price = break_even_price;
        return be_signal;
    }

    return nullptr;
}

std::unique_ptr<Signal> Strategy::generate_buy_signal() {
    auto sig = std::make_unique<Signal>();
    sig->action = "BUY";
    sig->quantity = buy_quantity;
    sig->price = buy_price;
    sig->take_profit = take_profit_distance;
    sig->stop_loss = stop_loss_distance;
    return sig;
}

std::unique_ptr<Signal> Strategy::generate_sell_signal() {
    auto sig = std::make_unique<Signal>();
    sig->action = "SELL";
    sig->quantity = sell_quantity;
    sig->price = sell_price;
    sig->take_profit = take_profit_distance;
    sig->stop_loss = stop_loss_distance;
    return sig;
}

std::unique_ptr<Signal> Strategy::generate_liquidation_signal() {
    auto sig = std::make_unique<Signal>();
    sig->action = "LIQUIDATE";
    return sig;
}

void Strategy::reset() {
    buy_quantity = 0.0;
    buy_price = 0.0;
    sell_quantity = 0.0;
    sell_price = 0.0;
    take_profit_distance = 0.0;
    stop_loss_distance = 0.0;
    signal = nullptr;
}

void Strategy::execute_long() {
    go_long();
    
    if (buy_quantity < 0.0 || buy_price <= 0.0) {
        logger->log_general("Paramètres d'achat incorrects", LogLevel::ERROR);
        throw std::runtime_error("Buy parameters not properly set");
    }

    if (buy_quantity == 0.0) {
        logger->log_general("Quantité d'achat nulle, trade annulé", LogLevel::WARNING);
        // If quantity is zero, we cannot proceed with the trade
        reset();
        return;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(true);
    logger->log_risk_calculation(risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        logger->log_general("Trade LONG rejeté: risque excessif", LogLevel::WARNING);
        logger->log_filter_detail("Limite de risque", 
                              "Risque calculé: " + logger->fast_double_to_string(risk) + 
                              ", PnL journalier: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Limite max: " + logger->fast_double_to_string(-max_loss_amount), 
                              LogLevel::INFO);
        reset();
        return;
    }
    
    logger->log_signal("BUY", buy_price, buy_quantity);
    logger->log_sl_tp(stop_loss_distance, take_profit_distance);
            
    signal = generate_buy_signal();
}

void Strategy::execute_short() {
    go_short();
    
    if (sell_quantity < 0.0 || sell_price <= 0.0) {
        logger->log_general("Paramètres de vente incorrects", LogLevel::ERROR);
        throw std::runtime_error("Sell parameters not properly set");
    }

    if (sell_quantity == 0.0) {
        logger->log_general("Quantité de vente nulle, trade annulé", LogLevel::WARNING);
        // If quantity is zero, we cannot proceed with the trade
        reset();
        return;
    }

    // Calculate risk and check if it's acceptable
    double risk = calculate_trade_risk(false);
    logger->log_risk_calculation(risk, (risk / base_config.cash) * 100.0);
    
    if (base_config.use_daily_max_loss && !is_trade_risk_acceptable(risk)) {
        // The trade is too risky compared to our daily limit
        double max_loss_amount = base_config.cash * base_config.daily_max_loss_percentage / 100.0;
        
        logger->log_general("Trade SHORT rejeté: risque excessif", LogLevel::WARNING);
        logger->log_filter_detail("Limite de risque", 
                              "Risque calculé: " + logger->fast_double_to_string(risk) + 
                              ", PnL journalier: " + logger->fast_double_to_string(daily_pnl) + 
                              ", Limite max: " + logger->fast_double_to_string(-max_loss_amount), 
                              LogLevel::INFO);
        reset();
        return;
    }
    
    logger->log_signal("SELL", sell_price, sell_quantity);
    logger->log_sl_tp(stop_loss_distance, take_profit_distance);
    
    signal = generate_sell_signal();
}

bool Strategy::execute_filters() {
    for (const auto& filter : filters())
        if (!filter())
            return false;  // Stop execution if any filter fails
    
    return true;  // All filters passed
}

void Strategy::execute() {
    // Update daily PnL tracking
    update_daily_pnl_tracking();

    
    // Update indicators
    if (!update_indicators()) {
        logger->log_execution_step("Indicateurs pas encore prêts", false);
        // logger->log_general("Indicateurs non initialisés - Arrêt de l'exécution");
        reset();
        return;
    }

    
    // Check if daily max profit has been reached
    if (is_daily_max_profit_reached()) {
        logger->log_execution_step("Profit max journalier atteint", false);
        double max_profit_amount = base_config.cash * base_config.daily_max_profit_percentage / 100.0;
        logger->log_general("Profit maximum journalier atteint: " + 
                          logger->fast_double_to_string(daily_pnl) + 
                          " >= " + logger->fast_double_to_string(max_profit_amount) + 
                          " (" + logger->fast_double_to_string(base_config.daily_max_profit_percentage) + "%)", LogLevel::INFO);
        
        signal = generate_liquidation_signal();
        return;
    }

    // Time check
    if (!check_time()) {
        logger->log_execution_step("Vérification horaires", false);
        
        signal = generate_liquidation_signal();
        return;
    }
    logger->log_execution_step("Vérification horaires", true);


    // Check for break-even signal before executing strategy
    auto be_signal = check_break_even();
    if (be_signal) {
        logger->log_general("Signal de break-even généré: " + 
                          logger->fast_double_to_string(be_signal->new_sl));
        signal = std::move(be_signal);
        return;
    }

    
    before();
    
    bool should_long_val = should_long();
    bool should_short_val = should_long_val ? false : should_short();
    
    if (should_long_val) {
        logger->log_execution_step("Conditions de long", true);
    } else if (should_short_val) {
        logger->log_execution_step("Conditions de short", true);
    } else {
        logger->log_execution_step("Conditions d entrée", false);
        reset();
        return;
    }
    
    if (!execute_filters()) {
        logger->log_execution_step("Filtres", false);
        logger->log_general("Filtres non passés - Pas de signal généré", LogLevel::INFO);
        reset();
        return;
    }
    logger->log_execution_step("Filtres", true);
    
    if (should_long_val) {
        execute_long();
    } else {
        execute_short();
    }
    
    after();
}


Strategy::Strategy(const StrategyBaseConfig& config) 
    : base_config(config), 
    signal(std::make_unique<Signal>()),
    logger(LoggerFactory::createLogger()) {
    set_log_level(static_cast<int>(base_config.logLevel));
    set_log_enabled(base_config.enable_logging);
}

// Main update method
Signal* Strategy::update_candle(const Candle& candle) {    
    logger->start_chrono();

    position_info = candle.position;

    // Update logger with the current candle
    logger->set_current_candle(candle);
    logger->clear();  // Clear previous logs

    logger->log_general("OHLC: " + 
        logger->fast_double_to_string(candle.ohlc.open) + "/" + 
        logger->fast_double_to_string(candle.ohlc.high) + "/" + 
        logger->fast_double_to_string(candle.ohlc.low) + "/" + 
        logger->fast_double_to_string(candle.ohlc.close));

    // Store the last trade P&L if provided in candle
    if (position_info.closed_trade_pnl != 0.0) {
        last_trade_pnl = position_info.closed_trade_pnl;
        logger->log_general("PnL du trade fermé: " + logger->fast_double_to_string(last_trade_pnl));
    }

    // Add to buffer for historical calculations
    candle_manager.add_candle(candle.ohlc);
    
    // Execute strategy
    execute();

    logger->finalize_and_send_logs();
    
    return signal.get();
}

// Properties
double Strategy::price() const {
    return candle_manager.get_latest_candle().close;
}

