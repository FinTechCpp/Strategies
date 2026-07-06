#pragma once

#include "Indicators/IncrementalIndicator.hpp"
#include "Managers/LoggerManager.hpp"
#include "Indicators/indicators.hpp"

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <type_traits>
#include <variant>
#include <optional>

// template<typename> inline constexpr bool always_false_v = false;

// Use shared MACDResult from filter namespace (defined in common.h)
using MACDResult = filter::MACDResult;
using BBResult = filter::BBResult;

// Common interface for all indicator managers
class IIndicatorHandlerBase {
public:
    virtual ~IIndicatorHandlerBase() = default;
    virtual bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) = 0;
    virtual bool update(const BasicCandle& candle, ILogger* logger) = 0;
    virtual bool isInitialized() const = 0;
    virtual const std::string& getName() const = 0;
    virtual int getRequiredPeriods() const = 0;
    virtual void reset() = 0; // Reset indicator to uninitialized state
};

// Typed manager for each type of indicator
template<typename T, typename R>
class IndicatorHandler : public IIndicatorHandlerBase {
private:
    std::shared_ptr<T> m_indicator;
    std::deque<R> m_history; // Storage for historical values
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    
public:
    template<typename... Args>
    IndicatorHandler(Args&&... args) {
        m_indicator = std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) override {
        STRATEGY_LOG(logger, log_general, "Initializing " + m_indicator->get_name(), LogLevel::DEBUG);
        
        std::optional<R> result = m_indicator->initialize_with_history(history);
        
        if (result.has_value()) {
            // Store the initial value
            m_history.clear();
            m_history.push_back(result.value());

            STRATEGY_LOG(logger, log_indicator_value, m_indicator->get_name(), result.value());
        } else {
            STRATEGY_LOG(logger, log_general, "Failed to initialize " + m_indicator->get_name(), LogLevel::ERROR);
        }
        
        return result.has_value();
    }
    
    bool update(const BasicCandle& candle, ILogger* logger) override {
        std::optional<R> result = m_indicator->update(candle);

        if (result.has_value()) {
            // Store the updated value
            m_history.push_back(result.value());
            // Limit the history (optional)
            if (m_history.size() > MAX_HISTORY_SIZE) {
                m_history.pop_front();
            }
            
            STRATEGY_LOG(logger, log_indicator_value, m_indicator->get_name(), result.value());
        } else {
            STRATEGY_LOG(logger, log_general, "Failed to update " + m_indicator->get_name(), LogLevel::ERROR);
        }
        
        return result.has_value();
    }
    
    bool isInitialized() const override {
        return m_indicator->initialized();
    }
    
    const std::string& getName() const override {
        return m_indicator->get_name();
    }

    int getRequiredPeriods() const override {
        return m_indicator->get_required_periods();
    }
    
    std::optional<R> getCurrentValue() const {
        if (m_history.empty()) {
            return std::nullopt;
        }
        return m_history.back();
    }

    std::optional<R> getHistoricalValue(int offset) const {
        if (m_history.empty() || offset >= static_cast<int>(m_history.size())) {
            return std::nullopt;
        }
        
        // Return the value at the specified offset (0 = most recent)
        size_t idx = m_history.size() - 1 - offset;
        return m_history[idx];
    }
    
    // Direct access to the underlying indicator (for compatibility)
    std::shared_ptr<T> getIndicator() const {
        return m_indicator;
    }

    // Reset the indicator to uninitialized state
    void reset() override {
        m_history.clear();
        m_indicator->reset();
    }
};


class IndicatorManager {
private:    
    // Maps to store handlers by parameter type
    std::map<filter::EMAParams, std::unique_ptr<IndicatorHandler<EMA, double>>> m_emaHandlers;
    std::map<filter::RSIParams, std::unique_ptr<IndicatorHandler<RSI, double>>> m_rsiHandlers;
    std::map<filter::StochasticParams, std::unique_ptr<IndicatorHandler<STOCH, std::pair<double, double>>>> m_stochHandlers;
    std::map<filter::ATRParams, std::unique_ptr<IndicatorHandler<ATR, double>>> m_atrHandlers;
    std::map<filter::SuperTrendParams, std::unique_ptr<IndicatorHandler<SUPERTREND, std::pair<double, int>>>> m_supertrendHandlers;
    std::map<filter::CCIParams, std::unique_ptr<IndicatorHandler<CCI, double>>> m_cciHandlers;
    std::map<filter::MACDParams, std::unique_ptr<IndicatorHandler<MACD, MACDResult>>> m_macdHandlers;
    std::map<filter::BBParams, std::unique_ptr<IndicatorHandler<BB, filter::BBResult>>> m_bbHandlers;
    std::map<filter::TimeCyclicParams, std::unique_ptr<IndicatorHandler<TIMECYCLIC, std::pair<double, double>>>> m_timeCyclicHandlers;
    std::map<filter::SwingStructureParams, std::unique_ptr<IndicatorHandler<SWINGSTRUCTURE, filter::SwingStructureResult>>> m_swingStructureHandlers;

    // List of all handlers for bulk operations
    std::vector<IIndicatorHandlerBase*> m_allHandlers;
    
public:

