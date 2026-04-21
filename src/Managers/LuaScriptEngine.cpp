#include "Managers/LuaScriptEngine.hpp"

#include "Managers/CandleManager.hpp"
#include "Managers/LoggerManager.hpp"
#include "Indicators/indicators.hpp"

#include <algorithm>
#include <cctype>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

LuaScriptEngine::LuaScriptEngine(const std::string& script,
                                 CandleManager& candle_manager,
                                 const PositionInfo& position_info,
                                 ILogger* logger)
    : m_script(script),
      m_candle_manager(candle_manager),
      m_position_info(position_info),
    m_logger(logger),
    m_required_history()
{
}

LuaScriptEngine::~LuaScriptEngine()
{
    if (m_lua_state) {
        lua_close(m_lua_state);
        m_lua_state = nullptr;
    }
}

bool LuaScriptEngine::initialize()
{
    if (m_script.empty()) {
        m_ready = false;
        return false;
    }

    m_lua_state = luaL_newstate();
    if (!m_lua_state) {
        log_error("Lua initialization failed: cannot allocate Lua state");
        m_ready = false;
        return false;
    }

    luaL_openlibs(m_lua_state);
    register_helpers();

    lua_newtable(m_lua_state);
    lua_pushstring(m_lua_state, "NONE");
    lua_setfield(m_lua_state, -2, "NONE");
    lua_pushstring(m_lua_state, "BUY");
    lua_setfield(m_lua_state, -2, "BUY");
    lua_pushstring(m_lua_state, "SELL");
    lua_setfield(m_lua_state, -2, "SELL");
    lua_pushstring(m_lua_state, "LIQUIDATE");
    lua_setfield(m_lua_state, -2, "LIQUIDATE");
    lua_pushstring(m_lua_state, "MOVE_SL");
    lua_setfield(m_lua_state, -2, "MOVE_SL");
    lua_setglobal(m_lua_state, "SignalType");

    const int load_status = luaL_loadbuffer(
        m_lua_state,
        m_script.c_str(),
        m_script.size(),
        "strategy_script");

    if (load_status != LUA_OK) {
        const char* error_message = lua_tostring(m_lua_state, -1);
        m_last_error = std::string("Lua compile error: ") + (error_message ? error_message : "unknown error");
        log_error(m_last_error);
        lua_pop(m_lua_state, 1);
        m_ready = false;
        return false;
    }

    if (lua_pcall(m_lua_state, 0, 0, 0) != LUA_OK) {
        const char* error_message = lua_tostring(m_lua_state, -1);
        m_last_error = std::string("Lua execution error during initialization: ") +
                  (error_message ? error_message : "unknown error");
        log_error(m_last_error);
        lua_pop(m_lua_state, 1);
        m_ready = false;
        return false;
    }

    lua_getglobal(m_lua_state, "on_candle");
    const bool valid_function = lua_isfunction(m_lua_state, -1) != 0;
    lua_pop(m_lua_state, 1);

    if (!valid_function) {
        m_last_error = "Lua script must define function on_candle(candle, position)";
        log_error(m_last_error);
        m_ready = false;
        return false;
    }

    log_debug("Lua script initialized successfully");
    m_ready = true;
    return true;
}

bool LuaScriptEngine::validate_script(const std::string& script, std::string& error_message)
{
    if (script.empty()) {
        error_message = "Script is empty.";
        return false;
    }

    lua_State* L = luaL_newstate();
    if (!L) {
        error_message = "Failed to allocate Lua state.";
        return false;
    }

    luaL_openlibs(L);

    // Provide the minimum global env
    lua_newtable(L);
    lua_pushstring(L, "NONE"); lua_setfield(L, -2, "NONE");
    lua_pushstring(L, "BUY"); lua_setfield(L, -2, "BUY");
    lua_pushstring(L, "SELL"); lua_setfield(L, -2, "SELL");
    lua_pushstring(L, "LIQUIDATE"); lua_setfield(L, -2, "LIQUIDATE");
    lua_pushstring(L, "MOVE_SL"); lua_setfield(L, -2, "MOVE_SL");
    lua_setglobal(L, "SignalType");

    // Dummy helper functions to avoid runtime errors on script load
    auto dummy = [](lua_State*) -> int { return 0; };
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "get_candle");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "candles_count");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "get_position");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "log");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "set_required_history");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "ema");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "rsi");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "stoch");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "atr");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "atrc");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "cci");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "macd");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "bb");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "supertrend");
    lua_pushcclosure(L, dummy, 0); lua_setglobal(L, "time_cyclic");

    if (luaL_loadbuffer(L, script.c_str(), script.size(), "strategy_script") != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        error_message = std::string("Compile error: ") + (msg ? msg : "unknown error");
        lua_close(L);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        error_message = std::string("Execution error during initialization: ") + (msg ? msg : "unknown error");
        lua_close(L);
        return false;
    }

    lua_getglobal(L, "on_candle");
    if (!lua_isfunction(L, -1)) {
        error_message = "Lua script must define function on_candle(candle, position)";
        lua_close(L);
        return false;
    }

    lua_close(L);
    return true;
}

