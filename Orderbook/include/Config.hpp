#pragma once

namespace Config {
/* -------------------------------------------------------------------------- */
/*                                   Traders                                  */
/* -------------------------------------------------------------------------- */
inline static constexpr int maxOrdersPerTrader =
    1000;  // maxinum number traders per trader to avoid overflow

/* ------------------------------- NoiseTrader ------------------------------ */
inline static constexpr int nPSpread = 40;  // NoiseTrader price spread
inline static constexpr int nQSpread = 100;  // NoiseTrader quantity spread
inline static constexpr int makerP = 0;  // NoiseTrader quantity spread

/* ------------------------------------ UI ----------------------------------- */
inline static constexpr int candleTradesPerCandle = 200;
inline static constexpr int candleMaxCandles = 1000;

}