    // Methods for registering indicators by type
    void registerEMA(const filter::EMAParams& params) {
        if (m_emaHandlers.find(params) != m_emaHandlers.end()) {
            return; // Already registered
        }
        
        auto handler = std::make_unique<IndicatorHandler<EMA, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_emaHandlers[params] = std::move(handler);
    }
    
    void registerRSI(const filter::RSIParams& params) {
        if (m_rsiHandlers.find(params) != m_rsiHandlers.end()) {
            return; // Already registered
        }
        
        auto handler = std::make_unique<IndicatorHandler<RSI, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_rsiHandlers[params] = std::move(handler);
    }
    
    void registerStochastic(const filter::StochasticParams& params) {
        if (m_stochHandlers.find(params) != m_stochHandlers.end()) {
            return; // Already registered
        }
        
        auto handler = std::make_unique<IndicatorHandler<STOCH, std::pair<double, double>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_stochHandlers[params] = std::move(handler);
    }
    
    void registerATR(const filter::ATRParams& params) {
        if (m_atrHandlers.find(params) != m_atrHandlers.end()) 
            return; // Already registered
        
        auto handler = std::make_unique<IndicatorHandler<ATR, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_atrHandlers[params] = std::move(handler);
    }
    
    void registerSuperTrend(const filter::SuperTrendParams& params) {
        if (m_supertrendHandlers.find(params) != m_supertrendHandlers.end()) 
            return; // Already registered
        
        auto handler = std::make_unique<IndicatorHandler<SUPERTREND, std::pair<double, int>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_supertrendHandlers[params] = std::move(handler);
    }
    
    void registerCCI(const filter::CCIParams& params) {
        if (m_cciHandlers.find(params) != m_cciHandlers.end()) 
            return; // Already registered
    
        auto handler = std::make_unique<IndicatorHandler<CCI, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_cciHandlers[params] = std::move(handler);
    }

    void registerMACD(const filter::MACDParams& params) {
        if (m_macdHandlers.find(params) != m_macdHandlers.end()) 
            return; // Already registered
        
        auto handler = std::make_unique<IndicatorHandler<MACD, MACDResult>>(params);
        m_allHandlers.push_back(handler.get());
        m_macdHandlers[params] = std::move(handler);
    }

    void registerBB(const filter::BBParams& params) {
        if (m_bbHandlers.find(params) != m_bbHandlers.end()) 
            return; // Already registered
        
        auto handler = std::make_unique<IndicatorHandler<BB, filter::BBResult>>(params);
        m_allHandlers.push_back(handler.get());
        m_bbHandlers[params] = std::move(handler);
    }

    void registerTimeCyclic(const filter::TimeCyclicParams& params = filter::TimeCyclicParams()) {
        if (m_timeCyclicHandlers.find(params) != m_timeCyclicHandlers.end())
            return; // Already registered

        auto handler = std::make_unique<IndicatorHandler<TIMECYCLIC, std::pair<double, double>>>(params);
        m_allHandlers.push_back(handler.get());
        m_timeCyclicHandlers[params] = std::move(handler);
    }

    void registerSwingStructure(const filter::SwingStructureParams& params) {
        if (m_swingStructureHandlers.find(params) != m_swingStructureHandlers.end())
            return; // Already registered

        auto handler = std::make_unique<IndicatorHandler<SWINGSTRUCTURE, filter::SwingStructureResult>>(params);
        m_allHandlers.push_back(handler.get());
        m_swingStructureHandlers[params] = std::move(handler);
    }

