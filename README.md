# Stock Price Prediction (Basic) - CSE 202 OOP with C++

A C++17 project for **CSE 202: Object Oriented Programming with C++**. Reads historical data from a CSV file, computes a simple moving average (SMA), classifies the current trend, and produces an illustrative N-step linear extrapolation. Includes an optional **local** web interface using a native C++ server.

> **Educational demonstration only.** `data/sample_stock.csv` consists of 90 **synthetic sample observations**, not real market prices. Results have not been validated using historical backtesting and are not investment guidance.

## Folder structure

- `src/stock.hpp`: interfaces, abstract classes, and data structures
- `src/stock.cpp`: CSV validation, moving averages, forecast, JSON output
- `src/main.cpp`: C++ command-line application
- `src/server.cpp`: optional C++ localhost HTTP server
- `web/`: website interface (HTML, CSS, JavaScript; model remains in C++)
- `data/sample_stock.csv`: synthetic example
- `tests/test_stock.cpp`: automated C++ unit tests

## Requirements

C++17 compiler such as GCC/g++ (no C++ third-party libraries). Windows CMD, Linux, or macOS supported when a compatible compiler is available.

## Build and run: Windows CMD

Open CMD in this repository folder. Verify `g++ --version`, then:

```bat
g++ -std=c++17 -O2 -Wall -Wextra src\stock.cpp src\main.cpp -o stock_predictor.exe
stock_predictor.exe data\sample_stock.csv 7 5
```

Parameters: `7` = SMA window; `5` = number of future *observations*. Window allowed 2-60; forecast steps 1-30. At least window + 1 valid data rows required.

Run tests:

```bat
g++ -std=c++17 -O2 src\stock.cpp tests\test_stock.cpp -o test_stock.exe
test_stock.exe
```

## Build and run: Linux/macOS

```sh
g++ -std=c++17 -O2 -Wall -Wextra src/stock.cpp src/main.cpp -o stock_predictor
./stock_predictor data/sample_stock.csv 7 5
g++ -std=c++17 -O2 src/stock.cpp tests/test_stock.cpp -o test_stock
./test_stock
```

## Optional local website

On Windows CMD:

```bat
g++ -std=c++17 -O2 -Wall -Wextra src\stock.cpp src\server.cpp -o stock_website.exe -lws2_32
stock_website.exe
```

Open `http://127.0.0.1:8080` (localhost only). On Linux/macOS, the original `build_linux.sh` builds the website too. The frontend uses HTML/CSS/JS, while **prediction and CSV handling are performed by C++**.

## Algorithm

1. Read CSV with `Date` in `YYYY-MM-DD` format and a positive finite `Close` field.
2. Validate, sort by date, reject duplicate dates and malformed prices.
3. Compute SMA using a sliding O(n) running sum: `SMA_t = (1/w) * sum of latest w closes`.
4. Compute SMA slope: `s = SMA_t - SMA_(t-1)`.
5. Set trend to **Upward**, **Downward**, or **Sideways** using a 0.05%-of-latest-SMA slope threshold.
6. Project steps: `prediction(h) = max(0.01, last_close + h*s)`.

This is a **statistical heuristic**, not machine learning or an empirically tested price predictor.

## OOP topics

- **Abstraction / inheritance / runtime polymorphism:** `StockRepository` -> `CsvStockRepository`, `ForecastModel` -> `MovingAverageModel`, called via base-class `unique_ptr`.
- **Encapsulation / modularity:** CSV helper functions are local to implementation and model logic is separated from I/O.
- **C++ STL and safety:** vector, optional, sort, exceptions and file streams.

## Verified sample run

- Data points: 90 synthetic observations
- Last date: 2026-09-11
- Last close: 145.07
- Seven-observation SMA: 148.13
- One-step SMA change: -0.76
- Classified trend: Downward
- First projected price: 144.31

## Notes on the report and slides

The university report template is supplied separately and is **not** part of this code repository unless you wish to add it. Replace personal-information fields, confirm the academic year with your instructor, and publish this folder to your own GitHub account before inserting a real repository URL into the report or PPT.
