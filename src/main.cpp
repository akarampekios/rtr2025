#include <iostream>
#include <memory>
#include "Application.h"

int main() {
    try {
        auto app = std::make_unique<Application>();
        app->run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

