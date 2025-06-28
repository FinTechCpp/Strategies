#pragma once
#include "common.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>

class CandleManager {
private:
    // Buffer simple des bougies de base
    std::vector<BasicCandle> candle_buffer;
    
    // Cache de bougies Heikin-Ashi
    std::vector<BasicCandle> ha_buffer;
    
    // Paramètres de gestion du buffer
    size_t max_buffer_size = 200;      // Taille maximale normale
    size_t hysteresis_threshold = 250; // Seuil déclenchant le nettoyage
    size_t clean_target_size = 180;    // Taille cible après nettoyage
    
    // Ajouter une bougie Heikin-Ashi de manière incrémentale
    void add_heikin_ashi_candle(const BasicCandle& current) {
        // Si c'est la première bougie, elle est identique à la bougie normale
        if (ha_buffer.empty()) {
            ha_buffer.push_back(current);
            return;
        }
        
        // Récupérer la dernière bougie HA
        const BasicCandle& prev_ha = ha_buffer.back();
        
        // Calculer la nouvelle bougie HA
        double ha_close = (current.open + current.high + current.low + current.close) / 4.0;
        double ha_open = (prev_ha.open + prev_ha.close) / 2.0;
        double ha_high = std::max({current.high, ha_open, ha_close});
        double ha_low = std::min({current.low, ha_open, ha_close});
        
        // Créer et ajouter la nouvelle bougie HA
        BasicCandle new_ha(current.date, ha_open, ha_high, ha_low, ha_close);
        ha_buffer.push_back(new_ha);
    }
    
    // Convertir BasicCandle en Candle (pour compatibilité si nécessaire)
    Candle to_candle(const BasicCandle& basic) const {
        Candle candle(basic.date, basic.open, basic.high, basic.low, basic.close);
        return candle;
    }
    
    // Nettoyer les buffers si nécessaire (logique d'hystérésis)
    void check_and_clean_buffers() {
        if (candle_buffer.size() > hysteresis_threshold) {
            // Calculer combien de bougies supprimer
            size_t to_remove = candle_buffer.size() - clean_target_size;
            
            // Supprimer du début du buffer
            candle_buffer.erase(candle_buffer.begin(), candle_buffer.begin() + to_remove);
            
            // Aligner le buffer HA
            if (ha_buffer.size() > to_remove) {
                ha_buffer.erase(ha_buffer.begin(), ha_buffer.begin() + to_remove);
            } else {
                // Si on a supprimé plus de bougies qu'il n'y en a dans ha_buffer,
                // on doit recalculer tout le buffer HA
                recalculate_all_heikin_ashi();
            }
        }
    }
    
    // Recalculer tout le buffer HA (appelé seulement si nécessaire)
    void recalculate_all_heikin_ashi() {
        if (candle_buffer.empty()) {
            ha_buffer.clear();
            return;
        }
        
        ha_buffer.clear();
        ha_buffer.reserve(candle_buffer.size());
        
        // Calculer les bougies HA suivantes de manière incrémentale
        for (size_t i = 0; i < candle_buffer.size(); ++i) {
            add_heikin_ashi_candle(candle_buffer[i]);
        }
    }

public:
    // Constructeur avec paramètres de gestion du buffer
    CandleManager(size_t max_size = 200, size_t threshold = 250, size_t target = 180) 
        : max_buffer_size(max_size), hysteresis_threshold(threshold), clean_target_size(target) {
            candle_buffer.reserve(max_buffer_size);
            ha_buffer.reserve(max_buffer_size);
        }
    
    // Ajouter une nouvelle bougie (BasicCandle)
    void add_candle(const BasicCandle& candle) {
        // Nouvelle bougie
        candle_buffer.push_back(candle);
        
        // Calcul incrémental pour la nouvelle bougie
        add_heikin_ashi_candle(candle);
        
        // Vérifier et nettoyer les buffers si nécessaire
        check_and_clean_buffers();
    }
    
    // Récupérer la dernière bougie
    BasicCandle get_latest_candle() const {
        if (candle_buffer.empty()) {
            throw std::runtime_error("No candles available");
        }
        return candle_buffer.back();
    }
    
    // Récupérer les N dernières bougies
    std::vector<BasicCandle> get_last_candles(size_t n) const {
        if (candle_buffer.empty()) {
            return {};
        }
        
        size_t count = std::min(n, candle_buffer.size());
        
        return std::vector<BasicCandle>(
            candle_buffer.end() - count, 
            candle_buffer.end()
        );
    }
    
    // Récupérer la dernière bougie Heikin-Ashi
    BasicCandle get_latest_heikin_ashi() const {
        if (ha_buffer.empty()) {
            throw std::runtime_error("No Heikin-Ashi candles available");
        }
        
        return ha_buffer.back();
    }
    
    // Récupérer les N dernières bougies Heikin-Ashi
    std::vector<BasicCandle> get_last_heikin_ashi_candles(size_t n) const {
        if (ha_buffer.empty()) {
            return {};
        }
        
        size_t count = std::min(n, ha_buffer.size());
        
        return std::vector<BasicCandle>(
            ha_buffer.end() - count, 
            ha_buffer.end()
        );
    }
    
    // Vérifier si une bougie est verte
    bool is_candle_green(const BasicCandle& candle) const {
        return candle.close > candle.open;
    }
    
    // Vérifier si la dernière bougie est verte
    bool is_latest_candle_green() const {
        if (candle_buffer.empty()) {
            return false;
        }
        return is_candle_green(candle_buffer.back());
    }
    
    // Vérifier si la dernière bougie Heikin-Ashi est verte
    bool is_latest_heikin_ashi_green() const {
        if (ha_buffer.empty()) {
            return false;
        }
        
        return is_candle_green(ha_buffer.back());
    }
    
    // Supprimer toutes les bougies
    void clear() {
        candle_buffer.clear();
        ha_buffer.clear();
    }
    
    // Récupérer le nombre total de bougies
    size_t size() const {
        return candle_buffer.size();
    }
    
    // Méthodes de compatibilité pour Strategy
    
    // Je vois pas comment cela pourrait être utile
    // Accès à une bougie par indice
    BasicCandle at(size_t index) const {
        return candle_buffer.at(index);
    }
    
    // Ca non plus
    // Opérateur [] pour accéder aux bougies
    BasicCandle operator[](size_t index) const {
        return candle_buffer[index];
    }
    
    // Configurez les paramètres de gestion du buffer
    void set_buffer_params(size_t max_size, size_t threshold, size_t target) {
        max_buffer_size = max_size;
        hysteresis_threshold = threshold;
        clean_target_size = target;
        
        // Vérifier si les nouveaux paramètres nécessitent un nettoyage immédiat
        check_and_clean_buffers();
    }
};