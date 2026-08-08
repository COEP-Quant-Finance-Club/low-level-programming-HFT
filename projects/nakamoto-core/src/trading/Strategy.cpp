#include "trading/Strategy.hpp"

namespace nkm {

ThresholdStrategy::ThresholdStrategy(double threshold)
    // A negative threshold would make the BUY and SELL regions overlap;
    // clamp to the smallest sensible value.
    : threshold_(threshold < 0.0 ? 0.0 : threshold)
{
}

Signal ThresholdStrategy::evaluate(double imbalance) const
{
    if (imbalance >= threshold_) {
        return Signal::Buy;
    }
    if (imbalance <= -threshold_) {
        return Signal::Sell;
    }
    return Signal::Hold;
}

double ThresholdStrategy::threshold() const
{
    return threshold_;
}

} // namespace nkm