    // Méthodes d'accès aux valeurs avec valeurs par défaut
    double getEMAValue(const filter::EMAParams& params, int offset = 0) const {
        auto it = m_emaHandlers.find(params);
        if (it != m_emaHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return 0.0;
    }
    
    double getRSIValue(const filter::RSIParams& params, int offset = 0) const {
        auto it = m_rsiHandlers.find(params);
        if (it != m_rsiHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return 0.0;
    }
    
    std::pair<double, double> getStochasticValue(const filter::StochasticParams& params, int offset = 0) const {
        auto it = m_stochHandlers.find(params);
        if (it != m_stochHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
                
            if (value.has_value()) 
                return value.value();    
        }
        return {0.0, 0.0};
    }
    
    double getATRValue(const filter::ATRParams& params, int offset = 0) const {
        auto it = m_atrHandlers.find(params);
        if (it != m_atrHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return 0.0;
    }
    
    std::pair<double, int> getSuperTrendValue(const filter::SuperTrendParams& params, int offset = 0) const {
        auto it = m_supertrendHandlers.find(params);
        if (it != m_supertrendHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return {0.0, 0};
    }
    
    double getCCIValue(const filter::CCIParams& params, int offset = 0) const {
        auto it = m_cciHandlers.find(params);
        if (it != m_cciHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return 0.0;
    }
    
    MACDResult getMACDValue(const filter::MACDParams& params, int offset = 0) const {
        auto it = m_macdHandlers.find(params);
        if (it != m_macdHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return {0.0, 0.0, 0.0};
    }

    BBResult getBBValue(const filter::BBParams& params, int offset = 0) const {
        auto it = m_bbHandlers.find(params);
        if (it != m_bbHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return BBResult{};
    }

    std::pair<double, double> getTimeCyclicValue(const filter::TimeCyclicParams& params = filter::TimeCyclicParams(), int offset = 0) const {
        auto it = m_timeCyclicHandlers.find(params);
        if (it != m_timeCyclicHandlers.end()) {
            auto value = offset == 0 ?
                it->second->getCurrentValue() :
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return {0.0, 0.0};
    }

    filter::SwingStructureResult getSwingStructureValue(const filter::SwingStructureParams& params, int offset = 0) const {
        auto it = m_swingStructureHandlers.find(params);
        if (it != m_swingStructureHandlers.end()) {
            auto value = offset == 0 ?
                it->second->getCurrentValue() :
                it->second->getHistoricalValue(offset);
            if (value.has_value()) {
                return value.value();
            }
        }
        return filter::SwingStructureResult{};
    }


    // Méthodes d'accès direct aux indicateurs pour compatibilité
    std::shared_ptr<EMA> getEMACalculator(const filter::EMAParams& params) {
        auto it = m_emaHandlers.find(params);
        if (it != m_emaHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<EMA, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<RSI> getRSICalculator(const filter::RSIParams& params) {
        auto it = m_rsiHandlers.find(params);
        if (it != m_rsiHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<RSI, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<STOCH> getStochasticCalculator(const filter::StochasticParams& params) {
        auto it = m_stochHandlers.find(params);
        if (it != m_stochHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<STOCH, std::pair<double, double>>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<ATR> getATRCalculator(const filter::ATRParams& params) {
        auto it = m_atrHandlers.find(params);
        if (it != m_atrHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<ATR, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<SUPERTREND> getSuperTrendCalculator(const filter::SuperTrendParams& params) {
        auto it = m_supertrendHandlers.find(params);
        if (it != m_supertrendHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<SUPERTREND, std::pair<double, int>>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<CCI> getCCICalculator(const filter::CCIParams& params) {
        auto it = m_cciHandlers.find(params);
        if (it != m_cciHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<CCI, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }

    std::shared_ptr<MACD> getMACDCalculator(const filter::MACDParams& params) {
        auto it = m_macdHandlers.find(params);
        if (it != m_macdHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<MACD, MACDResult>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }

    std::shared_ptr<BB> getBBCalculator(const filter::BBParams& params) {
        auto it = m_bbHandlers.find(params);
        if (it != m_bbHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<BB, filter::BBResult>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }

    std::shared_ptr<TIMECYCLIC> getTimeCyclicCalculator(const filter::TimeCyclicParams& params = filter::TimeCyclicParams()) {
        auto it = m_timeCyclicHandlers.find(params);
        if (it != m_timeCyclicHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<TIMECYCLIC, std::pair<double, double>>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }

    std::shared_ptr<SWINGSTRUCTURE> getSwingStructureCalculator(const filter::SwingStructureParams& params) {
        auto it = m_swingStructureHandlers.find(params);
        if (it != m_swingStructureHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<SWINGSTRUCTURE, filter::SwingStructureResult>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }

    // Méthodes de gestion en masse
    bool initializeAll(const std::vector<BasicCandle>& history, ILogger* logger) {
        STRATEGY_LOG(logger, log_general, "Initializing all indicators", LogLevel::DEBUG);
        
        bool success = true;
        for (auto handler : m_allHandlers) {
            bool indSuccess = handler->initialize(history, logger);
            success = success && indSuccess;
        }

        STRATEGY_LOG(logger, log_general, "Indicator initialization: " + 
            std::string(success ? "ALL SUCCESSFULLY INITIALIZED" : "SOME FAILED"), 
            success ? LogLevel::INFO : LogLevel::WARNING);
        
        return success;
    }
    
    bool updateAll(const BasicCandle& candle, ILogger* logger) {
        bool success = true;
        for (auto handler : m_allHandlers) {
            bool indSuccess = handler->update(candle, logger);
            success = success && indSuccess;
        }
        return success;
    }
    
    bool allInitialized() const {
        for (auto handler : m_allHandlers) {
            if (!handler->isInitialized()) {
                return false;
            }
        }
        return true;
    }

    // Reset all indicators to uninitialized state (for new trading day handling)
    void resetAll(ILogger* logger = nullptr) {
        if (logger) STRATEGY_LOG(logger, log_general, "Resetting all indicators for new trading day", LogLevel::INFO);
        for (auto handler : m_allHandlers) handler->reset();
    }
    
    bool needsInitialization() const {
        return !allInitialized() && !m_allHandlers.empty();
    }
    
    std::vector<std::string> getIndicatorNames() const {
        std::vector<std::string> names;
        names.reserve(m_allHandlers.size());
        for (auto handler : m_allHandlers) {
            names.push_back(handler->getName());
        }
        return names;
    }

    int getMaxRequiredPeriods() const {
        int max_period = 0;
        for (auto handler : m_allHandlers) {
            max_period = std::max(max_period, handler->getRequiredPeriods());
        }
        return max_period;
    }
};