std::optional<Signal> LuaScriptEngine::evaluate()
{
    if (!m_ready || !m_lua_state || m_candle_manager.size() == 0) {
        return std::nullopt;
    }

    lua_getglobal(m_lua_state, "on_candle");
    if (!lua_isfunction(m_lua_state, -1)) {
        lua_pop(m_lua_state, 1);
        m_last_error = "Lua runtime error: on_candle is no longer a valid function";
        log_error(m_last_error);
        throw std::runtime_error(m_last_error);
    }

    push_candle_table(m_lua_state, m_candle_manager.get_latest_candle());
    push_position_table(m_lua_state);

    if (lua_pcall(m_lua_state, 2, 1, 0) != LUA_OK) {
        const char* error_message = lua_tostring(m_lua_state, -1);
        m_last_error = std::string("Lua runtime error: ") + (error_message ? error_message : "unknown error");
        log_error(m_last_error);
        lua_pop(m_lua_state, 1);
        throw std::runtime_error(m_last_error);
    }

    std::optional<Signal> signal = parse_signal_from_stack(m_lua_state, -1);
    lua_pop(m_lua_state, 1);
    return signal;
}

LuaScriptEngine* LuaScriptEngine::self_from_upvalue(lua_State* L)
{
    return static_cast<LuaScriptEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
}

int LuaScriptEngine::lua_get_candle(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const lua_Integer offset = luaL_optinteger(L, 1, 0);
    if (offset < 0) {
        lua_pushnil(L);
        return 1;
    }

    const size_t count = self->m_candle_manager.size();
    if (count == 0 || static_cast<size_t>(offset) >= count) {
        lua_pushnil(L);
        return 1;
    }

    const size_t logical_index = (count - 1) - static_cast<size_t>(offset);
    self->push_candle_table(L, self->m_candle_manager.at(logical_index));
    return 1;
}

int LuaScriptEngine::lua_candles_count(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, static_cast<lua_Integer>(self->m_candle_manager.size()));
    return 1;
}

int LuaScriptEngine::lua_get_position(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    self->push_position_table(L);
    return 1;
}

int LuaScriptEngine::lua_log(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        return 0;
    }

    const char* message = luaL_optstring(L, 1, "");
    self->log_debug(std::string("[Lua] ") + message);
    return 0;
}

int LuaScriptEngine::lua_set_required_history(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        return 0;
    }

    const int history = static_cast<int>(luaL_optinteger(L, 1, 200));
    self->m_required_history = std::max(1, history);
    self->log_debug(std::string("[Lua] Required history set to: ") + std::to_string(self->m_required_history));
    return 0;
}

int LuaScriptEngine::lua_ema(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 20));
    const int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (period <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    EMA indicator{filter::EMAParams(period)};
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value.value());
    return 1;
}

int LuaScriptEngine::lua_rsi(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 14));
    const int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (period <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    RSI indicator{filter::RSIParams(period)};
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value.value());
    return 1;
}

