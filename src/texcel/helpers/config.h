/**
 * @file config.h
 * @ingroup texcel
 * @brief Configuración persistente, anchos de columna por hoja y gestor de rutas (`output/sheets/`).
 */

#ifndef __TEXCEL_HELPERS_CONFIG_H__
#define __TEXCEL_HELPERS_CONFIG_H__

#include "../../containers/vector.h"
#include "../../types.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

/**
 * @class TexcelConfig
 * @brief Gestiona `.config` (autoguardado, hoja activa y anchos de columna por hoja) y el listado `.txl`.
 */
class TexcelConfig {
public:
    using StringVec = Vector<VectorAscTraits<std::string>>;
    using ColWidthMap = std::map<TX, TX>;

    static constexpr const char* SHEET_EXT = ".txl";
    static constexpr TX MIN_COL_WIDTH = 5;
    static constexpr TX MAX_COL_WIDTH = 38;
    static constexpr TX DEFAULT_COL_WIDTH = 9;

    std::string sheets_dir    = "output/sheets";
    std::string current_sheet = "default";
    bool        autosave      = false;
    StringVec   sheet_files;
    std::map<std::string, ColWidthMap> sheet_col_widths;

    std::filesystem::path sheetPath(const std::string& name) const {
        return std::filesystem::path(sheets_dir) / (name + SHEET_EXT);
    }

    void ensureSheetsDir() const {
        std::error_code ec;
        std::filesystem::create_directories(sheets_dir, ec);
    }

    TX colWidth(const std::string& sheet, TX col, TX def_w = DEFAULT_COL_WIDTH) const {
        auto sit = sheet_col_widths.find(sheet);
        if (sit == sheet_col_widths.end()) return def_w;
        auto cit = sit->second.find(col);
        return (cit != sit->second.end()) ? cit->second : def_w;
    }

    TX activeColWidth(TX col, TX def_w = DEFAULT_COL_WIDTH) const {
        return colWidth(current_sheet, col, def_w);
    }

    void setColWidth(const std::string& sheet, TX col, TX w, TX def_w = DEFAULT_COL_WIDTH) {
        if (col < 0) return;
        TX clamped = std::clamp(w, MIN_COL_WIDTH, MAX_COL_WIDTH);
        auto& mp = sheet_col_widths[sheet];
        if (clamped == def_w) {
            mp.erase(col);
            if (mp.empty()) sheet_col_widths.erase(sheet);
        } else {
            mp[col] = clamped;
        }
    }

    void resetSheetColWidths(const std::string& sheet) {
        sheet_col_widths.erase(sheet);
    }

    void loadConfig() {
        ensureSheetsDir();
        sheet_col_widths.clear();
        std::ifstream ifs(std::filesystem::path(sheets_dir) / ".config");
        if (!ifs) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("autosave=")) {
                autosave = (line.substr(9) == "1");
            } else if (line.starts_with("current=")) {
                std::string name = line.substr(8);
                if (!name.empty()) current_sheet = name;
            } else if (line.starts_with("colw.")) {
                size_t eq = line.find('=');
                if (eq == std::string::npos || eq <= 5) continue;
                std::string sheet = line.substr(5, eq - 5);
                std::string spec = line.substr(eq + 1);
                parseColWidthSpec(sheet, spec);
            }
        }
    }

    void saveConfig() const {
        ensureSheetsDir();
        std::filesystem::path final_p = std::filesystem::path(sheets_dir) / ".config";
        std::filesystem::path tmp_p   = std::filesystem::path(sheets_dir) / ".config.tmp";
        {
            std::ofstream ofs(tmp_p, std::ios::trunc);
            if (!ofs) return;
            ofs << "autosave=" << (autosave ? "1" : "0") << "\n";
            ofs << "current=" << current_sheet << "\n";
            for (const auto& [sheet, mp] : sheet_col_widths) {
                if (mp.empty()) continue;
                ofs << "colw." << sheet << "=";
                bool first = true;
                for (const auto& [col, w] : mp) {
                    if (!first) ofs << ",";
                    ofs << col << ":" << w;
                    first = false;
                }
                ofs << "\n";
            }
            ofs.flush();
        }
        std::error_code ec;
        std::filesystem::rename(tmp_p, final_p, ec);
        if (ec) {
            std::filesystem::copy_file(tmp_p, final_p, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmp_p, ec);
        }
    }

    void refreshSheetList() {
        ensureSheetsDir();
        sheet_files.clear();
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(sheets_dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == SHEET_EXT) {
                names.push_back(entry.path().stem().string());
            }
        }
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            sheet_files.push_back(name);
        }
    }

private:
    void parseColWidthSpec(const std::string& sheet, std::string_view spec) {
        while (!spec.empty()) {
            size_t comma = spec.find(',');
            std::string_view item = (comma == std::string_view::npos) ? spec : spec.substr(0, comma);
            size_t colon = item.find(':');
            if (colon != std::string_view::npos && colon > 0 && colon + 1 < item.size()) {
                try {
                    TX col = static_cast<TX>(std::stol(std::string(item.substr(0, colon))));
                    TX w   = static_cast<TX>(std::stol(std::string(item.substr(colon + 1))));
                    setColWidth(sheet, col, w);
                } catch (...) {
                }
            }
            if (comma == std::string_view::npos) break;
            spec.remove_prefix(comma + 1);
        }
    }
};

#endif // __TEXCEL_HELPERS_CONFIG_H__
