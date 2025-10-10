#pragma once
#include "common.h"
#include <string>
#include <optional>

/**
 * Base class for all incremental indicators
 */
template <typename ReturnT>
class IncrementalIndicator {
protected:
    bool is_initialized = false;
    std::string name;
    int required_periods = 0;  // Nombre de périodes nécessaires pour l'initialisation
    
public:
    IncrementalIndicator(const std::string& name, int required_periods = 0) : name(name), required_periods(required_periods) {}
    virtual ~IncrementalIndicator() = default;

    bool requires_initialization() const { return !is_initialized; }
    bool initialized() const { return is_initialized; }

    // Getters pour le nom
    const std::string& get_name() const { return name; }
    int get_required_periods() const { return required_periods; }
    
    // Méthodes virtuelles pures pour les classes dérivées
    virtual std::optional<ReturnT> initialize_with_history(const std::vector<BasicCandle>& history) = 0;
    virtual std::optional<ReturnT> update(const BasicCandle& candle) = 0;
    virtual std::optional<ReturnT> get_value() const = 0;
};