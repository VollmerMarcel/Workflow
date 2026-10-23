#include "carwear/app/cli.h"

#include <iostream>
#include <locale>

int main(int argc, char** argv) {
    std::cout.imbue(std::locale::classic());
    std::cerr.imbue(std::locale::classic());
    return carwear::run_cli(argc, argv);
}
