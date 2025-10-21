#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <deque>
#include <optional>
#include <tuple>
#include <numeric>
#include <string>
#include <algorithm>

/**
 * Moving Average Convergence/Divergence (MACD) calculated incrementally
 *
 * Returns a tuple: (macd_line, signal_line, histogram)
 *
 * NOTE: This version adds the following configurable inputs:
 *  - fast length (fast_period)
 *  - slow length (slow_period)
 *  - source (Open, High, Low, Close)
 *  - signal smoothing (used for EMA signal smoothing period if provided)
 *  - Oscillator MA Type (EMA or SMA)  -- MA applied to fast/slow lines
 *  - Signal Line MA Type   (EMA or SMA)  -- MA applied to signal line
 *
 * The constructor expects filter::MACDParams to contain at least:
 *   int fast, slow, signal;
 * and optionally:
 *   enum source; // open/high/low/close 
 *   enum osc_ma_type;     // EMA or SMA 
 *   enum signal_ma_type;  // EMA or SMA 
 *   int signal_smoothing;        // optional smoothing length for signal (if 0 -> use signal)
 *
 * If your filter::MACDParams uses different member names/types adapt the mapping below.
 */
class MACD : public IncrementalIndicator<filter::MACDResult> {
public:
    enum class Source { OPEN, HIGH, LOW, CLOSE };
    enum class MAType { EMA, SMA };

private:
    int fast_period;
    int slow_period;
    int signal_period;
    int signal_smoothing_period; // if zero, use signal_period

    Source source_field;
    MAType osc_ma_type;
    MAType signal_ma_type;

    double mult_fast = 0.0;
    double mult_slow = 0.0;
    double mult_signal = 0.0;

    // current MA values (can be EMA or SMA depending on type)
    double ma_fast = 0.0;
    double ma_slow = 0.0;
    double signal = 0.0;

    double current_macd = 0.0;
    double current_hist = 0.0;

    // data structures for SMA incremental maintenance if needed
    std::deque<double> fast_window;
    std::deque<double> slow_window;
    std::deque<double> signal_window;
    double fast_sum = 0.0;
    double slow_sum = 0.0;
    double signal_sum = 0.0;

    // history of raw selected prices for initialization before we become fully initialized
    std::deque<double> price_history;
    // macd_history used to initialize signal line from past macd values
    std::deque<double> macd_history;

    // helper: choose price from a candle based on source_field
    inline double pick_price(const BasicCandle& c) const {
        switch (source_field) {
            case Source::OPEN:  return c.open;
            case Source::HIGH:  return c.high;
            case Source::LOW:   return c.low;
            case Source::CLOSE: default: return c.close;
        }
    }

public:
    MACD(filter::MACDParams params)
    : IncrementalIndicator<filter::MACDResult>(
        "MACD_" + std::to_string(params.fast) + "_" + std::to_string(params.slow) + "_" + std::to_string(params.signal),
        params.slow + params.signal + 1),
      fast_period(params.fast),
      slow_period(params.slow),
      signal_period(params.signal),
      signal_smoothing_period(0), // default -> use signal_period
      source_field(Source::CLOSE),
      osc_ma_type(MAType::EMA),
      signal_ma_type(MAType::EMA)
    {
        // signal smoothing period override (if present)
        if constexpr (std::is_member_object_pointer_v<decltype(&filter::MACDParams::signal_smoothing)>) {
            try {
                signal_smoothing_period = params.signal_smoothing;
            } catch (...) { signal_smoothing_period = 0; }
        }

        // compute multipliers for EMA paths (for EMA types only)
        mult_fast = 2.0 / (fast_period + 1.0);
        mult_slow = 2.0 / (slow_period + 1.0);
        int signal_period_for_mult = (signal_smoothing_period > 0 ? signal_smoothing_period : signal_period);
        mult_signal = 2.0 / (signal_period_for_mult + 1.0);
    }

    std::optional<filter::MACDResult> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<filter::MACDResult> update(const BasicCandle& candle) override;
    std::optional<filter::MACDResult> get_value() const override;
};


// ---------------- Implementation ----------------

