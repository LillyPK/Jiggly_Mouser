// mouse_move.hpp

#pragma once

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>
#include <mutex>

// -------------------------------------------------------
// Globals owned by mouse_move.hpp
// -------------------------------------------------------
inline int screenWidth  = 0;
inline int screenHeight = 0;
inline int centerX      = 0;
inline int centerY      = 0;
inline int radius       = 200;

inline std::atomic<bool> programMoving(false);  // Track if program is currently moving mouse
inline std::atomic<int>  delayCounter(0);       // Counter for delay period
inline std::mutex        posMutex;              // Mutex for position tracking

// Owned by JigglyMouser.cpp, referenced here
extern std::atomic<bool> running;
extern int startDelay;

// -------------------------------------------------------
// Platform-specific block
// -------------------------------------------------------

#ifdef _WIN32

#include <Windows.h>

inline std::atomic<DWORD> lastPenActivity(0);   // Track last pen/tablet activity
inline POINT lastKnownPos;                       // Last position set by program

inline HMONITOR getCurrentMonitor() {
    POINT pt;
    GetCursorPos(&pt);
    return MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
}

inline void updateScreenBounds() {
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
inline bool userActivityDetected() {
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

inline void monitorMouseMovement() {
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

inline void platformSetCursorPos(int x, int y) {
    std::lock_guard<std::mutex> lock(posMutex);
    SetCursorPos(x, y);
    lastKnownPos.x = x;
    lastKnownPos.y = y;
}

#elif defined(__linux__)

// -------------------------------------------------------
// Linux/X11 stubs - builds clean, does nothing
// Real X11 implementation goes here when linux support is added
// Will need: #include <X11/Xlib.h>
//            #include <X11/extensions/XInput2.h>
// -------------------------------------------------------

inline void updateScreenBounds() {
    // TODO: XRandR screen bounds detection goes here
}

inline bool userActivityDetected() {
    // TODO: XScreenSaverQueryInfo idle detection goes here
    return false;
}

inline void monitorMouseMovement() {
    // TODO: XRandR monitor change detection goes here
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

inline void platformSetCursorPos(int x, int y) {
    // TODO: XWarpPointer goes here
    (void)x;    // suppress unused warnings
    (void)y;
}

#endif  // platform block

// -------------------------------------------------------
// moveMousePeriodically - platform-agnostic logic
// calls platformSetCursorPos() for the one line that differs
// -------------------------------------------------------
inline void moveMousePeriodically(int intervalSeconds) {
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
        double angle = angleDist(gen);
        double distance = radiusDist(gen);
        int newX = static_cast<int>(centerX + distance * cos(angle));   // newX? first it was twitter, then X, and now its newX?
        int newY = static_cast<int>(centerY + distance * sin(angle));   // ooooooo more math that i forgot from highschool

        platformSetCursorPos(newX, newY);

        // Sleep in small chunks to remain responsive
        for (int i = 0; i < intervalSeconds && running && programMoving; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
