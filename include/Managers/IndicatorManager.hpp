#pragma once

#include "Indicators/IncrementalIndicator.hpp"
#include "Managers/LoggerManager.hpp"
#include "Indicators/indicators.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <type_traits>
#include <variant>


// Type pour stocker différentes valeurs d'indicateurs
using IndicatorValue = std::variant<
    double,                      // Pour EMA, RSI, ATR
    std::pair<double, double>,   // Pour Stochastic (K, D)
    std::pair<double, int>       // Pour SuperTrend (valeur, direction)
>;

class IndicatorManager {
private:
    // Interface commune pour tous les gestionnaires d'indicateurs
    class IIndicatorHandler {
    public:
        virtual ~IIndicatorHandler() = default;
        virtual bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) = 0;
        virtual bool update(const BasicCandle& candle, ILogger* logger) = 0;
        virtual bool isInitialized() const = 0;
        virtual const std::string& getName() const = 0;
        virtual int getRequiredPeriods() const = 0;
        virtual IndicatorValue getCurrentValue() const = 0;
        virtual IndicatorValue getHistoricalValue(int offset) const = 0;
    };
    
    // Gestionnaire typé pour chaque type d'indicateur
    template<typename T, typename R>
    class IndicatorHandler : public IIndicatorHandler {
    private:
        std::shared_ptr<T> m_indicator;
        std::vector<R> m_history; // Stockage des valeurs historiques
        
    public:
        template<typename... Args>
        IndicatorHandler(Args&&... args) {
            m_indicator = std::make_shared<T>(std::forward<Args>(args)...);
        }
        
        bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) override {
            if (logger) logger->log_general("Initialisation de " + m_indicator->get_name(), LogLevel::DEBUG);
            
            try {
                R result = m_indicator->initialize_with_history(history);
                bool success = isValidResult(result);
                
                if (success) {
                    // Stocker la valeur initiale
                    m_history.clear();
                    m_history.push_back(result);
                    
                    if (logger) {
                        logger->log_indicator_value(m_indicator->get_name(), result);
                    }
                } else if (logger) {
                    logger->log_general("Échec de l'initialisation de " + m_indicator->get_name(), LogLevel::ERROR);
                }
                
                return success;
            }
            catch (const std::exception& e) {
                if (logger) {
                    logger->log_general("Exception lors de l'initialisation de " + m_indicator->get_name() + ": " + e.what(), LogLevel::ERROR);
                }
                return false;
            }
        }
        
        bool update(const BasicCandle& candle, ILogger* logger) override {
            try {
                R result = m_indicator->update(candle);
                bool success = isValidResult(result);
                
                if (success) {
                    // Stocker la valeur mise à jour
                    m_history.push_back(result);
                    // Limiter l'historique (optionnel)
                    if (m_history.size() > 100) {
                        m_history.erase(m_history.begin());
                    }
                    
                    if (logger) {
                        logger->log_indicator_value(m_indicator->get_name(), result);
                    }
                } else if (logger) {
                    logger->log_general("Échec de la mise à jour de " + m_indicator->get_name(), LogLevel::ERROR);
                }
                
                return success;
            }
            catch (const std::exception& e) {
                if (logger) {
                    logger->log_general("Exception lors de la mise à jour de " + m_indicator->get_name() + ": " + e.what(), LogLevel::ERROR);
                }
                return false;
            }
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
        
        IndicatorValue getCurrentValue() const override {
            if (m_history.empty()) {
                // Valeur par défaut selon le type
                if constexpr (std::is_same_v<R, double>) {
                    return 0.0;
                } else if constexpr (std::is_same_v<R, std::pair<double, double>>) {
                    return std::make_pair(0.0, 0.0);
                } else if constexpr (std::is_same_v<R, std::pair<double, int>>) {
                    return std::make_pair(0.0, 0);
                }
            }
            return m_history.back();
        }
        
        IndicatorValue getHistoricalValue(int offset) const override {
            if (m_history.empty() || offset >= static_cast<int>(m_history.size())) {
                // Valeur par défaut selon le type
                if constexpr (std::is_same_v<R, double>) {
                    return 0.0;
                } else if constexpr (std::is_same_v<R, std::pair<double, double>>) {
                    return std::make_pair(0.0, 0.0);
                } else if constexpr (std::is_same_v<R, std::pair<double, int>>) {
                    return std::make_pair(0.0, 0);
                }
            }
            
            // Retourner la valeur à l'offset spécifié (0 = plus récent)
            size_t idx = m_history.size() - 1 - offset;
            return m_history[idx];
        }
        
        // Accès direct à l'indicateur sous-jacent (pour compatibilité)
        std::shared_ptr<T> getIndicator() const {
            return m_indicator;
        }
        
    private:
        // Vérifier si le résultat est valide
        bool isValidResult(double value) const {
            return value > 0.0 || value <= 0.0; // Accepte toutes les valeurs
        }
        
        bool isValidResult(const std::pair<double, double>& value) const {
            return true;
        }
        
        bool isValidResult(const std::pair<double, int>& value) const {
            return true;
        }
    };
    
    // Maps pour stocker les handlers par type de paramètres
    std::map<EMAParams, std::unique_ptr<IIndicatorHandler>> m_emaHandlers;
    std::map<RSIParams, std::unique_ptr<IIndicatorHandler>> m_rsiHandlers;
    std::map<StochasticParams, std::unique_ptr<IIndicatorHandler>> m_stochHandlers;
    std::map<ATRParams, std::unique_ptr<IIndicatorHandler>> m_atrHandlers;
    std::map<SuperTrendParams, std::unique_ptr<IIndicatorHandler>> m_supertrendHandlers;
    
    // Liste de tous les handlers pour les opérations en masse
    std::vector<IIndicatorHandler*> m_allHandlers;
    
    // Fonction utilitaire pour extraire une valeur simple
    static double extractValue(const IndicatorValue& value, bool isSecond = false) {
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } 
        else if (std::holds_alternative<std::pair<double, double>>(value)) {
            auto& pair = std::get<std::pair<double, double>>(value);
            return isSecond ? pair.second : pair.first;
        } 
        else if (std::holds_alternative<std::pair<double, int>>(value)) {
            auto& pair = std::get<std::pair<double, int>>(value);
            return isSecond ? static_cast<double>(pair.second) : pair.first;
        }
        return 0.0;
    }
    
