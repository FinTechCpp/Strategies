#pragma once
#include "IncrementalIndicator.hpp"
#include <vector>
#include <numeric>
#include <deque>
#include <cmath>
#include <optional>
#include <string>

/**
 * Bollinger Bands (BB) calculated incrementally
 *
 * This implementation follows the style of EMA and MACD examples:
 * - Supports Source selection (OPEN/HIGH/LOW/CLOSE)
 * - Supports MAType (EMA or SMA) for the middle band
 * - Supports an offset (displacement) for the output bands
 *
 * The constructor expects filter::BBParams to contain at least:
 *   int period;
 *   double stddev_multiplier;
 * and optionally:
 *   enum source; // open/high/low/close
 *   enum ma_type; // EMA or SMA
 *   int offset; // optional displacement
 *
 * The indicator returns a filter::BBResult with (middle, upper, lower).
 * If your project already defines filter::BBResult / filter::BBParams, adapt names accordingly.
 */

using Params = filter::BBParams;
using Result = filter::BBResult;
using Source = filter::PriceType;

class BB : public IncrementalIndicator<Result> {
public:
    enum class MAType { EMA, SMA };

private:
    int period;
    double stddev_multiplier;
    Source source_field;
    MAType ma_type;
    int offset; // displacement for output (in bars)

    // For incremental standard deviation / SMA window
    std::deque<double> window; // last 'period' prices for current index
    double window_sum = 0.0;
    double window_sumsq = 0.0;

    // For EMA path
    double ema_mult = 0.0;
    double ema_current = 0.0; // last EMA computed for the most recent index

    // Holds computed band values for recent indices (used to implement offset)
    std::deque<Result> band_history;

    // Price history used prior to initialization and to preserve required context
    std::deque<double> price_history;

    // Current exposed bands (the delayed output after applying offset)
    double current_middle = 0.0;
    double current_upper = 0.0;
    double current_lower = 0.0;

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
    // Constructor: expects filter::BBParams with members as documented above.
    BB(filter::BBParams params)
    : IncrementalIndicator<filter::BBResult>("BB_" + std::to_string(params.period), params.period + (params.offset > 0 ? params.offset : 0)),
      period(params.period),
      stddev_multiplier(params.stddev_multiplier),
      source_field(Source::CLOSE),
      ma_type(MAType::SMA),
      offset(0)
    {
        // optional members handling (adapt if your BBParams has different names)
        try {
            // source
            if constexpr (std::is_member_object_pointer_v<decltype(&filter::BBParams::source)>) {
                switch (params.source) {
                    case 0: source_field = Source::OPEN; break;
                    case 1: source_field = Source::HIGH; break;
                    case 2: source_field = Source::LOW; break;
                    case 3: default: source_field = Source::CLOSE; break;
                }
            }
        } catch (...) {}

        try {
            if constexpr (std::is_member_object_pointer_v<decltype(&filter::BBParams::ma_type)>) {
                // assume 0 -> SMA, 1 -> EMA
                ma_type = (params.ma_type == 1 ? MAType::EMA : MAType::SMA);
            }
        } catch (...) {}

        try {
            if constexpr (std::is_member_object_pointer_v<decltype(&filter::BBParams::offset)>) {
                offset = params.offset;
                if (offset < 0) offset = 0;
            }
        } catch (...) {}

        // EMA multiplier if used
        if (ma_type == MAType::EMA) {
            ema_mult = 2.0 / (period + 1.0);
        }
    }

    std::optional<filter::BBResult> initialize_with_history(const std::vector<BasicCandle>& history) override;
    std::optional<filter::BBResult> update(const BasicCandle& candle) override;
    std::optional<filter::BBResult> get_value() const override;
};


// ---------------- Implementation ----------------