inline std::optional<filter::MACDResult> MACD::initialize_with_history(const std::vector<BasicCandle>& history) {
    // Need enough data to compute slow MA and build signal MA from MACD history
    size_t needed = static_cast<size_t>(slow_period + signal_period);
    if (history.size() < needed) {
        return std::nullopt;
    }

    // build price vector according to selected source
    std::vector<double> prices;
    prices.reserve(history.size());
    for (const auto& c : history) prices.push_back(pick_price(c));

    // reset internal state
    price_history.clear();
    macd_history.clear();
    fast_window.clear();
    slow_window.clear();
    signal_window.clear();
    fast_sum = slow_sum = signal_sum = 0.0;
    ma_fast = ma_slow = signal = 0.0;

    std::optional<double> fast_opt;
    std::optional<double> slow_opt;

    // We'll walk prices and compute fast/slow MAs when each becomes available.
    for (size_t i = 0; i < prices.size(); ++i) {
        double p = prices[i];

        // --- fast MA ---
        if (osc_ma_type == MAType::EMA) {
            if (!fast_opt.has_value()) {
                if (i + 1 >= static_cast<size_t>(fast_period)) {
                    double sum = 0.0;
                    for (size_t j = i + 1 - fast_period; j <= i; ++j) sum += prices[j];
                    ma_fast = sum / fast_period; // initial SMA -> seed EMA
                    fast_opt = ma_fast;
                }
            } else {
                ma_fast = (p - ma_fast) * mult_fast + ma_fast;
            }
        } else { // SMA
            fast_window.push_back(p);
            fast_sum += p;
            if (fast_window.size() > static_cast<size_t>(fast_period)) {
                fast_sum -= fast_window.front();
                fast_window.pop_front();
            }
            if (fast_window.size() == static_cast<size_t>(fast_period)) {
                ma_fast = fast_sum / fast_period;
                fast_opt = ma_fast;
            }
        }

        // --- slow MA ---
        if (osc_ma_type == MAType::EMA) {
            if (!slow_opt.has_value()) {
                if (i + 1 >= static_cast<size_t>(slow_period)) {
                    double sum = 0.0;
                    for (size_t j = i + 1 - slow_period; j <= i; ++j) sum += prices[j];
                    ma_slow = sum / slow_period; // seed
                    slow_opt = ma_slow;
                }
            } else {
                ma_slow = (p - ma_slow) * mult_slow + ma_slow;
            }
        } else { // SMA
            slow_window.push_back(p);
            slow_sum += p;
            if (slow_window.size() > static_cast<size_t>(slow_period)) {
                slow_sum -= slow_window.front();
                slow_window.pop_front();
            }
            if (slow_window.size() == static_cast<size_t>(slow_period)) {
                ma_slow = slow_sum / slow_period;
                slow_opt = ma_slow;
            }
        }

        // If both MAs available, record MACD value for this index
        if (fast_opt.has_value() && slow_opt.has_value()) {
            double macd_val = ma_fast - ma_slow;
            macd_history.push_back(macd_val);
        }
    }

    // Need at least signal_period macd values to initialize signal MA
    if (macd_history.size() < static_cast<size_t>(signal_period)) {
        return std::nullopt;
    }

    // initialize signal according to signal_ma_type
    if (signal_ma_type == MAType::EMA) {
        // initial SMA over first signal_period
        double sum = 0.0;
        for (size_t i = 0; i < static_cast<size_t>(signal_period); ++i) sum += macd_history[i];
        signal = sum / signal_period;
        // then apply EMA using mult_signal over remaining macd_history
        for (size_t i = signal_period; i < macd_history.size(); ++i) {
            signal = (macd_history[i] - signal) * mult_signal + signal;
        }
    } else { // SMA signal
        // last signal_period macd values average
        size_t m = macd_history.size();
        double sum = 0.0;
        for (size_t i = m - signal_period; i < m; ++i) sum += macd_history[i];
        signal = sum / signal_period;

        // prepare signal_window for incremental SMA updates (push the last signal_period values)
        signal_window.clear();
        signal_sum = 0.0;
        size_t start = (m >= static_cast<size_t>(signal_period)) ? (m - signal_period) : 0;
        for (size_t i = start; i < m; ++i) {
            signal_window.push_back(macd_history[i]);
            signal_sum += macd_history[i];
        }
    }

    // Set current values to last computed
    current_macd = macd_history.back();
    current_hist = current_macd - signal;

    // Prepare incremental internal windows for future updates:
    // For SMA fast/slow we already have fast_window/slow_window as they were built while iterating.
    // For EMA paths ma_fast/ma_slow hold last EMA values.

    is_initialized = true;
    return std::make_optional(filter::MACDResult(current_macd, signal, current_hist));
}

inline std::optional<filter::MACDResult> MACD::update(const BasicCandle& candle) {
    double price = pick_price(candle);

    // Accumulate until initialized
    if (!is_initialized) {
        price_history.push_back(price);
        if (price_history.size() >= static_cast<size_t>(slow_period + signal_period)) {
            // build BasicCandle vector with selected price in close field to reuse old initializer signature
            std::vector<BasicCandle> hist;
            hist.reserve(price_history.size());
            for (double p : price_history) {
                BasicCandle c; c.open = c.high = c.low = c.close = p;
                hist.push_back(c);
            }
            return initialize_with_history(hist);
        }
        return std::nullopt;
    }

    // Update oscillator MAs incrementally
    if (osc_ma_type == MAType::EMA) {
        ma_fast = (price - ma_fast) * mult_fast + ma_fast;
        ma_slow = (price - ma_slow) * mult_slow + ma_slow;
    } else { // SMA incremental
        // fast window
        fast_window.push_back(price);
        fast_sum += price;
        if (fast_window.size() > static_cast<size_t>(fast_period)) {
            fast_sum -= fast_window.front();
            fast_window.pop_front();
        }
        if (fast_window.size() == static_cast<size_t>(fast_period)) ma_fast = fast_sum / fast_period;

        // slow window
        slow_window.push_back(price);
        slow_sum += price;
        if (slow_window.size() > static_cast<size_t>(slow_period)) {
            slow_sum -= slow_window.front();
            slow_window.pop_front();
        }
        if (slow_window.size() == static_cast<size_t>(slow_period)) ma_slow = slow_sum / slow_period;
    }

    current_macd = ma_fast - ma_slow;

    // Update signal line incrementally based on its MA type
    if (signal_ma_type == MAType::EMA) {
        signal = (current_macd - signal) * mult_signal + signal;
    } else { // SMA signal
        signal_window.push_back(current_macd);
        signal_sum += current_macd;
        if (signal_window.size() > static_cast<size_t>(signal_period)) {
            signal_sum -= signal_window.front();
            signal_window.pop_front();
        }
        if (signal_window.size() == static_cast<size_t>(signal_period)) {
            signal = signal_sum / signal_period;
        } else {
            // until signal_window is full we cannot produce a valid signal -> prefer returning nullopt
            return std::nullopt;
        }
    }

    current_hist = current_macd - signal;
    return std::make_optional(filter::MACDResult(current_macd, signal, current_hist));
}

inline std::optional<filter::MACDResult> MACD::get_value() const {
    // if not initialized or signal not ready (in SMA signal case), still return current values (caller can check is_initialized)
    return std::make_optional(filter::MACDResult(current_macd, signal, current_hist));
}