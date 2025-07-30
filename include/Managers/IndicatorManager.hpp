#pragma once

#include "Indicators/indicators.hpp"
#include "Managers/LoggerManager.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <typeindex>
#include <any>

class IndicatorManager {
public:
    using IndicatorPtr = std::shared_ptr<void>;
    using IndicatorId = std::string;

    template<typename T>
    IndicatorId registerIndicator(const std::string& name, const std::shared_ptr<T>& indicator) {
        const std::string id = generateId<T>(name);
        m_indicators[id] = indicator;
        m_updateFunctions[id] = [indicator](const BasicCandle& candle) {
            return indicator->update(candle);
        };
        m_initFunctions[id] = [indicator](const std::vector<BasicCandle>& history) {
            return indicator->initialize_with_history(history);
        };
        m_typeMap[id] = std::type_index(typeid(T));
        
        // Stocker les valeurs historiques si nécessaire
        m_historySize[id] = 1;  // Par défaut, ne garde que la valeur actuelle
        m_values[id] = std::vector<std::any>{};
        
        return id;
    }
    
    // Définir la taille de l'historique pour un indicateur
    void setHistorySize(const IndicatorId& id, size_t size) {
        if (m_historySize.find(id) != m_historySize.end()) {
            m_historySize[id] = size;
        }
    }
    
    // Initialise tous les indicateurs avec l'historique
    bool initializeAll(const std::vector<BasicCandle>& history, ILogger* logger = nullptr) {
        bool success = true;
        for (auto& [id, _] : m_indicators) {
            if (logger) logger->log_general("Initialisation de " + id, LogLevel::DEBUG);
            bool indSuccess = initializeIndicator(id, history);
            if (!indSuccess && logger) logger->log_general("Échec de l'initialisation de " + id, LogLevel::ERROR);
            success = success && indSuccess;
        }
        return success;
    }
    
    // Initialise un indicateur spécifique
    bool initializeIndicator(const IndicatorId& id, const std::vector<BasicCandle>& history) {
        if (m_initFunctions.find(id) == m_initFunctions.end()) return false;
        
        auto result = m_initFunctions[id](history);
        
        // Stocker le résultat dans la liste des valeurs
        storeValue(id, result);
        
        return true;
    }
    
    // Met à jour tous les indicateurs avec une nouvelle bougie
    bool updateAll(const BasicCandle& candle, ILogger* logger = nullptr) {
        bool success = true;
        for (auto& [id, _] : m_indicators) {
            bool indSuccess = updateIndicator(id, candle);
            if (!indSuccess && logger) logger->log_general("Échec de la mise à jour de " + id, LogLevel::ERROR);
            success = success && indSuccess;
        }
        return success;
    }
    
    // Met à jour un indicateur spécifique
    bool updateIndicator(const IndicatorId& id, const BasicCandle& candle) {
        if (m_updateFunctions.find(id) == m_updateFunctions.end()) return false;
        
        auto result = m_updateFunctions[id](candle);
        
        // Décaler l'historique et stocker la nouvelle valeur
        shiftAndStoreValue(id, result);
        
        return true;
    }
    
    // Récupère la valeur actuelle d'un indicateur
    template<typename T>
    T getValue(const IndicatorId& id) const {
        if (m_values.find(id) == m_values.end() || m_values.at(id).empty()) {
            throw std::runtime_error("Indicateur non initialisé ou pas de valeurs disponibles: " + id);
        }
        
        try {
            return std::any_cast<T>(m_values.at(id)[0]);
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error("Type incompatible pour l'indicateur: " + id);
        }
    }
    
    // Récupère l'historique des valeurs d'un indicateur
    template<typename T>
    std::vector<T> getHistory(const IndicatorId& id) const {
        if (m_values.find(id) == m_values.end()) {
            throw std::runtime_error("Indicateur non trouvé: " + id);
        }
        
        const auto& anyValues = m_values.at(id);
        std::vector<T> result;
        result.reserve(anyValues.size());
        
        for (const auto& value : anyValues) {
            try {
                result.push_back(std::any_cast<T>(value));
            } catch (const std::bad_any_cast& e) {
                throw std::runtime_error("Type incompatible dans l'historique de l'indicateur: " + id);
            }
        }
        
        return result;
    }
    