int LuaScriptEngine::lua_stoch(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int fast_k = static_cast<int>(luaL_optinteger(L, 1, 14));
    const int slow_k = static_cast<int>(luaL_optinteger(L, 2, 3));
    const int slow_d = static_cast<int>(luaL_optinteger(L, 3, 3));
    const int offset = static_cast<int>(luaL_optinteger(L, 4, 0));
    if (fast_k <= 0 || slow_k <= 0 || slow_d <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    STOCH indicator(filter::StochasticParams(fast_k, slow_k, slow_d));
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushnumber(L, value->first);
    lua_setfield(L, -2, "k");
    lua_pushnumber(L, value->second);
    lua_setfield(L, -2, "d");
    return 1;
}

int LuaScriptEngine::lua_atr(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 14));
    bool use_log = false;
    int offset = 0;

    if (lua_gettop(L) >= 2) {
        if (lua_isboolean(L, 2)) {
            use_log = lua_toboolean(L, 2) != 0;
        } else if (lua_isnumber(L, 2)) {
            offset = static_cast<int>(lua_tointeger(L, 2));
        }
    }
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        offset = static_cast<int>(lua_tointeger(L, 3));
    }

    if (period <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    ATR indicator(filter::ATRParams(period, use_log));
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value.value());
    return 1;
}

int LuaScriptEngine::lua_atrc(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 14));
    const int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (period <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    ATRC indicator(period);
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value.value());
    return 1;
}

int LuaScriptEngine::lua_cci(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 20));
    const int offset = static_cast<int>(luaL_optinteger(L, 2, 0));
    if (period <= 0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    CCI indicator{filter::CCIParams(period)};
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, value.value());
    return 1;
}

int LuaScriptEngine::lua_macd(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int fast = static_cast<int>(luaL_optinteger(L, 1, 12));
    const int slow = static_cast<int>(luaL_optinteger(L, 2, 26));
    const int signal = static_cast<int>(luaL_optinteger(L, 3, 9));
    const int offset = static_cast<int>(luaL_optinteger(L, 4, 0));
    const std::string source = luaL_optstring(L, 5, "CLOSE");
    const std::string osc_ma = luaL_optstring(L, 6, "EMA");
    const std::string signal_ma = luaL_optstring(L, 7, "EMA");
    const int signal_smoothing = static_cast<int>(luaL_optinteger(L, 8, 0));

    if (fast <= 0 || slow <= 0 || signal <= 0 || fast >= slow) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    filter::MACDParams params(
        fast,
        slow,
        signal,
        self->parse_price_source(source),
        self->parse_ma_type(osc_ma),
        self->parse_ma_type(signal_ma),
        signal_smoothing);

    MACD indicator(params);
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushnumber(L, value->macdLine);
    lua_setfield(L, -2, "macd_line");
    lua_pushnumber(L, value->signalLine);
    lua_setfield(L, -2, "signal_line");
    lua_pushnumber(L, value->histogram);
    lua_setfield(L, -2, "histogram");
    return 1;
}

