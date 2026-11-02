#pragma once

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace stock {

struct StockRecord {
    std::string date;
    double close;
};

struct ForecastPoint {
    int step;
    double predictedClose;
};

struct AnalysisResult {
    std::string modelName;
    std::size_t window;
    double latestClose;
    double latestSma;
    double dailySmaSlope;
    double dailyChangePercent;
    std::string trend;
    std::vector<StockRecord> history;
    std::vector<std::optional<double>> smaSeries;
    std::vector<ForecastPoint> forecasts;
};

// Abstraction and polymorphism: other input formats could implement this class.
class StockRepository {
public:
    virtual ~StockRepository() = default;
    virtual std::vector<StockRecord> load(std::istream& input) const = 0;
};

// Single responsibility: reading and validating historical prices.
class CsvStockRepository final : public StockRepository {
public:
    std::vector<StockRecord> load(std::istream& input) const override;
};

// Abstraction and polymorphism: future models can implement this interface.
class ForecastModel {
public:
    virtual ~ForecastModel() = default;
    virtual std::string name() const = 0;
    virtual AnalysisResult analyze(const std::vector<StockRecord>& prices,
                                   std::size_t window, int horizon) const = 0;
};

class MovingAverageModel final : public ForecastModel {
public:
    std::string name() const override;
    AnalysisResult analyze(const std::vector<StockRecord>& prices,
                           std::size_t window, int horizon) const override;
};

std::string toJson(const AnalysisResult& report);
std::string jsonEscape(const std::string& input);

} // namespace stock