public:
    // Méthodes d'enregistrement par type d'indicateur
    void registerEMA(const EMAParams& params) {
        if (m_emaHandlers.find(params) != m_emaHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<EMA, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_emaHandlers[params] = std::move(handler);
    }
    
    void registerRSI(const RSIParams& params) {
        if (m_rsiHandlers.find(params) != m_rsiHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<RSI, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_rsiHandlers[params] = std::move(handler);
    }
    
    void registerStochastic(const StochasticParams& params) {
        if (m_stochHandlers.find(params) != m_stochHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<STOCH, std::pair<double, double>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_stochHandlers[params] = std::move(handler);
    }
    
    void registerATR(const ATRParams& params) {
        if (m_atrHandlers.find(params) != m_atrHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<ATR, double>>(params);
        m_allHandlers.push_back(handler.get());
        m_atrHandlers[params] = std::move(handler);
    }
    
    void registerSuperTrend(const SuperTrendParams& params) {
        if (m_supertrendHandlers.find(params) != m_supertrendHandlers.end()) {
            return; // Déjà enregistré
        }
        
        auto handler = std::make_unique<IndicatorHandler<SUPERTREND, std::pair<double, int>>>(
            params);
        m_allHandlers.push_back(handler.get());
        m_supertrendHandlers[params] = std::move(handler);
    }
    
    // Méthodes d'accès aux valeurs
    double getEMAValue(const EMAParams& params, int offset = 0) const {
        auto it = m_emaHandlers.find(params);
        if (it != m_emaHandlers.end()) {
            return extractValue(offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset));
        }
        return 0.0;
    }
    
    double getRSIValue(const RSIParams& params, int offset = 0) const {
        auto it = m_rsiHandlers.find(params);
        if (it != m_rsiHandlers.end()) {
            return extractValue(offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset));
        }
        return 0.0;
    }
    
    std::pair<double, double> getStochasticValue(const StochasticParams& params, int offset = 0) const {
        auto it = m_stochHandlers.find(params);
        if (it != m_stochHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
                
            if (std::holds_alternative<std::pair<double, double>>(value)) {
                return std::get<std::pair<double, double>>(value);
            }
        }
        return {0.0, 0.0};
    }
    
    double getATRValue(const ATRParams& params, int offset = 0) const {
        auto it = m_atrHandlers.find(params);
        if (it != m_atrHandlers.end()) {
            return extractValue(offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset));
        }
        return 0.0;
    }
    
    std::pair<double, int> getSuperTrendValue(const SuperTrendParams& params, int offset = 0) const {
        auto it = m_supertrendHandlers.find(params);
        if (it != m_supertrendHandlers.end()) {
            auto value = offset == 0 ? 
                it->second->getCurrentValue() : 
                it->second->getHistoricalValue(offset);
                
            if (std::holds_alternative<std::pair<double, int>>(value)) {
                return std::get<std::pair<double, int>>(value);
            }
        }
        return {0.0, 0};
    }
    
    // Méthodes d'accès direct aux indicateurs pour compatibilité
    std::shared_ptr<EMA> getEMACalculator(const EMAParams& params) {
        auto it = m_emaHandlers.find(params);
        if (it != m_emaHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<EMA, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<RSI> getRSICalculator(const RSIParams& params) {
        auto it = m_rsiHandlers.find(params);
        if (it != m_rsiHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<RSI, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<STOCH> getStochasticCalculator(const StochasticParams& params) {
        auto it = m_stochHandlers.find(params);
        if (it != m_stochHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<STOCH, std::pair<double, double>>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<ATR> getATRCalculator(const ATRParams& params) {
        auto it = m_atrHandlers.find(params);
        if (it != m_atrHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<ATR, double>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    std::shared_ptr<SUPERTREND> getSuperTrendCalculator(const SuperTrendParams& params) {
        auto it = m_supertrendHandlers.find(params);
        if (it != m_supertrendHandlers.end()) {
            auto* handler = dynamic_cast<IndicatorHandler<SUPERTREND, std::pair<double, int>>*>(it->second.get());
            if (handler) {
                return handler->getIndicator();
            }
        }
        return nullptr;
    }
    
    // Méthodes de gestion en masse
    bool initializeAll(const std::vector<BasicCandle>& history, ILogger* logger = nullptr) {
        if (logger) logger->log_general("Initialisation de tous les indicateurs", LogLevel::DEBUG);
        
        bool success = true;
        for (auto handler : m_allHandlers) {
            bool indSuccess = handler->initialize(history, logger);
            success = success && indSuccess;
        }
        
        if (logger) {
            logger->log_general("Initialisation des indicateurs: " + 
                std::string(success ? "TOUS INITIALISÉS AVEC SUCCÈS" : "CERTAINS ONT ÉCHOUÉ"), 
                success ? LogLevel::INFO : LogLevel::WARNING);
        }
        
        return success;
    }
    
    bool updateAll(const BasicCandle& candle, ILogger* logger = nullptr) {
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