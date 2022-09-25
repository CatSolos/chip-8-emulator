#include <iostream>

#include "vk_engine.h"

int main(int argc, char* argv[]) {
    std::cout << "Starting" << std::endl;

    VulkanEngine engine;

    engine.init();
    engine.run();
    engine.cleanup();
}