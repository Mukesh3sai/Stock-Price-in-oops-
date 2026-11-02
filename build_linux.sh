#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra src/stock.cpp src/main.cpp -o stock_predictor
g++ -std=c++17 -O2 -Wall -Wextra src/stock.cpp src/server.cpp -o stock_website
g++ -std=c++17 -O2 src/stock.cpp tests/test_stock.cpp -o test_stock
./test_stock
echo 'Build succeeded. Run ./stock_website to open the local website.'
