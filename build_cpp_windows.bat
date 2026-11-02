@echo off
setlocal
cd /d "%~dp0"
g++ -std=c++17 -O2 -Wall -Wextra src\stock.cpp src\main.cpp -o stock_predictor.exe
if errorlevel 1 (
  echo Build failed. Install a C++17 g++ compiler first.
  exit /b 1
)
echo Built stock_predictor.exe successfully.
echo Run: stock_predictor.exe data\sample_stock.csv 7 5
endlocal