inline std::optional<filter::BBResult> BB::initialize_with_history(const std::vector<BasicCandle>& history) {
    // Need at least period + offset prices to be able to output one value after applying displacement
    size_t needed = static_cast<size_t>(period + offset);
    if (history.size() < needed) {
        return std::nullopt;
    }

    // Build price vector according to selected source
    std::vector<double> prices;
    prices.reserve(history.size());
    for (const auto& c : history) prices.push_back(pick_price(c));

    // reset internal state
    window.clear();
    window_sum = 0.0;
    window_sumsq = 0.0;
    price_history.clear();
    band_history.clear();
    ema_current = 0.0;

    // We'll walk prices and compute middle (SMA or EMA) and stddev for each index where window is full.
    // For EMA we seed with the SMA of the first full window.
    for (size_t i = 0; i < prices.size(); ++i) {
        double p = prices[i];
        price_history.push_back(p);
        // Maintain rolling window for variance/stddev
        window.push_back(p);
        window_sum += p;
        window_sumsq += p * p;
        if (window.size() > static_cast<size_t>(period)) {
            double out = window.front();
            window.pop_front();
            window_sum -= out;
            window_sumsq -= out * out;
        }

        // compute middle/stddev only when window is full
        if (window.size() == static_cast<size_t>(period)) {
            double middle = 0.0;
            if (ma_type == MAType::SMA) {
                middle = window_sum / period;
            } else { // EMA
                // Seed EMA on the first occurrence
                if (ema_current == 0.0) {
                    // seed with SMA of this first full window
                    ema_current = window_sum / period;
                } else {
                    // update EMA with current price
                    ema_current = (p - ema_current) * ema_mult + ema_current;
                }
                // Note: when seeding the first EMA the ema_current already equals SMA for that index.
                middle = ema_current;
            }

            // stddev (population, dividing by period)
            double mean_for_std = window_sum / period;
            double variance = (window_sumsq / period) - (mean_for_std * mean_for_std);
            if (variance < 0.0 && variance > -1e-12) variance = 0.0; // clamp tiny negative
            double stddev = std::sqrt(std::max(0.0, variance));
            double upper = middle + stddev_multiplier * stddev;
            double lower = middle - stddev_multiplier * stddev;

            double percentB = 0.0;
            double denom = (upper - lower);
            if (std::abs(denom) > 1e-12) 
                percentB = (p - lower) / denom;
            
            band_history.emplace_back(middle, upper, lower, percentB);
        }
    }

    // After processing all prices we must have at least offset+1 completed band entries to output one (delayed)
    if (band_history.size() < static_cast<size_t>(offset + 1)) 
        return std::nullopt;
    
    // The output corresponds to the last computed band entry shifted by offset
    size_t idx = band_history.size() - 1 - static_cast<size_t>(offset);
    const filter::BBResult& out = band_history[idx];
    current_middle = out.middle;
    current_upper = out.upper;
    current_lower = out.lower;

    // Trim price_history to keep only necessary context for future incremental updates:
    // keep the last 'period' prices (for stddev) plus up to 'offset' prices for band_history alignment
    size_t keep_prices = static_cast<size_t>(period + offset);
    while (price_history.size() > keep_prices) price_history.pop_front();

    // Also keep band_history limited to offset+1 (we need up to offset future outputs)
    while (band_history.size() > static_cast<size_t>(offset + 1)) band_history.pop_front();

    is_initialized = true;
        return std::make_optional(filter::BBResult(current_middle, current_upper, current_lower, 0.0));
}

inline std::optional<filter::BBResult> BB::update(const BasicCandle& candle) {
    double price = pick_price(candle);

    // Accumulate until initialized
    if (!is_initialized) {
        price_history.push_back(price);
        if (price_history.size() >= static_cast<size_t>(period + offset)) {
            // build BasicCandle vector with selected price in close field to reuse initializer signature
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

    // Update rolling window sums for stddev
    window.push_back(price);
    window_sum += price;
    window_sumsq += price * price;
    if (window.size() > static_cast<size_t>(period)) {
        double out = window.front();
        window.pop_front();
        window_sum -= out;
        window_sumsq -= out * out;
    }

    // Maintain full price_history (needed for offset logic and possible re-initialization)
    price_history.push_back(price);
    size_t keep_prices = static_cast<size_t>(period + offset);
    while (price_history.size() > keep_prices) price_history.pop_front();

    // Only compute a band entry when we have a full window
    if (window.size() == static_cast<size_t>(period)) {
        double middle = 0.0;
        if (ma_type == MAType::SMA) {
            middle = window_sum / period;
        } else { // EMA
            // ema_current holds last EMA computed at previous "most recent index"
            // update EMA with newest price
            // Note: ema_current was already seeded during initialize. Keep using it incrementally.
            ema_current = (price - ema_current) * ema_mult + ema_current;
            middle = ema_current;
        }

        double mean_for_std = window_sum / period;
        double variance = (window_sumsq / period) - (mean_for_std * mean_for_std);
        if (variance < 0.0 && variance > -1e-12) variance = 0.0;
        double stddev = std::sqrt(std::max(0.0, variance));
        double upper = middle + stddev_multiplier * stddev;
        double lower = middle - stddev_multiplier * stddev;

        // push newly computed band for the current index
        double percentB = 0.0;
        double denom = (upper - lower);
        if (std::abs(denom) > 1e-12) 
            percentB = (price - lower) / denom;
        band_history.emplace_back(middle, upper, lower, percentB);
    }

    // If we have enough band entries to emit one after offset, do so
    if (band_history.size() > static_cast<size_t>(offset)) {
        filter::BBResult out = band_history.front();
        band_history.pop_front();

        current_middle = out.middle;
        current_upper = out.upper;
        current_lower = out.lower;

        // Keep band_history small (we need at most offset entries)
        while (band_history.size() > static_cast<size_t>(offset)) band_history.pop_front();

           return std::make_optional(filter::BBResult(current_middle, current_upper, current_lower, 0.0));
    }

    // Not enough band history to produce an output due to offset
    return std::nullopt;
}

inline std::optional<filter::BBResult> BB::get_value() const {
    if (!is_initialized) return std::nullopt;
    return std::make_optional(filter::BBResult(current_middle, current_upper, current_lower, 0.0));
}