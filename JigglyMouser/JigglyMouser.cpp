#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>

int screenWidth = GetSystemMetrics(SM_CXSCREEN);    // Screen Width Variable
int screenHeight = GetSystemMetrics(SM_CYSCREEN);   // Screen Height Variable
int centerX = screenWidth / 2;                      // Doesnt really need explained
int centerY = screenHeight / 2;                     // Also doesnt need explained
int radius = 200;                                   // Bounds distance from the center for the mouse movements
int intervalSeconds, startDelay;                    // If you used the program you would know what these are

std::atomic<bool> running(true);

void moveMousePeriodically(int intervalSeconds) {
    
    std::random_device rd;      // Sets random_device to the rd variable
    std::mt19937 gen(rd());     // But Mother, i dont want to explain this
    std::uniform_real_distribution<double> angleDist(0, 2 * 3.14159265358979323846);    // oooooo fancy math i learned in Jr.High
    std::uniform_real_distribution<double> radiusDist(0, radius);                       // some times they ask you if you're fine

    while (running) {
        double angle = angleDist(gen);      // Double persision float angle variable that equals angleDist
        double distance = radiusDist(gen);  // Double distance is what they call people not allowed near school zones
        int newX = static_cast<int>(centerX + distance * cos(angle));   // newX? first it was twitter, then X, and now its newX?
        int newY = static_cast<int>(centerY + distance * sin(angle));   // ooooooo more math that i forgot from highschool

        SetCursorPos(newX, newY);   // does this need explained
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));     // I just like adding fluff to my code to make it look fuller
    }
}

int main() {
    std::cout << "Enter the seconds between mouse movements: ";     // Prints a message to the screen
    std::cin >> intervalSeconds;                                    // cin stands for "character input" its input for "int intervalSeconds"
    std::cout << "Enter the delay /s before starting: ";            // Prints another message to the screen
    std::cin >> startDelay;                                         // is input for "int startDelay"

    std::this_thread::sleep_for(std::chrono::seconds(startDelay));  // this is a very sleepy line of code. you should sub to https://www.patreon.com/c/LillyPK so i can buy line 42's coffee

    std::thread mouseMover(moveMousePeriodically, intervalSeconds); // my hands hurt

    std::cout << "Press Enter to stop the program...\n"; // prints a message to the screen
    std::cin.get();
    std::cin.get(); // look this was someone on stack overflows fix and it works. dont ask how, why, when, where, who. just know it works

    running = false;
    if (mouseMover.joinable()) {
        mouseMover.join();
    }

    return 0;
}
