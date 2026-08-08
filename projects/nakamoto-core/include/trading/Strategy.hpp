#pragma once

namespace nkm {

enum class Signal
{
    Hold,
    Buy,
    Sell,
};

// Simple configurable baseline strategy:
//
//   imbalance >= +threshold  -> BUY
//   imbalance <= -threshold  -> SELL
//   otherwise                -> HOLD
//
// This is an engineering baseline to complete the trading pipeline; it is NOT
// assumed to be profitable.
class ThresholdStrategy
{
public:
    explicit ThresholdStrategy(double threshold = 0.60);

    [[nodiscard]] Signal evaluate(double imbalance) const;

    [[nodiscard]] double threshold() const;

private:
    double threshold_;
};

} // namespace nkm
