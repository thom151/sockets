#include <iostream>
#include <thread>
#include <chrono>

void backgroundTask() {
    for (int i = 0; i < 5; i++) {
        std::cout << "Background task: " << i << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    std::thread worker(backgroundTask);

    for (int i = 0; i < 5; i++) {
        std::cout << "MAIN: " << i << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    worker.join();

    std::cout << "Finished!\n";
}
