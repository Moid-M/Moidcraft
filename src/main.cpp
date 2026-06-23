#include "core/engine.hpp"
#include <iostream>
#include <cstdlib>

int main() {
    try {
        Engine engine;
        engine.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
