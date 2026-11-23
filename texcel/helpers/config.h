/**
 * @file config.h
 * @ingroup texcel
 * @brief Configuración persistente y gestor de rutas de hojas (`output/sheets/`).
 */

#ifndef __TEXCEL_HELPERS_CONFIG_H__
#define __TEXCEL_HELPERS_CONFIG_H__

#include "../../containers/vector.h"
#include "../../types.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/**
 * @class TexcelConfig
 * @brief Gestiona el archivo `.config`, el modo de autoguardado y el listado de hojas en `output/sheets/`.
 */
class TexcelConfig {
public:
    using StringVec = Vector<VectorAscTraits<std::string>>;

    static constexpr const char* SHEET_EXT = ".txl";

    std::string sheets_dir    = "output/sheets";
    std::string current_sheet = "default";
    bool        autosave      = false;
    StringVec   sheet_files;

    std::filesystem::path sheetPath(const std::string& name) const {
        return std::filesystem::path(sheets_dir) / (name + SHEET_EXT);
    }

    void ensureSheetsDir() const {
        std::error_code ec;
        std::filesystem::create_directories(sheets_dir, ec);
    }

    void loadConfig() {
        ensureSheetsDir();
        std::ifstream ifs(std::filesystem::path(sheets_dir) / ".config");
        if (!ifs) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.starts_with("autosave=")) {
                autosave = (line.substr(9) == "1");
            } else if (line.starts_with("current=")) {
                std::string name = line.substr(8);
                if (!name.empty()) current_sheet = name;
            }
        }
    }

    void saveConfig() const {
        ensureSheetsDir();
        std::ofstream ofs(std::filesystem::path(sheets_dir) / ".config", std::ios::trunc);
        if (!ofs) return;
        ofs << "autosave=" << (autosave ? "1" : "0") << "\n";
        ofs << "current=" << current_sheet << "\n";
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
};

#endif // __TEXCEL_HELPERS_TUI_CONFIG_H__
