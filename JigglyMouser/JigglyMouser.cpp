#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>
#include <mutex>

//int screenWidth = GetSystemMetrics(SM_CXSCREEN);    // Screen Width Variable
//int screenHeight = GetSystemMetrics(SM_CYSCREEN);   // Screen Height Variable
//int centerX = screenWidth / 2;                      // Doesnt really need explained
//int centerY = screenHeight / 2;                     // Also doesnt need explained
//int radius = 200;                                   // Bounds distance from the center for the mouse movements


int centerX = GetSystemMetrics(SM_CXSCREEN) / 2;
int centerY = GetSystemMetrics(SM_CYSCREEN) / 2;
int intervalSeconds, startDelay;                    // If you used the program you would know what these are

std::atomic<bool> running(true);
std::atomic<bool> programMoving(false);             // Track if program is currently moving mouse
std::atomic<int> delayCounter(0);                   // Counter for delay period
std::mutex posMutex;                                // Mutex for position tracking

POINT lastKnownPos;                                 // Last position set by program

void monitorMouseMovement() {
    POINT currentPos;
    GetCursorPos(&currentPos);
    lastKnownPos = currentPos;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Check every 100ms

        GetCursorPos(&currentPos);

        // Check if mouse moved AND it wasn't moved by the program
        bool movedByUser = false;
        {
            std::lock_guard<std::mutex> lock(posMutex);
            if ((currentPos.x != lastKnownPos.x || currentPos.y != lastKnownPos.y)) {
                movedByUser = true;
                lastKnownPos = currentPos;
            }
        }

        if (movedByUser) {
            if (programMoving) {
                std::cout << "\nUser movement detected! Stopping program and resetting delay...\n";
                programMoving = false;
            }
            // Reset counter regardless of program state
            delayCounter = 0;
        }
    }
}

void moveMousePeriodically(int intervalSeconds) {
    std::random_device rd;      // Sets random_device to the rd variable
    std::mt19937 gen(rd());     // But Mother, i dont want to explain this
    std::uniform_real_distribution<double> angleDist(0, 2 * 3.14159265358979323846);    // oooooo fancy math i learned in Jr.High
    std::uniform_real_distribution<double> radiusDist(0, 200);                       // some times they ask you if you're fine

    // Initial delay countdown
    std::cout << "Starting delay countdown:\n";
    while (running && delayCounter < startDelay) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        delayCounter++;

        if (delayCounter % 10 == 0 || delayCounter == startDelay) {
            std::cout << delayCounter << " seconds elapsed...\n";
        }
    }

    if (!running) return;

    std::cout << "Program now moving mouse every " << intervalSeconds << " seconds.\n";
    programMoving = true;

    while (running) {
        // Wait for delay period if program was stopped
        while (running && !programMoving) {
            delayCounter = 0;
            std::cout << "Waiting for " << startDelay << " seconds of inactivity...\n";

            while (running && delayCounter < startDelay) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                int currentCount = ++delayCounter;

                if (currentCount % 10 == 0 || currentCount == startDelay) {
                    std::cout << currentCount << " seconds of inactivity...\n";
                }
            }

            if (delayCounter >= startDelay && running) {
                std::cout << "Resuming mouse movements.\n";
                programMoving = true;
                break;
            }
        }

        if (!running) break;

        // Move mouse
        double angle = angleDist(gen);      // Double persision float angle variable that equals angleDist
        double distance = radiusDist(gen);  // Double distance is what they call people not allowed near school zones
        int newX = static_cast<int>(centerX + distance * cos(angle));   // newX? first it was twitter, then X, and now its newX?
        int newY = static_cast<int>(centerY + distance * sin(angle));   // ooooooo more math that i forgot from highschool

        {
            std::lock_guard<std::mutex> lock(posMutex);
            SetCursorPos(newX, newY);
            lastKnownPos.x = newX;
            lastKnownPos.y = newY;
        }

        // Sleep in small chunks to remain responsive
        for (int i = 0; i < intervalSeconds && running && programMoving; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

int main() {
    std::cout << "Enter the seconds between mouse movements: ";     // Prints a message to the screen
    std::cin >> intervalSeconds;                                    // cin stands for "character input" its input for "int intervalSeconds"
    std::cout << "Enter the delay /s before starting: ";            // Prints another message to the screen
    std::cin >> startDelay;                                         // is input for "int startDelay"

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
