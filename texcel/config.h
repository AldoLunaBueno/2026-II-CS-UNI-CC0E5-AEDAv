/**
 * @file config.h
 * @ingroup texcel
 * @brief Configuración persistente y gestor de rutas de hojas (`output/sheets/`).
 */

#ifndef __TEXCEL_CONFIG_H__
#define __TEXCEL_CONFIG_H__

#include "../containers/vector.h"
#include "../types.h"
#include <filesystem>
#include <fstream>
#include <string>

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
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(sheets_dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == SHEET_EXT) {
                sheet_files.push_back(entry.path().stem().string());
            }
        }
        for (size_t i = 0; i < sheet_files.size(); ++i) {
            for (size_t j = i + 1; j < sheet_files.size(); ++j) {
                if (sheet_files[j].getValue() < sheet_files[i].getValue()) {
                    std::string tmp = sheet_files[i].getValue();
                    sheet_files[i].value() = sheet_files[j].getValue();
                    sheet_files[j].value() = tmp;
                }
            }
        }
    }
};

#endif // __TEXCEL_CONFIG_H__
