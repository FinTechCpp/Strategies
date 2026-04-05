#pragma once

#include "common.h"

#include <optional>
#include <string>

struct lua_State;
class CandleManager;
class ILogger;

class LuaScriptEngine {
public:
    LuaScriptEngine(const std::string& script,
                    CandleManager& candle_manager,
                    const PositionInfo& position_info,
                    ILogger* logger);

    ~LuaScriptEngine();

    bool initialize();
    bool is_ready() const { return m_ready; }
    int get_required_history() const { return m_required_history; }

    const std::string& get_last_error() const { return m_last_error; }

    std::optional<Signal> evaluate();

    static bool validate_script(const std::string& script, std::string& error_message);

private:
    static LuaScriptEngine* self_from_upvalue(lua_State* L);

    static int lua_get_candle(lua_State* L);
    static int lua_candles_count(lua_State* L);
    static int lua_get_position(lua_State* L);
    static int lua_log(lua_State* L);
    static int lua_set_required_history(lua_State* L);

    void register_helpers();
    void push_candle_table(lua_State* L, const BasicCandle& candle) const;
    void push_position_table(lua_State* L) const;
    std::optional<Signal> parse_signal_from_stack(lua_State* L, int stack_index) const;
    SignalType parse_signal_type(const std::string& signal_type) const;

    void log_debug(const std::string& message) const;
    void log_error(const std::string& message) const;

private:
    std::string m_script;
    CandleManager& m_candle_manager;
    const PositionInfo& m_position_info;
    ILogger* m_logger = nullptr;

    lua_State* m_lua_state = nullptr;
    bool m_ready = false;
    int m_required_history;    
    std::string m_last_error;
};
