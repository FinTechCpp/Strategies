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

// Interface commune pour tous les gestionnaires d'indicateurs
class IIndicatorHandlerBase {
public:
    virtual ~IIndicatorHandlerBase() = default;
    virtual bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) = 0;
    virtual bool update(const BasicCandle& candle, ILogger* logger) = 0;
    virtual bool isInitialized() const = 0;
    virtual const std::string& getName() const = 0;
    virtual int getRequiredPeriods() const = 0;
};

// Gestionnaire typé pour chaque type d'indicateur
template<typename T, typename R>
class IndicatorHandler : public IIndicatorHandlerBase {
private:
    std::shared_ptr<T> m_indicator;
    std::deque<R> m_history; // Stockage des valeurs historiques
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    
public:
    template<typename... Args>
    IndicatorHandler(Args&&... args) {
        m_indicator = std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) override {
        STRATEGY_LOG(logger, log_general, "Initialisation de " + m_indicator->get_name(), LogLevel::DEBUG);
        
        std::optional<R> result = m_indicator->initialize_with_history(history);
        
        if (result.has_value()) {
            // Stocker la valeur initiale
            m_history.clear();
            m_history.push_back(result.value());

            STRATEGY_LOG(logger, log_indicator_value, m_indicator->get_name(), result.value());
        } else {
            STRATEGY_LOG(logger, log_general, "Échec de l'initialisation de " + m_indicator->get_name(), LogLevel::ERROR);
        }
        
        return result.has_value();
    }
    
    bool update(const BasicCandle& candle, ILogger* logger) override {
        std::optional<R> result = m_indicator->update(candle);

        if (result.has_value()) {
            // Stocker la valeur mise à jour
            m_history.push_back(result.value());
            // Limiter l'historique (optionnel)
            if (m_history.size() > MAX_HISTORY_SIZE) {
                m_history.pop_front();
            }
            
            STRATEGY_LOG(logger, log_indicator_value, m_indicator->get_name(), result.value());
        } else {
            STRATEGY_LOG(logger, log_general, "Échec de la mise à jour de " + m_indicator->get_name(), LogLevel::ERROR);
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
        
        // Retourner la valeur à l'offset spécifié (0 = plus récent)
        size_t idx = m_history.size() - 1 - offset;
        return m_history[idx];
    }
    
    // Accès direct à l'indicateur sous-jacent (pour compatibilité)
    std::shared_ptr<T> getIndicator() const {
        return m_indicator;
    }
};


class IndicatorManager {
private:    
    // Maps pour stocker les handlers par type de paramètres
    std::map<filter::EMAParams, std::unique_ptr<IndicatorHandler<EMA, double>>> m_emaHandlers;
    std::map<filter::RSIParams, std::unique_ptr<IndicatorHandler<RSI, double>>> m_rsiHandlers;
    std::map<filter::StochasticParams, std::unique_ptr<IndicatorHandler<STOCH, std::pair<double, double>>>> m_stochHandlers;
    std::map<filter::ATRParams, std::unique_ptr<IndicatorHandler<ATR, double>>> m_atrHandlers;
    std::map<filter::SuperTrendParams, std::unique_ptr<IndicatorHandler<SUPERTREND, std::pair<double, int>>>> m_supertrendHandlers;
    std::map<filter::CCIParams, std::unique_ptr<IndicatorHandler<CCI, double>>> m_cciHandlers;
    std::map<filter::MACDParams, std::unique_ptr<IndicatorHandler<MACD, MACDResult>>> m_macdHandlers;
    std::map<filter::BBParams, std::unique_ptr<IndicatorHandler<BB, filter::BBResult>>> m_bbHandlers;
    std::map<filter::TimeCyclicParams, std::unique_ptr<IndicatorHandler<TIMECYCLIC, std::pair<double, double>>>> m_timeCyclicHandlers;

    // Liste de tous les handlers pour les opérations en masse
    std::vector<IIndicatorHandlerBase*> m_allHandlers;
    
public:

    // template<typename IndicatorT, typename ParamT, typename ReturnT>
    // void registerIndicator(const ParamT& params) {
    //     auto& handlers = getHandlerMapImpl<IndicatorT, ParamT, ReturnT>();

    //     if (handlers.find(params) != handlers.end()) {
    //         return; // Déjà enregistré
    //     }

    //     auto handler = std::make_unique<IndicatorHandler<IndicatorT, ReturnT>>(params);
    //     m_allHandlers.push_back(handler.get());
    //     handlers[params] = std::move(handler);
    // }

    // // Méthode générique d'accès aux valeurs
    // template<typename IndicatorT, typename ParamT, typename ReturnT>
    // std::optional<ReturnT> getIndicatorValue(const ParamT& params, int offset = 0) const {
    //     const auto& handlers = getHandlerMapImpl<IndicatorT, ParamT, ReturnT>();
        
    //     auto it = handlers.find(params);
    //     if (it != handlers.end()) {
    //         return offset == 0 ? 
    //             it->second->getCurrentValue() : 
    //             it->second->getHistoricalValue(offset);
    //     }
    //     return std::nullopt;
    // }

