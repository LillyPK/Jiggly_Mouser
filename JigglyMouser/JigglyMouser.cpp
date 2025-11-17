#include <iostream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>
#include <mutex>

int screenWidth = GetSystemMetrics(SM_CXSCREEN);    // Screen Width Variable
int screenHeight = GetSystemMetrics(SM_CYSCREEN);   // Screen Height Variable
int centerX = screenWidth / 2;                      // Doesnt really need explained
int centerY = screenHeight / 2;                     // Also doesnt need explained
int radius = 200;                                   // Bounds distance from the center for the mouse movements
int intervalSeconds, startDelay;                    // If you used the program you would know what these are

std::atomic<bool> running(true);
std::atomic<bool> programMoving(false);             // Track if program is currently moving mouse
std::atomic<int> delayCounter(0);                   // Counter for delay period
std::atomic<DWORD> lastPenActivity(0);              // Track last pen/tablet activity
std::mutex posMutex;                                // Mutex for position tracking

POINT lastKnownPos;                                 // Last position set by program

HMONITOR getCurrentMonitor() {
    POINT pt;
    GetCursorPos(&pt);
    return MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
}

void updateScreenBounds() {
    HMONITOR hMonitor = getCurrentMonitor();
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfo(hMonitor, &mi)) {
        screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
        centerX = mi.rcMonitor.left + screenWidth / 2;
        centerY = mi.rcMonitor.top + screenHeight / 2;
    }
}

// Returns true if the user has interacted with mouse, keyboard, or tablet very recently
bool userActivityDetected() {
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (!GetLastInputInfo(&lii)) {
        return false;
    }

    DWORD now = GetTickCount();
    DWORD idleMs = now - lii.dwTime;

    // If the user did something within the last 150ms, count it as activity
    if (idleMs < 150) {
        return true;
    }

    // Check for pen/tablet input using GetMessageExtraInfo
    DWORD extraInfo = GetMessageExtraInfo();
    // Signature for pen/touch input (0xFF515700 series)
    // Check if it's pen/stylus input
    if ((extraInfo & 0xFFFFFF00) == 0xFF515700) {
        lastPenActivity = now;
        return true;
    }

    // Also check if pen was used recently
    DWORD timeSinceLastPen = now - lastPenActivity.load();
    if (timeSinceLastPen < 150) {
        return true;
    }

    // Additional check: see if tablet buttons are pressed
    // Wacom tablets often use these as side buttons
    if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000 ||
        GetAsyncKeyState(VK_XBUTTON2) & 0x8000) {
        return true;
    }

    return false;
}

void monitorMouseMovement() {
    POINT currentPos;
    GetCursorPos(&currentPos);
    lastKnownPos = currentPos;
    HMONITOR lastMonitor = getCurrentMonitor();

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Check every 100ms

        GetCursorPos(&currentPos);
        HMONITOR currentMonitor = getCurrentMonitor();

        // Check if monitor changed
        if (currentMonitor != lastMonitor) {
            std::cout << "\nMonitor changed. Updating screen bounds...\n";
            updateScreenBounds();
            lastMonitor = currentMonitor;
        }

        // Check for any global user activity such as keyboard, mouse, or tablet
        bool activity = userActivityDetected();

        if (activity) {
            if (programMoving) {
                std::cout << "\nUser activity detected. Stopping program and resetting delay...\n";
                programMoving = false;
            }
            delayCounter = 0;
        }
    }
}

void moveMousePeriodically(int intervalSeconds) {
    std::random_device rd;      // Sets random_device to the rd variable
    std::mt19937 gen(rd());     // But Mother, i dont want to explain this
    std::uniform_real_distribution<double> angleDist(0, 2 * 3.14159265358979323846);    // oooooo fancy math i learned in Jr.High
    std::uniform_real_distribution<double> radiusDist(0, radius);                       // some times they ask you if you're fine

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

        // Update screen bounds before moving (in case monitor changed during delay)
        updateScreenBounds();

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
    std::cout << "Enter the seconds between mouse movements: ";
    std::cin >> intervalSeconds;
    std::cout << "Enter the delay /s before starting: ";
    std::cin >> startDelay;

    std::thread mouseMonitor(monitorMouseMovement);
    std::thread mouseMover(moveMousePeriodically, intervalSeconds);

    std::cout << "\nPress Enter to stop the program...\n";
    std::cin.get();
    std::cin.get();

    running = false;

    if (mouseMonitor.joinable()) {
        mouseMonitor.join();
    }

    if (mouseMover.joinable()) {
        mouseMover.join();
    }

    std::cout << "Program stopped.\n";

    return 0;
}
