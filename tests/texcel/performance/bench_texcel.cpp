#include "../../../src/texcel/engine.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

struct BenchResult {
    std::string name;
    double load_ms   = 0.0;
    double recalc_ms = 0.0;
    double edit_ms   = 0.0;
    double clear_ms  = 0.0;
    double total_ms() const { return load_ms + recalc_ms + edit_ms + clear_ms; }
};

static double elapsedMs(std::chrono::steady_clock::time_point a,
                        std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

static BenchResult runScenario(const std::string& name,
                               const std::string& txl_path,
                               const std::string& origin_cell,
                               const std::string& new_origin_val,
                               const std::string& last_cell,
                               const std::string& expected_before,
                               const std::string& expected_after) {
    BenchResult res{name};
    SpreadsheetEngine e;

    std::ifstream ifs(txl_path);
    assert(ifs.good() && "No se pudo abrir el archivo .txl de rendimiento");

    auto t0 = std::chrono::steady_clock::now();
    ifs >> e.rawTable();
    auto t1 = std::chrono::steady_clock::now();
    res.load_ms = elapsedMs(t0, t1);

    auto t2 = std::chrono::steady_clock::now();
    e.recalculateAll();
    auto t3 = std::chrono::steady_clock::now();
    res.recalc_ms = elapsedMs(t2, t3);

    std::string got_before = e.getCell(last_cell).displayValue();
    if (got_before != expected_before) {
        std::cerr << "[FAIL] " << name << " initial " << last_cell
                  << ": got=" << got_before << " expected=" << expected_before << "\n";
        assert(false);
    }

    auto t4 = std::chrono::steady_clock::now();
    e.setCell(origin_cell, new_origin_val);
    auto t5 = std::chrono::steady_clock::now();
    res.edit_ms = elapsedMs(t4, t5);

    std::string got_after = e.getCell(last_cell).displayValue();
    if (got_after != expected_after) {
        std::cerr << "[FAIL] " << name << " after edit " << last_cell
                  << ": got=" << got_after << " expected=" << expected_after << "\n";
        assert(false);
    }

    auto t6 = std::chrono::steady_clock::now();
    e.clear();
    auto t7 = std::chrono::steady_clock::now();
    res.clear_ms = elapsedMs(t6, t7);

    return res;
}

int main() {
    BenchResult simple = runScenario(
        "Simple (20,000 celdas)",
        "tmp/perf_simple.txl",
        "A1", "10", "GR100", "234", "243"
    );

    BenchResult mixed = runScenario(
        "Mixta (20,186 celdas)",
        "tmp/perf_mixed.txl",
        "A1", "20", "F103", "11071", "12625"
    );

    double grand_total = simple.total_ms() + mixed.total_ms();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n=== Texcel Performance Benchmark (-O2) ===\n";
    std::cout << std::left << std::setw(26) << "Escenario"
              << std::right << std::setw(12) << "Load (ms)"
              << std::setw(14) << "Recalc (ms)"
              << std::setw(14) << "Edit->Fin"
              << std::setw(12) << "Clear (ms)"
              << std::setw(14) << "Total (ms)" << "\n";
    std::cout << std::string(92, '-') << "\n";

    auto printRow = [](const BenchResult& r) {
        std::cout << std::left << std::setw(26) << r.name
                  << std::right << std::setw(12) << r.load_ms
                  << std::setw(14) << r.recalc_ms
                  << std::setw(14) << r.edit_ms
                  << std::setw(12) << r.clear_ms
                  << std::setw(14) << r.total_ms() << "\n";
    };
    printRow(simple);
    printRow(mixed);
    std::cout << std::string(92, '-') << "\n";
    std::cout << std::left << std::setw(78) << "SUMA TOTAL (Simple + Mixta)"
              << std::right << std::setw(14) << grand_total << " ms\n";

    std::cout << "\n[PASS] bench_texcel\n";
    return 0;
}
