#pragma once
#include "IncrementalIndicator.hpp"
#include <deque>
#include <vector>
#include <cmath>

/**
 * Swing Structure Trend indicator.
 *
 * Tracks the running-extreme high and low independently: each is the most extreme bar seen
 * since the last confirmed swing, replaced whenever a more extreme bar appears. A candidate
 * is confirmed as soon as (a) a prior leg exists within minPeriods-maxPeriods bars before it
 * whose wick is at least highMove/lowMove away, and (b) a bar within minPeriods-maxPeriods
 * bars after it reverses by at least highMove/lowMove (wick-to-wick, not wick-to-close) -
 * confirmation happens on the first qualifying bar, it does not wait for maxPeriods bars.
 *
 * The trend is up when the latest swing high/low both exceed the previous ones, down when
 * both are lower, and uncertain otherwise.
 */
class SWINGSTRUCTURE : public IncrementalIndicator<filter::SwingStructureResult> {
private:
    double high_move;
    double low_move;
    int min_periods;
    int max_periods;

    std::deque<BasicCandle> buffer; // rolling window, oldest first, capped at 2*max_periods+1
    long long buffer_start_index = 0; // absolute index of buffer.front()
    long long current_index = -1;     // absolute index of the most recently fed candle

    struct SwingPoint { double price; };
    std::vector<SwingPoint> swing_highs; // last 2 confirmed swing highs
    std::vector<SwingPoint> swing_lows;  // last 2 confirmed swing lows

    double last_swing_high = 0.0;
    double last_swing_low = 0.0;
    int current_trend = 0; // 1 = up, -1 = down, 0 = unknown/uncertain

    // Running-extreme candidates, tracked independently for highs and lows.
    long long high_idx = -1; double high_extreme = 0.0; bool high_leg_ok = false;
    long long low_idx = -1;  double low_extreme = 0.0;  bool low_leg_ok = false;

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

    const BasicCandle& at(long long abs_idx) const {
        return buffer[static_cast<size_t>(abs_idx - buffer_start_index)];
    }

    // Does a bar exist within [min_periods, max_periods] bars before `idx` (bounded to the
    // retained buffer) whose wick is at least `move` away from `extreme`?
    bool has_prior_leg(long long idx, bool for_high, double extreme, double move) const {
        for (int k = min_periods; k <= max_periods; ++k) {
            long long ref_idx = idx - k;
            if (ref_idx < buffer_start_index) break; // not enough history retained
            const BasicCandle& ref = at(ref_idx);
            if (for_high) {
                if (extreme - ref.low >= move) return true;
            } else {
                if (ref.high - extreme >= move) return true;
            }
        }
        return false;
    }

    void process_new_candle() {
        const BasicCandle& c = buffer.back();
        long long i = current_index;

        // Swing high tracking
        if (high_idx < 0 || c.high > high_extreme) {
            high_idx = i; high_extreme = c.high; high_leg_ok = false;
        }
        long long high_bars_since = i - high_idx;
        if (high_bars_since >= min_periods && high_bars_since <= max_periods) {
            if (!high_leg_ok) high_leg_ok = has_prior_leg(high_idx, true, high_extreme, high_move);
            if (high_leg_ok && high_extreme - c.low >= high_move) {
                push_swing(swing_highs, high_extreme);
                last_swing_high = high_extreme;
                high_idx = i; high_extreme = c.high; high_leg_ok = false;
            }
        } else if (high_bars_since > max_periods) {
            // No reversal within max_periods bars: this candidate has gone stale.
            // Restart the search from the current bar instead of staying anchored
            // to it indefinitely (which would otherwise happen forever if this bar
            // is a historical extreme price never revisits, e.g. after a large gap).
            high_idx = i; high_extreme = c.high; high_leg_ok = false;
        }

        // Swing low tracking
        if (low_idx < 0 || c.low < low_extreme) {
            low_idx = i; low_extreme = c.low; low_leg_ok = false;
        }
        long long low_bars_since = i - low_idx;
        if (low_bars_since >= min_periods && low_bars_since <= max_periods) {
            if (!low_leg_ok) low_leg_ok = has_prior_leg(low_idx, false, low_extreme, low_move);
            if (low_leg_ok && c.high - low_extreme >= low_move) {
                push_swing(swing_lows, low_extreme);
                last_swing_low = low_extreme;
                low_idx = i; low_extreme = c.low; low_leg_ok = false;
            }
        } else if (low_bars_since > max_periods) {
            // Stale candidate: restart the search from the current bar (see the
            // matching comment in the swing-high branch above).
            low_idx = i; low_extreme = c.low; low_leg_ok = false;
        }

        current_trend = compute_trend();
    }

    void feed(const BasicCandle& candle) {
        buffer.push_back(candle);
        ++current_index;
        long long max_buffer = 2LL * max_periods + 1;
        while (static_cast<long long>(buffer.size()) > max_buffer) {
            buffer.pop_front();
            ++buffer_start_index;
        }
        process_new_candle();
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
        buffer_start_index = 0;
        current_index = -1;
        swing_highs.clear();
        swing_lows.clear();
        last_swing_high = 0.0;
        last_swing_low = 0.0;
        current_trend = 0;
        high_idx = -1;
        low_idx = -1;

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
        buffer_start_index = 0;
        current_index = -1;
        swing_highs.clear();
        swing_lows.clear();
        last_swing_high = 0.0;
        last_swing_low = 0.0;
        current_trend = 0;
        high_idx = -1;
        low_idx = -1;
    }
};