int LuaScriptEngine::lua_bb(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int period = static_cast<int>(luaL_optinteger(L, 1, 20));
    const double stddev = luaL_optnumber(L, 2, 2.0);
    const int offset = static_cast<int>(luaL_optinteger(L, 3, 0));
    const std::string source = luaL_optstring(L, 4, "CLOSE");
    const std::string ma_type = luaL_optstring(L, 5, "SMA");

    if (period <= 0 || stddev <= 0.0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    filter::BBParams params(period, stddev, 0);
    const filter::PriceType price_source = self->parse_price_source(source);
    if (price_source == filter::PriceType::OPEN) {
        params.source = 0;
    } else if (price_source == filter::PriceType::HIGH) {
        params.source = 1;
    } else if (price_source == filter::PriceType::LOW) {
        params.source = 2;
    } else {
        params.source = 3;
    }
    params.ma_type = (self->parse_ma_type(ma_type) == filter::MAType::EMA) ? 1 : 0;

    BB indicator(params);
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushnumber(L, value->middle);
    lua_setfield(L, -2, "middle");
    lua_pushnumber(L, value->upper);
    lua_setfield(L, -2, "upper");
    lua_pushnumber(L, value->lower);
    lua_setfield(L, -2, "lower");
    lua_pushnumber(L, value->percentB);
    lua_setfield(L, -2, "percent_b");
    return 1;
}

int LuaScriptEngine::lua_supertrend(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int atr_period = static_cast<int>(luaL_optinteger(L, 1, 10));
    const double multiplier = luaL_optnumber(L, 2, 3.0);
    const int offset = static_cast<int>(luaL_optinteger(L, 3, 0));

    if (atr_period <= 0 || multiplier <= 0.0) {
        lua_pushnil(L);
        return 1;
    }

    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    SUPERTREND indicator(filter::SuperTrendParams(atr_period, multiplier));
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushnumber(L, value->first);
    lua_setfield(L, -2, "value");
    lua_pushinteger(L, value->second);
    lua_setfield(L, -2, "direction");
    lua_pushboolean(L, value->second > 0);
    lua_setfield(L, -2, "is_uptrend");
    lua_pushboolean(L, value->second < 0);
    lua_setfield(L, -2, "is_downtrend");
    return 1;
}

int LuaScriptEngine::lua_time_cyclic(lua_State* L)
{
    LuaScriptEngine* self = self_from_upvalue(L);
    if (!self) {
        lua_pushnil(L);
        return 1;
    }

    const int offset = static_cast<int>(luaL_optinteger(L, 1, 0));
    const auto history = self->get_history_for_offset(offset);
    if (history.empty()) {
        lua_pushnil(L);
        return 1;
    }

    TIMECYCLIC indicator{filter::TimeCyclicParams()};
    const auto value = indicator.initialize_with_history(history);
    if (!value.has_value()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushnumber(L, value->first);
    lua_setfield(L, -2, "sin");
    lua_pushnumber(L, value->second);
    lua_setfield(L, -2, "cos");
    return 1;
}

void LuaScriptEngine::register_helpers()
{
    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_get_candle, 1);
    lua_setglobal(m_lua_state, "get_candle");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_candles_count, 1);
    lua_setglobal(m_lua_state, "candles_count");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_get_position, 1);
    lua_setglobal(m_lua_state, "get_position");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_log, 1);
    lua_setglobal(m_lua_state, "log");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_set_required_history, 1);
    lua_setglobal(m_lua_state, "set_required_history");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_ema, 1);
    lua_setglobal(m_lua_state, "ema");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_rsi, 1);
    lua_setglobal(m_lua_state, "rsi");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_stoch, 1);
    lua_setglobal(m_lua_state, "stoch");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_atr, 1);
    lua_setglobal(m_lua_state, "atr");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_atrc, 1);
    lua_setglobal(m_lua_state, "atrc");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_cci, 1);
    lua_setglobal(m_lua_state, "cci");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_macd, 1);
    lua_setglobal(m_lua_state, "macd");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_bb, 1);
    lua_setglobal(m_lua_state, "bb");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_supertrend, 1);
    lua_setglobal(m_lua_state, "supertrend");

    lua_pushlightuserdata(m_lua_state, this);
    lua_pushcclosure(m_lua_state, &LuaScriptEngine::lua_time_cyclic, 1);
    lua_setglobal(m_lua_state, "time_cyclic");
}

void LuaScriptEngine::push_candle_table(lua_State* L, const BasicCandle& candle) const
{
    lua_newtable(L);

    lua_pushnumber(L, candle.open);
    lua_setfield(L, -2, "open");

    lua_pushnumber(L, candle.high);
    lua_setfield(L, -2, "high");

    lua_pushnumber(L, candle.low);
    lua_setfield(L, -2, "low");

    lua_pushnumber(L, candle.close);
    lua_setfield(L, -2, "close");

    lua_pushinteger(L, candle.date.year);
    lua_setfield(L, -2, "year");

    lua_pushinteger(L, candle.date.month);
    lua_setfield(L, -2, "month");

    lua_pushinteger(L, candle.date.day);
    lua_setfield(L, -2, "day");

    lua_pushinteger(L, candle.date.time.hour);
    lua_setfield(L, -2, "hour");

    lua_pushinteger(L, candle.date.time.minute);
    lua_setfield(L, -2, "minute");

    lua_pushinteger(L, candle.date.time.second);
    lua_setfield(L, -2, "second");
}