    // Vérifie si un indicateur est initialisé
    bool isInitialized(const IndicatorId& id) const {
        if (m_indicators.find(id) == m_indicators.end()) return false;
        return !m_values.at(id).empty();
    }
    
    // Génère un ID unique pour un indicateur
    template<typename T>
    static std::string generateId(const std::string& name) {
        return name.empty() ? typeid(T).name() : name;
    }
    
    // Crée et enregistre un indicateur EMA
    IndicatorId createEMA(int period, const std::string& name = "") {
        std::string actualName = name.empty() ? "EMA_" + std::to_string(period) : name;
        auto ema = std::make_shared<EMA>(period, actualName);
        return registerIndicator<EMA>(actualName, ema);
    }
    
    // Crée et enregistre un indicateur RSI
    IndicatorId createRSI(int period, const std::string& name = "") {
        std::string actualName = name.empty() ? "RSI_" + std::to_string(period) : name;
        auto rsi = std::make_shared<RSI>(period, actualName);
        return registerIndicator<RSI>(actualName, rsi);
    }
    
    // Crée et enregistre un indicateur STOCH
    IndicatorId createSTOCH(int fastK, int slowK, int slowD, const std::string& name = "") {
        std::string actualName = name.empty() ? 
            "STOCH_" + std::to_string(fastK) + "_" + std::to_string(slowK) + "_" + std::to_string(slowD) : name;
        auto stoch = std::make_shared<STOCH>(fastK, slowK, slowD, actualName);
        return registerIndicator<STOCH>(actualName, stoch);
    }
    
    // Crée et enregistre un indicateur ATRLOG
    IndicatorId createATRLOG(int period, const std::string& name = "") {
        std::string actualName = name.empty() ? "ATRLOG_" + std::to_string(period) : name;
        auto atrlog = std::make_shared<ATRLOG>(period, actualName);
        return registerIndicator<ATRLOG>(actualName, atrlog);
    }
    
    // Crée et enregistre un indicateur SUPERTREND
    IndicatorId createSUPERTREND(int atrPeriod, double multiplier, const std::string& name = "") {
        std::string actualName = name.empty() ? 
            "SUPERTREND_" + std::to_string(atrPeriod) + "_" + std::to_string(int(multiplier)) : name;
        auto supertrend = std::make_shared<SUPERTREND>(atrPeriod, multiplier, actualName);
        return registerIndicator<SUPERTREND>(actualName, supertrend);
    }
    
private:
    // Stockage des indicateurs
    std::unordered_map<IndicatorId, IndicatorPtr> m_indicators;
    
    // Fonction de mise à jour pour chaque indicateur
    std::unordered_map<IndicatorId, std::function<std::any(const BasicCandle&)>> m_updateFunctions;
    
    // Fonction d'initialisation pour chaque indicateur
    std::unordered_map<IndicatorId, std::function<std::any(const std::vector<BasicCandle>&)>> m_initFunctions;
    
    // Type de chaque indicateur
    std::unordered_map<IndicatorId, std::type_index> m_typeMap;
    
    // Valeurs actuelles et historiques
    std::unordered_map<IndicatorId, std::vector<std::any>> m_values;
    
    // Taille de l'historique à conserver
    std::unordered_map<IndicatorId, size_t> m_historySize;
    
    // Stocke une nouvelle valeur (remplace la valeur actuelle)
    void storeValue(const IndicatorId& id, const std::any& value) {
        m_values[id].clear();
        m_values[id].push_back(value);
    }
    
    // Décale l'historique et stocke une nouvelle valeur
    void shiftAndStoreValue(const IndicatorId& id, const std::any& value) {
        if (m_values.find(id) == m_values.end()) {
            m_values[id] = std::vector<std::any>{value};
            return;
        }
        
        auto& values = m_values[id];
        const size_t historySize = m_historySize[id];
        
        // Décaler les valeurs
        if (values.size() >= historySize) {
            values.resize(historySize);
            for (size_t i = historySize - 1; i > 0; --i) {
                values[i] = values[i-1];
            }
        } else {
            values.push_back(std::any());
            for (size_t i = values.size() - 1; i > 0; --i) {
                values[i] = values[i-1];
            }
        }
        
        // Stocker la nouvelle valeur
        values[0] = value;
    }
};