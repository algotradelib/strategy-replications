# strategy_replications

A strategy built on [algotradelib](https://github.com/ygeunkim/algotradelib), replayed beside the library's own.

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/strategy_replications
```

The generated program needs no credentials and reaches no network: its bars are built in code, so it
prints the same table on every machine.

## Where your code goes

`src/main.cpp` holds `MyStrategy`. That struct is yours; everything around it is the harness that
feeds it bars and prints the comparison. Two things to keep when you rewrite it:

- **Handlers stay templates.** `template <class Ctx> void on_bar(Ctx&, const strategy::BarEvent&)`
  is what lets one strategy run in a backtest, in a dry run and in a live session without being
  rewritten. `ATL_ASSERT_STRATEGY` checks it at compile time.
- **Emit a target position, not an order.** A strategy emitting orders has to remember whether it
  already entered, and after a restart it does not know. Stating the position you want leaves that to
  reconciliation, which can see the portfolio and the working orders.

## Your own data

This library does not read files, on purpose: bars come from a broker adapter or from your own code.
Replace `closes()` with your history, keeping prices in `Decimal64` so nothing rounds on the way in.
See "Using it" on the library's documentation main page (`doxygen/index.md`) for the two routes.