    // // helper interne (implémentation)
    // template<typename IndicatorT, typename ParamT, typename ReturnT>
    // auto& getHandlerMapImpl() {
    //     if constexpr (std::is_same_v<IndicatorT, EMA> &&
    //                   std::is_same_v<ParamT, filter::EMAParams> &&
    //                   std::is_same_v<ReturnT, double>) {
    //         return m_emaHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, RSI> &&
    //                          std::is_same_v<ParamT, filter::RSIParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_rsiHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, STOCH> &&
    //                          std::is_same_v<ParamT, filter::StochasticParams> &&
    //                          std::is_same_v<ReturnT, std::pair<double, double>>) {
    //         return m_stochHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, ATR> &&
    //                          std::is_same_v<ParamT, filter::ATRParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_atrHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, SUPERTREND> &&
    //                          std::is_same_v<ParamT, filter::SuperTrendParams> &&
    //                          std::is_same_v<ReturnT, std::pair<double, int>>) {
    //         return m_supertrendHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, CCI> &&
    //                          std::is_same_v<ParamT, filter::CCIParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_cciHandlers;
    //     } else {
    //         static_assert(always_false_v<IndicatorT>, "getHandlerMap: combinaison IndicatorT/ParamT/ReturnT non supportée");
    //     }
    // }

    // template<typename IndicatorT, typename ParamT, typename ReturnT>
    // const auto& getHandlerMapImpl() const {
    //     if constexpr (std::is_same_v<IndicatorT, EMA> &&
    //                   std::is_same_v<ParamT, filter::EMAParams> &&
    //                   std::is_same_v<ReturnT, double>) {
    //         return m_emaHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, RSI> &&
    //                          std::is_same_v<ParamT, filter::RSIParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_rsiHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, STOCH> &&
    //                          std::is_same_v<ParamT, filter::StochasticParams> &&
    //                          std::is_same_v<ReturnT, std::pair<double, double>>) {
    //         return m_stochHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, ATR> &&
    //                          std::is_same_v<ParamT, filter::ATRParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_atrHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, SUPERTREND> &&
    //                          std::is_same_v<ParamT, filter::SuperTrendParams> &&
    //                          std::is_same_v<ReturnT, std::pair<double, int>>) {
    //         return m_supertrendHandlers;
    //     } else if constexpr (std::is_same_v<IndicatorT, CCI> &&
    //                          std::is_same_v<ParamT, filter::CCIParams> &&
    //                          std::is_same_v<ReturnT, double>) {
    //         return m_cciHandlers;
    //     } else {
    //         static_assert(always_false_v<IndicatorT>, "getHandlerMap (const): combinaison IndicatorT/ParamT/ReturnT non supportée");
    //     }
    // }
    


    // Méthodes d'enregistrement par type d'indicateur
    void registerEMA(const filter::EMAParams& params) {
        if (m_emaHandlers.find(params) != m_emaHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<EMA, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_emaHandlers[params] = std::move(handler);
    }
    
    void registerRSI(const filter::RSIParams& params) {
        if (m_rsiHandlers.find(params) != m_rsiHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<RSI, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_rsiHandlers[params] = std::move(handler);
    }
    
    void registerStochastic(const filter::StochasticParams& params) {
        if (m_stochHandlers.find(params) != m_stochHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<STOCH, std::pair<double, double>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_stochHandlers[params] = std::move(handler);
    }
    
    void registerATR(const filter::ATRParams& params) {
        if (m_atrHandlers.find(params) != m_atrHandlers.end()) 
            return; // Déjà enregistré
        
        auto handler = std::make_unique<IndicatorHandler<ATR, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_atrHandlers[params] = std::move(handler);
    }
    
    void registerSuperTrend(const filter::SuperTrendParams& params) {
        if (m_supertrendHandlers.find(params) != m_supertrendHandlers.end()) 
            return; // Déjà enregistré
        
        auto handler = std::make_unique<IndicatorHandler<SUPERTREND, std::pair<double, int>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_supertrendHandlers[params] = std::move(handler);
    }
    
    void registerCCI(const filter::CCIParams& params) {
        if (m_cciHandlers.find(params) != m_cciHandlers.end()) 
            return; // Déjà enregistré
    
        auto handler = std::make_unique<IndicatorHandler<CCI, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_cciHandlers[params] = std::move(handler);
    }

    void registerMACD(const filter::MACDParams& params) {
        if (m_macdHandlers.find(params) != m_macdHandlers.end()) 
            return; // Déjà enregistré
        
        auto handler = std::make_unique<IndicatorHandler<MACD, MACDResult>>(params);
        m_allHandlers.push_back(handler.get());
        m_macdHandlers[params] = std::move(handler);
    }

    void registerBB(const filter::BBParams& params) {
        if (m_bbHandlers.find(params) != m_bbHandlers.end()) 
            return; // Déjà enregistré
        
        auto handler = std::make_unique<IndicatorHandler<BB, filter::BBResult>>(params);
        m_allHandlers.push_back(handler.get());
        m_bbHandlers[params] = std::move(handler);
    }

    void registerTimeCyclic(const filter::TimeCyclicParams& params = filter::TimeCyclicParams()) {
        if (m_timeCyclicHandlers.find(params) != m_timeCyclicHandlers.end()) 
            return; // Déjà enregistré
        
        auto handler = std::make_unique<IndicatorHandler<TIMECYCLIC, std::pair<double, double>>>(params);
        m_allHandlers.push_back(handler.get());
        m_timeCyclicHandlers[params] = std::move(handler);
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
    
    // Méthodes de gestion en masse
    bool initializeAll(const std::vector<BasicCandle>& history, ILogger* logger) {
        STRATEGY_LOG(logger, log_general, "Initialisation de tous les indicateurs", LogLevel::DEBUG);
        
        bool success = true;
        for (auto handler : m_allHandlers) {
            bool indSuccess = handler->initialize(history, logger);
            success = success && indSuccess;
        }

        STRATEGY_LOG(logger, log_general, "Initialisation des indicateurs: " + 
            std::string(success ? "TOUS INITIALISÉS AVEC SUCCÈS" : "CERTAINS ONT ÉCHOUÉ"), 
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