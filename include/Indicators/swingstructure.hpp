#pragma once
#include "IncrementalIndicator.hpp"
#include <deque>
#include <vector>
#include <cmath>

/**
 * Swing Structure Trend indicator.
 *
 * A swing high is confirmed when the price rose by at least `highMove` over
 * a leg of `minPeriods`-`maxPeriods` bars before the peak, then fell by at
 * least `highMove` over a leg of the same duration range after it, and the
 * peak is the highest high within +/- maxPeriods bars. A swing low mirrors
 * this using `lowMove` on candle lows/closes.
 *
 * The trend is up when the latest swing high/low both exceed the previous
 * ones, down when both are lower, and uncertain otherwise.
 *
 * Each candidate bar is evaluated exactly once, at the moment it becomes
 * the middle of a 2*maxPeriods+1 rolling window (i.e. once maxPeriods bars
 * are available both before and after it) - this gives the widest possible
 * validation window and avoids re-checking the same candidate repeatedly.
 */
class SWINGSTRUCTURE : public IncrementalIndicator<filter::SwingStructureResult> {
private:
    double high_move;
    double low_move;
    int min_periods;
    int max_periods;

    std::deque<BasicCandle> buffer; // rolling window, oldest first, capped at 2*max_periods+1

    struct SwingPoint { double price; };
    std::vector<SwingPoint> swing_highs; // last 2 confirmed swing highs
    std::vector<SwingPoint> swing_lows;  // last 2 confirmed swing lows

    double last_swing_high = 0.0;
    double last_swing_low = 0.0;
    int current_trend = 0; // 1 = up, -1 = down, 0 = unknown/uncertain

    static void push_swing(std::vector<SwingPoint>& list, double price) {
        list.push_back({price});
        if (list.size() > 2) list.erase(list.begin());
    }

    int compute_trend() const {
        if (swing_highs.size() < 2 || swing_lows.size() < 2) return 0;
        double hh = swing_highs[1].price, ph = swing_highs[0].price;
        double hl = swing_lows[1].price,  pl = swing_lows[0].price;
        if (hh > ph && hl > pl) return 1;
        if (hh < ph && hl < pl) return -1;
        return 0;
    }

    // Evaluates buffer[peakIdx] (expected to be the middle of the full window) as a swing high.
    bool check_swing_high(int peakIdx, double& outPrice) const {
        double peak = buffer[peakIdx].high;

        bool rose = false;
        for (int k = min_periods; k <= max_periods; ++k) {
            if (peak - buffer[peakIdx - k].close >= high_move) { rose = true; break; }
        }
        if (!rose) return false;

        bool fell = false;
        for (int j = min_periods; j <= max_periods; ++j) {
            if (peak - buffer[peakIdx + j].close >= high_move) { fell = true; break; }
        }
        if (!fell) return false;

        for (int i = 0; i < static_cast<int>(buffer.size()); ++i) {
            if (i != peakIdx && buffer[i].high > peak) return false;
        }

        outPrice = peak;
        return true;
    }

    // Evaluates buffer[troughIdx] (expected to be the middle of the full window) as a swing low.
    bool check_swing_low(int troughIdx, double& outPrice) const {
        double trough = buffer[troughIdx].low;

        bool fell = false;
        for (int k = min_periods; k <= max_periods; ++k) {
            if (buffer[troughIdx - k].close - trough >= low_move) { fell = true; break; }
        }
        if (!fell) return false;

        bool rose = false;
        for (int j = min_periods; j <= max_periods; ++j) {
            if (buffer[troughIdx + j].close - trough >= low_move) { rose = true; break; }
        }
        if (!rose) return false;

        for (int i = 0; i < static_cast<int>(buffer.size()); ++i) {
            if (i != troughIdx && buffer[i].low < trough) return false;
        }

        outPrice = trough;
        return true;
    }

    void process_candidate() {
        int peakIdx = max_periods; // middle of the full window

        double price;
        if (check_swing_high(peakIdx, price)) {
            push_swing(swing_highs, price);
            last_swing_high = price;
        }
        if (check_swing_low(peakIdx, price)) {
            push_swing(swing_lows, price);
            last_swing_low = price;
        }

        current_trend = compute_trend();
    }

    void feed(const BasicCandle& candle) {
        buffer.push_back(candle);
        if (static_cast<int>(buffer.size()) > 2 * max_periods + 1) {
            buffer.pop_front();
        }
        if (static_cast<int>(buffer.size()) == 2 * max_periods + 1) {
            process_candidate();
        }
    }

public:
    explicit SWINGSTRUCTURE(filter::SwingStructureParams params)
        : IncrementalIndicator<filter::SwingStructureResult>(
              "SWINGSTRUCTURE_" + std::to_string(params.highMove) + "_" + std::to_string(params.lowMove) + "_" +
              std::to_string(params.minPeriods) + "_" + std::to_string(params.maxPeriods),
              params.maxPeriods * 2 + 1),
          high_move(params.highMove), low_move(params.lowMove),
          min_periods(params.minPeriods), max_periods(params.maxPeriods) {}

    std::optional<filter::SwingStructureResult> initialize_with_history(const std::vector<BasicCandle>& history) override {
        buffer.clear();
        swing_highs.clear();
        swing_lows.clear();
        last_swing_high = 0.0;
        last_swing_low = 0.0;
        current_trend = 0;

        if (history.size() < static_cast<size_t>(2 * max_periods + 1)) {
            is_initialized = false;
            return std::nullopt;
        }

        for (const auto& candle : history) {
            feed(candle);
        }

        is_initialized = true;
        return std::make_optional(filter::SwingStructureResult(last_swing_high, last_swing_low, current_trend));
    }

    std::optional<filter::SwingStructureResult> update(const BasicCandle& candle) override {
        if (!is_initialized) {
            return std::nullopt;
        }

        feed(candle);

        return std::make_optional(filter::SwingStructureResult(last_swing_high, last_swing_low, current_trend));
    }

    std::optional<filter::SwingStructureResult> get_value() const override {
        return std::make_optional(filter::SwingStructureResult(last_swing_high, last_swing_low, current_trend));
    }

    void reset() override {
        IncrementalIndicator<filter::SwingStructureResult>::reset();
        buffer.clear();
        swing_highs.clear();
        swing_lows.clear();
        last_swing_high = 0.0;
        last_swing_low = 0.0;
        current_trend = 0;
    }
};
