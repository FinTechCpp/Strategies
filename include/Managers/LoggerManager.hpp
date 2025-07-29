#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <chrono>

enum class LogCategory {
    GENERAL,
    INDICATOR,
    FILTER,
    SIGNAL,
    EXECUTION,
    RISK,
    TIME
};

class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Configuration du logger
    virtual void set_log_callback(std::function<void(const std::string&, int)> callback) = 0;
    virtual void set_enabled(bool state) = 0;
    virtual void set_verbosity(int level) = 0;
    virtual LogLevel get_verbosity() const = 0;
    virtual void set_current_candle(const Candle& candle) = 0;
    virtual void clear() = 0;
    
    // Méthodes pour gérer le chronomètre
    virtual void start_chrono() = 0;
    virtual int64_t stop_chrono_and_log() = 0;
    
    // Finalisation et envoi des logs
    virtual void finalize_and_send_logs() = 0;
    
    // Logs généraux
    virtual void log_general(const std::string& message, int level = LogLevel::INFO) = 0;
    virtual void log_general(std::string&& message, int level = LogLevel::INFO) = 0;
    
    // Logs d'indicateurs
    virtual void log_indicator_value(const std::string& name, double value, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_value(const std::string& name, std::pair<double, double> values, int level = LogLevel::DEBUG) = 0;
    virtual void log_indicator_comparison(const std::string& name, double value, double threshold, const std::string& comparison_op, bool result, int level = LogLevel::DEBUG) = 0;
    
    // Logs de filtres
    virtual void log_filter_result(const std::string& name, bool passed, const std::string& detail = "", int level = LogLevel::DEBUG) = 0;
    
    // Logs de signaux
    virtual void log_signal(const std::string& action, double price, double quantity, int level = LogLevel::INFO) = 0;
    virtual void log_sl_tp(double sl_distance, double tp_distance, int level = LogLevel::INFO) = 0;
    
    // Logs d'exécution
    virtual void log_execution_step(const std::string& step, bool success, int level = LogLevel::INFO) = 0;
    virtual void log_execution_time(int64_t duration_us, int level = LogLevel::DEBUG) = 0;
    
    // Logs de risque
    virtual void log_risk_calculation(double risk_amount, double risk_percentage, int level = LogLevel::INFO) = 0;
    virtual void log_position_sizing(double raw_size, double adjusted_size, const std::string& reason, int level = LogLevel::INFO) = 0;
    
    // Logs de temps
    virtual void log_time_check(bool in_trading_hours, const std::string& detail, int level = LogLevel::INFO) = 0;
    
    // Obtention de tous les logs pour la bougie actuelle
    virtual std::string get_all_logs() const = 0;

    virtual std::string fast_double_to_string(double value, int precision = 4) = 0;
    virtual std::string fast_int_to_string(int value) = 0;
};

class LoggerManager : public ILogger {
private:
    bool enabled = true;
    LogLevel verbosity_level = LogLevel::DEBUG;
    std::function<void(const std::string&, int)> callback;
    DateTime current_candle_date;
    mutable char buffer[64];
    mutable std::string msgBuffer;
    // mutable char numBuffer[64];

    // Variables pour le chronomètre
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    bool chrono_running = false;
    
    // Buffer pour stocker les logs par catégorie
    std::vector<std::string> general_logs;
    std::vector<std::string> indicator_logs;
    std::vector<std::string> filter_logs;
    std::vector<std::string> signal_logs;
    std::vector<std::string> execution_logs;
    std::vector<std::string> risk_logs;
    std::vector<std::string> time_logs;
    
    // Indentation pour les logs hiérarchiques
    std::string indent(int level) const {
        return std::string(level * 2, ' ');
    }
    
    // Ajout d'un log à la catégorie appropriée
    void add_log(LogCategory category, std::string&& message, int level = LogLevel::INFO) {
        if (!enabled || level < verbosity_level)
            return;
        
        std::vector<std::string>* target_logs;
        
        // Utiliser un pointeur direct au vecteur approprié au lieu d'un switch
        switch (category) {
            case LogCategory::GENERAL:   target_logs = &general_logs; break;
            case LogCategory::INDICATOR: target_logs = &indicator_logs; break;
            case LogCategory::FILTER:    target_logs = &filter_logs; break;
            case LogCategory::SIGNAL:    target_logs = &signal_logs; break;
            case LogCategory::EXECUTION: target_logs = &execution_logs; break;
            case LogCategory::RISK:      target_logs = &risk_logs; break;
            case LogCategory::TIME:      target_logs = &time_logs; break;
        }

        target_logs->push_back(std::move(message));  // Utiliser move pour éviter une copie
    }

public:
    LoggerManager() {
        // Préallouer la mémoire pour le buffer de message
        msgBuffer.reserve(256);
        
        // Préallouer la mémoire pour les vecteurs de logs (évite les réallocations)
        general_logs.reserve(50);
        indicator_logs.reserve(50);
        filter_logs.reserve(50);
        signal_logs.reserve(20);
        execution_logs.reserve(20);
        risk_logs.reserve(20);
        time_logs.reserve(10);
    }

