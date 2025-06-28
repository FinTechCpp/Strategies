#pragma once
#include "common.h"
#include <string>

/**
 * Base class for all incremental indicators
 */
template <typename ReturnType>
class IncrementalIndicator {
protected:
    bool is_initialized = false;
    std::string name;
    
public:
    IncrementalIndicator(const std::string& name) : name(name) {}
    virtual ~IncrementalIndicator() = default;

    bool requires_initialization() const { return !is_initialized; }
    bool initialized() const { return is_initialized; }

    // Getters & setters pour le nom
    const std::string& get_name() const { return name; }
    void set_name(const std::string& indicator_name) { name = indicator_name; }
    
    // Méthodes virtuelles pures pour les classes dérivées
    virtual ReturnType initialize_with_history(const std::vector<BasicCandle>& history) = 0;
    virtual ReturnType update(const BasicCandle& candle) = 0;
    virtual ReturnType get_value() const = 0;
};