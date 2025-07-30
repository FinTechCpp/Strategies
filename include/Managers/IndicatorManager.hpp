#pragma once

#include "Indicators/IncrementalIndicator.hpp"
#include "Managers/LoggerManager.hpp"

#include <memory>
#include <string>
#include <vector>
#include <type_traits>

class IndicatorManager {
private:
    // Interface pour manipuler les indicateurs sans connaître leur type
    class IIndicatorHandler {
    public:
        virtual ~IIndicatorHandler() = default;
        virtual bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) = 0;
        virtual bool update(const BasicCandle& candle, ILogger* logger) = 0;
        virtual bool isInitialized() const = 0;
        virtual const std::string& getName() const = 0;
        virtual int getRequiredPeriods() const = 0;
    };
    
    // Implémentation typée de l'interface pour chaque indicateur
    template <typename T, typename R>
    class IndicatorHandler : public IIndicatorHandler {
    private:
        std::shared_ptr<T> m_indicator;
        
    public:
        IndicatorHandler(std::shared_ptr<T> indicator) : m_indicator(indicator) {
            static_assert(std::is_base_of_v<IncrementalIndicator<R>, T>, 
                        "T must inherit from IncrementalIndicator<R>");
        }
        
        bool initialize(const std::vector<BasicCandle>& history, ILogger* logger) override {
            if (logger) logger->log_general("Initialisation de " + m_indicator->get_name(), LogLevel::DEBUG);
            
            try {
                R result = m_indicator->initialize_with_history(history);
                bool success = isValidResult(result);
                
                if (success && logger) {
                    logValue(logger, result);
                } else if (!success && logger) {
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
                
                if (success && logger) {
                    logValue(logger, result);
                } else if (!success && logger) {
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
        
    private:
        // Vérifier si le résultat est valide (spécialisations pour différents types)
        bool isValidResult(double value) const {
            return value > 0.0;
        }
        
        bool isValidResult(const std::pair<double, double>& value) const {
            return value.first > 0.0 && value.second > 0.0;
        }
        
        bool isValidResult(const std::pair<double, int>& value) const {
            return value.first > 0.0 && value.second != 0;
        }
        
        // Journaliser la valeur (spécialisations pour différents types)
        void logValue(ILogger* logger, double value) const {
            logger->log_indicator_value(m_indicator->get_name(), value);
        }
        
        void logValue(ILogger* logger, const std::pair<double, double>& value) const {
            logger->log_indicator_value(m_indicator->get_name(), value);
        }
        
        void logValue(ILogger* logger, const std::pair<double, int>& value) const {
            logger->log_indicator_value(m_indicator->get_name(), value);
        }
    };
    
    // Liste des gestionnaires d'indicateurs
    std::vector<std::unique_ptr<IIndicatorHandler>> m_handlers;
    
public:
    /**
     * Enregistre un indicateur à gérer
     * @tparam T Type de l'indicateur (EMA, RSI, STOCH, etc.)
     * @tparam R Type de retour de l'indicateur (double, std::pair<double, double>, etc.)
     * @param indicator Pointeur partagé vers l'indicateur
     */
    template<typename T, typename R>
    void registerIndicator(std::shared_ptr<T> indicator) {
        m_handlers.push_back(std::make_unique<IndicatorHandler<T, R>>(indicator));
    }
    
    /**
     * Initialise tous les indicateurs avec l'historique des bougies
     * @param history Historique des bougies
     * @param logger Logger pour les messages (optionnel)
     * @return true si tous les indicateurs ont été initialisés avec succès
     */
    bool initializeAll(const std::vector<BasicCandle>& history, ILogger* logger = nullptr) {
        if (logger) logger->log_general("Initialisation de tous les indicateurs", LogLevel::DEBUG);
        
        bool success = true;
        for (auto& handler : m_handlers) {
            bool indSuccess = handler->initialize(history, logger);
            success = success && indSuccess;
        }
        
        // Bilan de l'initialisation
        if (logger) {
            logger->log_general("Initialisation des indicateurs: " + 
                std::string(success ? "TOUS INITIALISÉS AVEC SUCCÈS" : "CERTAINS ONT ÉCHOUÉ"), 
                success ? LogLevel::INFO : LogLevel::WARNING);
        }
        
        return success;
    }
    
    /**
     * Met à jour tous les indicateurs avec une nouvelle bougie
     * @param candle Nouvelle bougie
     * @param logger Logger pour les messages (optionnel)
     * @return true si tous les indicateurs ont été mis à jour avec succès
     */
    bool updateAll(const BasicCandle& candle, ILogger* logger = nullptr) {
        bool success = true;
        for (auto& handler : m_handlers) {
            bool indSuccess = handler->update(candle, logger);
            success = success && indSuccess;
        }
        return success;
    }
    
    /**
     * Vérifie si tous les indicateurs sont initialisés
     */
    bool allInitialized() const {
        for (const std::unique_ptr<IndicatorManager::IIndicatorHandler>& handler : m_handlers) {
            if (!handler->isInitialized()) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * Vérifie si des indicateurs ont besoin d'initialisation
     */
    bool needsInitialization() const {
        return !allInitialized() && !m_handlers.empty();
    }
    
    /**
     * Récupère la liste des noms des indicateurs enregistrés
     */
    std::vector<std::string> getIndicatorNames() const {
        std::vector<std::string> names;
        names.reserve(m_handlers.size());
        for (const auto& handler : m_handlers) {
            names.push_back(handler->getName());
        }
        return names;
    }

    /**
     * Retourne le nombre maximum de périodes nécessaires pour initialiser
     * tous les indicateurs enregistrés
     */
    int getMaxRequiredPeriods() const {
        int max_period = 0;
        for (const auto& handler : m_handlers) {
            max_period = std::max(max_period, handler->getRequiredPeriods());
        }
        return max_period;
    }
};