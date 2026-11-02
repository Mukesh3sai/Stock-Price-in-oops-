#include "stock.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace stock {
namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

// Handles quoted cells containing commas and escaped double quotes.
std::vector<std::string> splitCsvRow(const std::string& row) {
    std::vector<std::string> cells;
    std::string current;
    bool inQuotes = false;
    for (std::size_t i = 0; i < row.size(); ++i) {
        const char ch = row[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < row.size() && row[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            cells.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (inQuotes) throw std::runtime_error("Unclosed quote in CSV row");
    cells.push_back(trim(current));
    return cells;
}

bool validIsoDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(date[i]))) return false;
    }
    const int year = std::stoi(date.substr(0, 4));
    const int month = std::stoi(date.substr(5, 2));
    const int day = std::stoi(date.substr(8, 2));
    if (year < 1900 || month < 1 || month > 12 || day < 1) return false;
    const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    const int monthDays[12] = {31, leap ? 29 : 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
    return day <= monthDays[month - 1];
}

} // namespace

std::vector<StockRecord> CsvStockRepository::load(std::istream& input) const {
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("CSV file is empty");
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3); // UTF-8 BOM
    }
    const auto headers = splitCsvRow(line);
    int dateColumn = -1;
    int closeColumn = -1;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        const auto header = lower(trim(headers[i]));
        if (header == "date") dateColumn = static_cast<int>(i);
        if (header == "close") closeColumn = static_cast<int>(i);
    }
    if (dateColumn == -1 || closeColumn == -1) {
        throw std::runtime_error("CSV must have 'Date' and 'Close' columns");
    }

    std::vector<StockRecord> records;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) continue;
        std::vector<std::string> cells;
        try {
            cells = splitCsvRow(line);
        } catch (const std::exception& ex) {
            throw std::runtime_error("Line " + std::to_string(lineNumber) + ": " + ex.what());
        }
        if (static_cast<int>(cells.size()) <= std::max(dateColumn, closeColumn)) {
            throw std::runtime_error("Line " + std::to_string(lineNumber) + ": missing Date or Close");
        }
        const std::string date = trim(cells[dateColumn]);
        if (!validIsoDate(date)) {
            throw std::runtime_error("Line " + std::to_string(lineNumber) +
                                     ": use an ISO date such as 2026-09-15");
        }
        const std::string value = trim(cells[closeColumn]);
        double close = 0.0;
        std::size_t used = 0;
        try {
            close = std::stod(value, &used);
        } catch (const std::exception&) {
            throw std::runtime_error("Line " + std::to_string(lineNumber) + ": invalid Close price");
        }
        if (used != value.size() || !std::isfinite(close) || close <= 0.0) {
            throw std::runtime_error("Line " + std::to_string(lineNumber) +
                                     ": Close must be a positive finite number");
        }
        records.push_back({date, close});
    }
    if (records.empty()) throw std::runtime_error("CSV has no price rows");
    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) {
        return a.date < b.date;
    });
    for (std::size_t i = 1; i < records.size(); ++i) {
        if (records[i - 1].date == records[i].date) {
            throw std::runtime_error("Duplicate date: " + records[i].date);
        }
    }
    return records;
}

std::string MovingAverageModel::name() const { return "Simple Moving Average + Trend"; }

AnalysisResult MovingAverageModel::analyze(const std::vector<StockRecord>& prices,
                                           const std::size_t window,
                                           const int horizon) const {
    if (window < 2 || window > 60) throw std::runtime_error("Moving-average window must be from 2 to 60");
    if (horizon < 1 || horizon > 30) throw std::runtime_error("Forecast horizon must be from 1 to 30");
    if (prices.size() < window + 1) {
        throw std::runtime_error("Need at least window + 1 rows; upload more history or lower the window");
    }
    AnalysisResult report{};
    report.modelName = name();
    report.window = window;
    report.history = prices;
    report.smaSeries.resize(prices.size());

    // Running sum: SMA for each row in O(n), avoiding repeated summation.
    double sum = 0.0;
    for (std::size_t i = 0; i < prices.size(); ++i) {
        sum += prices[i].close;
        if (i >= window) sum -= prices[i - window].close;
        if (i + 1 >= window) report.smaSeries[i] = sum / static_cast<double>(window);
    }
    report.latestClose = prices.back().close;
    report.latestSma = *report.smaSeries.back();
    const double previousSma = *report.smaSeries[prices.size() - 2];
    report.dailySmaSlope = report.latestSma - previousSma;
    report.dailyChangePercent = 100.0 * (prices.back().close - prices[prices.size() - 2].close)
                                / prices[prices.size() - 2].close;

    // Ignore slopes smaller than 0.05% of current SMA per observation.
    const double threshold = report.latestSma * 0.0005;
    report.trend = report.dailySmaSlope > threshold ? "Upward" :
                   report.dailySmaSlope < -threshold ? "Downward" : "Sideways";

    // Illustrative extrapolation (not a validated stock-price prediction):
    // forecast h = last actual Close + h * (last SMA - previous SMA).
    for (int h = 1; h <= horizon; ++h) {
        const double projected = std::max(0.01, report.latestClose +
                                                 h * report.dailySmaSlope);
        report.forecasts.push_back({h, projected});
    }
    return report;
}

std::string jsonEscape(const std::string& input) {
    std::ostringstream output;
    for (unsigned char ch : input) {
        switch (ch) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (ch < 32) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(ch) << std::dec;
                } else {
                    output << static_cast<char>(ch);
                }
        }
    }
    return output.str();
}

std::string toJson(const AnalysisResult& report) {
    std::ostringstream out;
    out << std::setprecision(12);
    out << "{\"model\":\"" << jsonEscape(report.modelName) << "\""
        << ",\"window\":" << report.window
        << ",\"latest_close\":" << report.latestClose
        << ",\"latest_sma\":" << report.latestSma
        << ",\"slope\":" << report.dailySmaSlope
        << ",\"change_pct\":" << report.dailyChangePercent
        << ",\"trend\":\"" << jsonEscape(report.trend) << "\""
        << ",\"history\":[";
    for (std::size_t i = 0; i < report.history.size(); ++i) {
        if (i) out << ',';
        out << "{\"date\":\"" << jsonEscape(report.history[i].date) << "\""
            << ",\"close\":" << report.history[i].close
            << ",\"sma\":";
        if (report.smaSeries[i]) out << *report.smaSeries[i];
        else out << "null";
        out << '}';
    }
    out << "],\"forecasts\":[";
    for (std::size_t i = 0; i < report.forecasts.size(); ++i) {
        if (i) out << ',';
        out << "{\"day\":" << report.forecasts[i].step
            << ",\"predicted_close\":" << report.forecasts[i].predictedClose << '}';
    }
    out << "]}";
    return out.str();
}

} // namespace stock
