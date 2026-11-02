@echo off
setlocal
cd /d "%~dp0"
echo Compiling C++ console program...
g++ -std=c++17 -O2 -Wall -Wextra src\stock.cpp src\main.cpp -o stock_predictor.exe
if errorlevel 1 goto fail
echo Compiling C++ website server...
g++ -std=c++17 -O2 -Wall -Wextra src\stock.cpp src\server.cpp -o stock_website.exe -lws2_32
if errorlevel 1 goto fail
echo Compiling tests...
g++ -std=c++17 -O2 src\stock.cpp tests\test_stock.cpp -o test_stock.exe
if errorlevel 1 goto fail
echo.
echo SUCCESS: run stock_predictor.exe data\sample_stock.csv 7 5
echo Website: run stock_website.exe and visit http://127.0.0.1:8080
goto end
:fail
echo.
echo Build failed. Ensure a working C++17 g++ compiler is installed and on PATH.
exit /b 1
:end
endlocal
