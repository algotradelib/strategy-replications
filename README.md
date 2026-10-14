# strategy-replications

Replications of [Ernest P. Chan (2013), *Algorithmic Trading: Winning Strategies and Their Rationale*. Wiley](https://www.amazon.com/dp/1118460146?lv=shuf&channelId=500&plpRedirect=mhFallback), using C++.

- [ ] Time series analytics (chapter 2)
  - [ ] Augmented Dickey-Fuller Test
  - [ ] Hurst Exponent and Variance Ratio Test
  - [ ] Half-life of Mean Reversion
  - [ ] Cointegration and Johansen Test for Cointegration
- [ ] Mean Reversion Strategies (chapter 3)
  - [ ] Linear mean-reverting
  - [ ] Bollinger Bands - same as `vanilla::ZScoreReversion`
  - [x] Kalman Filter
- [ ] Mean Reversion of Stocks and ETFs (chapter 4)
  - [ ] Buy-on-Gap Model
  - [ ] Cross-Sectional Linear Long-Short
  - [ ] Arbitrage between an ETF and Component Stocks
- [ ] Interday Momentum Strategies (chapter 6)
  - [ ] Time series momentum
  - [ ] Cross-sectional momentum
- [ ] Mean Reversion of Currencies and Futures (chapter 5)
  - [ ] Trading Currency Cross-Rates
  - [ ] Trading Futures Calendar Spread
  - [ ] Volatility futures versus Equity index futures

```bash
build/bin/atl_strategy -s <strategy_name> -d <data_path> ...
```
