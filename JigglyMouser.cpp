// JigglyMouser.cpp

#include "mouse_move.hpp"
#include <iostream>
#include <atomic>
#include <thread>

int intervalSeconds, startDelay;        // If you used the program you would know what these are
std::atomic<bool> running(true);

int main() {
    // Get interval seconds with validation
    do {
        std::cout << "Enter the seconds between mouse movements (minimum 5): ";     // Prints a message to the screen
        std::cin >> intervalSeconds;                                                // cin stands for "character input" its input for "int intervalSeconds"

        if (std::cin.fail()) {
            std::cout << "Invalid input. Only enter numbers.\n";
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            intervalSeconds = 0; // Set to invalid value to continue loop
        } else if (intervalSeconds < 5) {
            std::cout << "Invalid input. Must be at least 5 seconds.\n";
        }
    } while (intervalSeconds < 5);

    // Get start delay with validation
    do {
        std::cout << "Enter the delay /s before starting (minimum 5): ";            // Prints another message to the screen
        std::cin >> startDelay;                                                     // is input for "int startDelay"

        if (std::cin.fail()) {
            std::cout << "Invalid input. Only enter numbers.\n";
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            startDelay = 0; // Set to invalid value to continue loop
        } else if (startDelay < 5) {
            std::cout << "Invalid input. Must be at least 5 seconds.\n";
        }
    } while (startDelay < 5);

    std::thread mouseMonitor(monitorMouseMovement);                 // Thread to monitor user mouse movement
    std::thread mouseMover(moveMousePeriodically, intervalSeconds); // my hands hurt

    std::cout << "\nPress Enter to stop the program...\n"; // prints a message to the screen
    std::cin.get();
    std::cin.get(); // look this was someone on stack overflows fix and it works. dont ask how, why, when, where, who. just know it works

    running = false;

    if (mouseMonitor.joinable()) {
        mouseMonitor.join();
    }

    if (mouseMover.joinable()) {
        mouseMover.join();
    }

    return 0;
}