    std::string fast_double_to_string(double value, int precision = 4) override {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision);
        return std::string(buffer, ptr - buffer);
    }

    std::string fast_int_to_string(int value) override {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::string(buffer, ptr - buffer);
    }

    void set_log_callback(std::function<void(const std::string&, int)> callback)  override{
        // Enregistrer le callback pour les logs
        this->callback = std::move(callback);
    }
    
    // Configuration du logger
    void set_enabled(bool state) override { enabled = state; }
    void set_verbosity(int level) override { verbosity_level = static_cast<LogLevel>(level); }
    LogLevel get_verbosity() const override { return verbosity_level; }
    void set_current_candle(const Candle& candle) override { current_candle_date = candle.ohlc.date; }
    void clear() override {
        general_logs.clear();
        indicator_logs.clear();
        filter_logs.clear();
        signal_logs.clear();
        execution_logs.clear();
        risk_logs.clear();
        time_logs.clear();
    }
    
    // Méthodes pour gérer le chronomètre
    void start_chrono() override {
        start_time = std::chrono::high_resolution_clock::now();
        chrono_running = true;
    }

    int64_t stop_chrono_and_log() override {
        if (!chrono_running) {
            log_general("Tentative d'arrêt du chronomètre alors qu'il n'est pas démarré", LogLevel::WARNING);
            return 0;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        int64_t duration_us = duration.count();
        
        // Logger le temps d'exécution
        log_execution_time(duration_us);
        
        chrono_running = false;
        return duration_us;
    }
    
    // Nouvelle méthode pour finaliser les logs et les envoyer
    void finalize_and_send_logs() override {
        // Si le chronomètre est toujours en cours, l'arrêter et logger le temps
        if (chrono_running)
            stop_chrono_and_log();
        
        // Envoyer tous les logs
        std::string message(get_all_logs());
        if (message.empty() || !callback) return;

        callback(message, get_verbosity());
    }
    
    // Logs généraux
    void log_general(std::string&& message, int level = LogLevel::INFO) override {
        add_log(LogCategory::GENERAL, std::move(message), level);
    }

    void log_general(const std::string& message, int level = LogLevel::INFO) override {
        // Créer une copie et la déplacer pour éviter une double copie
        std::string msg_copy = message;
        add_log(LogCategory::GENERAL, std::move(msg_copy), level);
    }
    
    // Logs d'indicateurs
    void log_indicator_value(const std::string& name, double value, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicateur " + name + " = " + fast_double_to_string(value);
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }

    void log_indicator_value(const std::string& name, std::pair<double, double> values, int level = LogLevel::DEBUG) override {
        std::string msg = "Indicateur " + name + " = [" + fast_double_to_string(values.first) + ", " + fast_double_to_string(values.second) + "]";
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }
    
    void log_indicator_comparison(const std::string& name, double value, double threshold, const std::string& comparison_op, bool result, int level = LogLevel::DEBUG) override {
        std::string status = result ? "VALIDÉ" : "REJETÉ";
        std::string msg = "Indicateur " + name + " " + status + ": " + fast_double_to_string(value) + " " + comparison_op + " " + fast_double_to_string(threshold);
        add_log(LogCategory::INDICATOR, std::move(msg), level);
    }
    
    // Logs de filtres
    void log_filter_result(const std::string& name, bool passed, const std::string& detail = "", int level = LogLevel::INFO) override {
        std::string status = passed ? "PASSÉ" : "REJETÉ";
        std::string msg = "Filtre " + name + ": " + status;
        add_log(LogCategory::FILTER, std::move(msg), level);
        if (!detail.empty())
            add_log(LogCategory::FILTER, indent(1) + detail, level);
    }
    
    // Logs de signaux
    void log_signal(const std::string& action, double price, double quantity, int level = LogLevel::INFO) override {
        std::string msg = "Signal " + action + " généré: Prix=" + fast_double_to_string(price) + ", Quantité=" + fast_double_to_string(quantity);
        add_log(LogCategory::SIGNAL, std::move(msg), level);
    }

    void log_sl_tp(double sl_distance, double tp_distance, int level = LogLevel::INFO) override {
        std::string msg = indent(1) + "SL=" + fast_double_to_string(sl_distance) + ", TP=" + fast_double_to_string(tp_distance);
        add_log(LogCategory::SIGNAL, std::move(msg), level);
    }
    
    // Logs d'exécution
    void log_execution_step(const std::string& step, bool success, int level = LogLevel::INFO) override {
        std::string status = success ? "succès" : "échec";
        std::string msg = "Étape '" + step + "': " + status;
        add_log(LogCategory::EXECUTION, std::move(msg), level);
    }
    
    // Logs de risque
    void log_risk_calculation(double risk_amount, double risk_percentage, int level = LogLevel::INFO) override {
        std::string msg = "Risque calculé: " + fast_double_to_string(risk_amount) + " (" + fast_double_to_string(risk_percentage) + "% du capital)";
        add_log(LogCategory::RISK, std::move(msg), level);
    }

    void log_position_sizing(double raw_size, double adjusted_size, const std::string& reason, int level = LogLevel::INFO) override {
        std::string msg = "Position sizing: " + fast_double_to_string(raw_size) + " -> " + fast_double_to_string(adjusted_size) + " (" + reason + ")";
        add_log(LogCategory::RISK, std::move(msg), level);
    }
    
    // Logs de temps
    void log_time_check(bool in_trading_hours, const std::string& detail, int level = LogLevel::INFO) override {
        std::string status = in_trading_hours ? "DANS" : "HORS";
        std::string msg = status + " horaires de trading: " + detail;
        add_log(LogCategory::TIME, std::move(msg), level);
    }

    // Logs de performance
    void log_execution_time(int64_t duration_us, int level = LogLevel::DEBUG) override {
        std::string msg = "Temps d'exécution: " + fast_int_to_string(duration_us) + " us";

        // Changer le niveau si le traitement prend trop de temps
        if (duration_us > 20) {  // Plus de 20 us
            level = LogLevel::WARNING;
            msg += " (LENT)";
        } else if (duration_us > 10) {  // Plus de 10 us
            level = LogLevel::INFO;
            msg += " (Modéré)";
        }

        add_log(LogCategory::EXECUTION, std::move(msg), level);
    }
    
    // Obtention de tous les logs pour la bougie actuelle
    std::string get_all_logs() const override {
        if (general_logs.empty() && indicator_logs.empty() && filter_logs.empty() && 
            signal_logs.empty() && execution_logs.empty() && risk_logs.empty() && time_logs.empty()) {
            return "";
        }

        // Estimer la taille totale requise pour éviter les réallocations
        size_t total_size = 200; // En-tête de base
        
        // Ajouter la taille estimée pour chaque section
        total_size += general_logs.size() * 50;    // Moyenne estimée par log
        total_size += indicator_logs.size() * 50;
        total_size += filter_logs.size() * 50;
        total_size += signal_logs.size() * 50;
        total_size += execution_logs.size() * 50;
        total_size += risk_logs.size() * 50;
        total_size += time_logs.size() * 50;
        
        // Pré-allouer la string finale
        std::string result;
        result.reserve(total_size);
        
        // Construire l'en-tête de la bougie directement dans la string
        result += "\n";
        result += "╔══════════════════════════════════════════════════════╗\n";
        result += "║             BOUGIE: ";
        result += current_candle_date.to_string();
        result += std::string(14, ' ');
        result += "║\n";
        result += "╚══════════════════════════════════════════════════════╝\n";

        // Construire chaque section
        auto append_section = [&result, this](const std::vector<std::string>& logs, const char* title) {
            if (logs.empty()) return;
            
            result += title;
            result += "\n";
            
            for (const auto& log : logs) {
                result += indent(1);
                result += log;
                result += "\n";
            }
        };

        append_section(general_logs, "GENERAL");
        append_section(indicator_logs, "INDICATEURS");
        append_section(filter_logs, "FILTRES");
        append_section(signal_logs, "SIGNAUX");
        append_section(execution_logs, "EXÉCUTION");
        append_section(risk_logs, "RISQUE");
        append_section(time_logs, "TEMPS");

        return result;
    }
};