void LuaScriptEngine::push_position_table(lua_State* L) const
{
    lua_newtable(L);

    lua_pushboolean(L, m_position_info.entry_price > 0.0);
    lua_setfield(L, -2, "is_open");

    lua_pushnumber(L, m_position_info.entry_price);
    lua_setfield(L, -2, "entry_price");

    lua_pushnumber(L, m_position_info.take_profit_price);
    lua_setfield(L, -2, "take_profit_price");

    lua_pushnumber(L, m_position_info.closed_trade_pnl);
    lua_setfield(L, -2, "closed_trade_pnl");
}

std::optional<Signal> LuaScriptEngine::parse_signal_from_stack(lua_State* L, int stack_index) const
{
    if (lua_isnoneornil(L, stack_index)) {
        return std::nullopt;
    }

    Signal signal;

    if (lua_isstring(L, stack_index)) {
        const char* type_string = lua_tostring(L, stack_index);
        signal.type = parse_signal_type(type_string ? type_string : "NONE");
        if (signal.type == SignalType::NONE) {
            return std::nullopt;
        }
        return signal;
    }

    if (!lua_istable(L, stack_index)) {
        log_error("Lua return value must be nil, string, or table");
        return std::nullopt;
    }

    lua_getfield(L, stack_index, "type");
    const char* type_string = lua_tostring(L, -1);
    signal.type = parse_signal_type(type_string ? type_string : "NONE");
    lua_pop(L, 1);

    if (signal.type == SignalType::NONE) {
        return std::nullopt;
    }

    lua_getfield(L, stack_index, "quantity");
    if (lua_isnumber(L, -1)) {
        signal.quantity = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, stack_index, "price");
    if (lua_isnumber(L, -1)) {
        signal.price = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, stack_index, "take_profit");
    if (lua_isnumber(L, -1)) {
        signal.take_profit = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, stack_index, "stop_loss");
    if (lua_isnumber(L, -1)) {
        signal.stop_loss = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, stack_index, "new_sl");
    if (lua_isnumber(L, -1)) {
        signal.new_sl = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    return signal;
}

SignalType LuaScriptEngine::parse_signal_type(const std::string& signal_type) const
{
    std::string upper = signal_type;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (upper == "BUY") {
        return SignalType::BUY;
    }
    if (upper == "SELL") {
        return SignalType::SELL;
    }
    if (upper == "LIQUIDATE" || upper == "CLOSE") {
        return SignalType::LIQUIDATE;
    }
    if (upper == "MOVE_SL" || upper == "MOVE-STOP" || upper == "MOVE_STOP") {
        return SignalType::MOVE_SL;
    }

    return SignalType::NONE;
}

std::vector<BasicCandle> LuaScriptEngine::get_history_for_offset(int offset) const
{
    if (offset < 0) {
        return {};
    }

    const size_t count = m_candle_manager.size();
    if (count == 0 || static_cast<size_t>(offset) >= count) {
        return {};
    }

    // offset=0 -> include latest candle
    // offset=1 -> exclude latest candle (value at previous candle)
    const size_t target_count = count - static_cast<size_t>(offset);
    std::vector<BasicCandle> history;
    history.reserve(target_count);

    for (size_t i = 0; i < target_count; ++i) {
        history.push_back(m_candle_manager.at(i));
    }

    return history;
}

filter::PriceType LuaScriptEngine::parse_price_source(const std::string& source) const
{
    std::string upper = source;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (upper == "OPEN") {
        return filter::PriceType::OPEN;
    }
    if (upper == "HIGH") {
        return filter::PriceType::HIGH;
    }
    if (upper == "LOW") {
        return filter::PriceType::LOW;
    }
    return filter::PriceType::CLOSE;
}

filter::MAType LuaScriptEngine::parse_ma_type(const std::string& ma_type) const
{
    std::string upper = ma_type;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (upper == "SMA") {
        return filter::MAType::SMA;
    }
    return filter::MAType::EMA;
}

void LuaScriptEngine::log_debug(const std::string& message) const
{
    if (m_logger) {
        m_logger->log_general(message, LogLevel::DEBUG);
    }
}

void LuaScriptEngine::log_error(const std::string& message) const
{
    if (m_logger) {
        m_logger->log_general(message, LogLevel::ERROR);
    }
}
