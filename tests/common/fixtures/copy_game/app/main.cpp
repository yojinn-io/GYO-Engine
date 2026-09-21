#include "gyo/AppConfig.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc, char** argv) {
    if (argc < 1) return 2;
    const auto root = std::filesystem::absolute(argv[0]).parent_path();
    std::ifstream input(root / Gyo::AppConfig::Assets / "marker.txt");
    std::string value;
    std::getline(input, value);
    std::cout << Gyo::AppConfig::Id << ':' << value << '\n';
    return value.empty() ? 1 : 0;
}
