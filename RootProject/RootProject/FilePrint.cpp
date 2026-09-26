#include "FilePrint.h"



// ファイル書き込み用の関数
void FilePrint::save_error(const std::string file, int line, const std::string message) {
    // 「ios::app」で追記モードで開く
    std::ofstream error_file("debug.txt", std::ios::app);

    if (error_file.is_open()) {
        error_file << "[" << file << " : " << line << "行目] " << message << std::endl;
    }
}