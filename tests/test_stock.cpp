#include "../src/stock.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

bool approx(double a, double b) { return std::abs(a - b) < 1e-8; }

int main() {
    stock::CsvStockRepository csv;
    stock::MovingAverageModel model;
    std::istringstream data(
        "Date,Open,Close,Volume\n"
        "2026-01-05,0,18,1\n"
        "2026-01-01,0,10,1\n"
        "2026-01-02,0,12,1\n"
        "2026-01-03,0,14,1\n"
        "2026-01-04,0,16,1\n");
    const auto records = csv.load(data);
    assert(records.size() == 5);
    assert(records.front().date == "2026-01-01");
    const auto r = model.analyze(records, 3, 3);
    assert(approx(r.latestClose, 18.0));
    assert(approx(r.latestSma, 16.0));
    assert(approx(r.dailySmaSlope, 2.0));
    assert(r.trend == "Upward");
    assert(!r.smaSeries[0].has_value());
    assert(approx(*r.smaSeries[2], 12.0));
    assert(approx(r.forecasts[0].predictedClose, 20.0));
    assert(approx(r.forecasts[2].predictedClose, 24.0));
    assert(stock::toJson(r).find("\"predicted_close\":24") != std::string::npos);
    bool caught = false;
    try { model.analyze(records, 5, 3); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    caught = false;
    try {
        std::istringstream invalid("Date,Close\n2026-01-01,-5\n");
        (void)csv.load(invalid);
    } catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    caught = false;
    try {
        std::istringstream dup("Date,Close\n2026-01-01,5\n2026-01-01,6\n");
        (void)csv.load(dup);
    } catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    std::cout << "PASS: CSV sorting/validation, SMA, trend, prediction, and JSON\n";
}
