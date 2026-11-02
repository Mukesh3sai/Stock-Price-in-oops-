#include "stock.hpp"

#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: stock_predictor <CSV_FILE> [SMA_WINDOW=7] [FUTURE_STEPS=5]\n"
                  << "Example: stock_predictor data/sample_stock.csv 7 5\n";
        return 0;
    }
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: stock_predictor <CSV_FILE> [SMA_WINDOW=7] [FUTURE_STEPS=5]\n";
        return 1;
    }
    try {
        const std::string filePath = argv[1];
        const int window = argc >= 3 ? std::stoi(argv[2]) : 7;
        const int days = argc >= 4 ? std::stoi(argv[3]) : 5;
        if (window < 2 || window > 60 || days < 1 || days > 30) {
            throw std::runtime_error("Window: 2..60; future steps: 1..30");
        }
        std::ifstream input(filePath);
        if (!input) throw std::runtime_error("Cannot open CSV: " + filePath);

        // OOP polymorphism: repository and model are used through abstract base types.
        std::unique_ptr<stock::StockRepository> repository =
            std::make_unique<stock::CsvStockRepository>();
        std::unique_ptr<stock::ForecastModel> model =
            std::make_unique<stock::MovingAverageModel>();
        const auto history = repository->load(input);
        const auto result = model->analyze(history, static_cast<std::size_t>(window), days);

        std::cout << "\n=============================================\n"
                  << " STOCK PRICE PREDICTION - C++ OOP PROJECT\n"
                  << "=============================================\n"
                  << "Model              : " << result.modelName << '\n'
                  << "Observations       : " << result.history.size() << '\n'
                  << "Latest date        : " << result.history.back().date << '\n'
                  << std::fixed << std::setprecision(2)
                  << "Latest close       : " << result.latestClose << '\n'
                  << "SMA (" << window << " observations)     : " << result.latestSma << '\n'
                  << "One-step SMA slope : " << result.dailySmaSlope << '\n'
                  << "Recent trend       : " << result.trend << '\n'
                  << "Last-close change  : " << result.dailyChangePercent << "%\n"
                  << "\nIllustrative forecast (future observations, NOT calendar dates):\n";
        for (const auto& forecast : result.forecasts) {
            std::cout << "  Step +" << forecast.step << " : " << forecast.predictedClose << '\n';
        }
        std::cout << "\nMethod: future Close = last Close + step * (latest SMA - previous SMA)\n"
                  << "WARNING: Educational heuristic, not a reliable market forecast.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