// Logger null qui ne fait rien (pour optimisation en mode backtest)
class NullLogger : public ILogger {
public:
    void set_log_callback(std::function<void(const std::string&, int)>) override {}
    void set_enabled(bool) override {}
    void set_verbosity(int) override {}
    LogLevel get_verbosity() const override { return LogLevel::INFO; }
    void set_current_candle(const Candle&) override {}
    void clear() override {}
    void start_chrono() override {}
    int64_t stop_chrono_and_log() override { return 0; }
    void finalize_and_send_logs() override {}
    void log_general(const std::string&, int) override {}
    void log_general(std::string&&, int) override {}
    void log_indicator_value(const std::string&, double, int) override {}
    void log_indicator_value(const std::string&, std::pair<double, double>, int) override {}
    void log_indicator_comparison(const std::string&, double, double, const std::string&, bool, int) override {}
    void log_filter_result(const std::string&, bool, const std::string&, int) override {}
    void log_signal(const std::string&, double, double, int) override {}
    void log_sl_tp(double, double, int) override {}
    void log_execution_step(const std::string&, bool, int) override {}
    void log_execution_time(int64_t, int) override {}
    void log_risk_calculation(double, double, int) override {}
    void log_position_sizing(double, double, const std::string&, int) override {}
    void log_time_check(bool, const std::string&, int) override {}
    std::string get_all_logs() const override { return ""; }

    std::string fast_double_to_string(double value, int precision = 4) override { return ""; }
    std::string fast_int_to_string(int value) override { return ""; }
};

