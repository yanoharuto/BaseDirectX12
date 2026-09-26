#pragma once

#include <iostream>
#include <fstream>
#include <string>


class FilePrint {
    FilePrint();
    public:
        static void save_error(const std::string file, int line, const std::string message);
};
