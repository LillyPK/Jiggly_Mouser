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

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

// -------------------------------------------------------
// Linux/X11 globals
// -------------------------------------------------------
inline Display* xDisplay   = nullptr;   // Connection to the X server
inline Window   xRootWin   = 0;         // Root window of the default screen
inline int      xLastMouseX = 0;        // Last known mouse X for cursor pos tracking
inline int      xLastMouseY = 0;        // Last known mouse Y for cursor pos tracking

// Opens the X display connection if not already open
// Returns true if display is available
inline bool ensureDisplay() {
    if (xDisplay == nullptr) {
        xDisplay = XOpenDisplay(nullptr);
        if (xDisplay == nullptr) {
            std::cerr << "Failed to open X display.\n";
            return false;
        }
        xRootWin = XRootWindow(xDisplay, DefaultScreen(xDisplay));
    }
    return true;
}

// Gets the current cursor position via XQueryPointer
// Returns true on success, fills outX and outY
inline bool getCursorPos(int& outX, int& outY) {
    if (!ensureDisplay()) return false;

    Window rootReturn, childReturn;
    int rootX, rootY, winX, winY;
    unsigned int maskReturn;

    Bool result = XQueryPointer(
        xDisplay, xRootWin,
        &rootReturn, &childReturn,
        &rootX, &rootY,
        &winX, &winY,
        &maskReturn
    );

    if (result) {
        outX = rootX;
        outY = rootY;
    }

    return result == True;
}

// Walks all XRandR CRTCs to find which monitor the cursor is currently on
// Sets screenWidth, screenHeight, centerX, centerY from that monitor's geometry
inline void updateScreenBounds() {
    if (!ensureDisplay()) return;

    int curX = 0, curY = 0;
    if (!getCursorPos(curX, curY)) return;

    XRRScreenResources* res = XRRGetScreenResources(xDisplay, xRootWin);
    if (!res) {
        std::cerr << "Failed to get XRandR screen resources.\n";
        return;
    }

    for (int i = 0; i < res->ncrtc; i++) {
        XRRCrtcInfo* crtc = XRRGetCrtcInfo(xDisplay, res, res->crtcs[i]);
        if (!crtc) continue;

        // Skip CRTCs with no outputs (disabled/inactive)
        if (crtc->noutput == 0) {
            XRRFreeCrtcInfo(crtc);
            continue;
        }

        int monLeft   = crtc->x;
        int monTop    = crtc->y;
        int monRight  = crtc->x + (int)crtc->width;
        int monBottom = crtc->y + (int)crtc->height;

        // Check if cursor falls within this monitor's rectangle
        if (curX >= monLeft && curX < monRight &&
            curY >= monTop  && curY < monBottom) {
            screenWidth  = (int)crtc->width;
            screenHeight = (int)crtc->height;
            centerX      = monLeft + screenWidth  / 2;
            centerY      = monTop  + screenHeight / 2;

            XRRFreeCrtcInfo(crtc);
            break;
        }

        XRRFreeCrtcInfo(crtc);
    }

    XRRFreeScreenResources(res);
}

// Returns true if the user has interacted with mouse or keyboard very recently
// Uses XQueryPointer to detect mouse movement
inline bool userActivityDetected() {
    if (!ensureDisplay()) return false;

    int curX = 0, curY = 0;
    if (!getCursorPos(curX, curY)) return false;

    // If mouse position changed since last check, count it as activity
    if (curX != xLastMouseX || curY != xLastMouseY) {
        xLastMouseX = curX;
        xLastMouseY = curY;
        return true;
    }

    return false;
}

inline void monitorMouseMovement() {
    if (!ensureDisplay()) return;

    // Grab initial cursor position and screen bounds
    getCursorPos(xLastMouseX, xLastMouseY);
    updateScreenBounds();

    // Track which monitor we started on by storing last known centerX/centerY
    int lastCenterX = centerX;
    int lastCenterY = centerY;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Check every 100ms

        bool activity = userActivityDetected();

        // Re-check bounds in case the cursor moved to a different monitor
        updateScreenBounds();
        if (centerX != lastCenterX || centerY != lastCenterY) {
            std::cout << "\nMonitor changed. Updating screen bounds...\n";
            lastCenterX = centerX;
            lastCenterY = centerY;
        }

        if (activity) {
            if (programMoving) {
                std::cout << "\nUser activity detected. Stopping program and resetting delay...\n";
                programMoving = false;
            }
            delayCounter = 0;
        }
    }

    // Clean up X display connection on exit
    if (xDisplay) {
        XCloseDisplay(xDisplay);
        xDisplay = nullptr;
    }
}

inline void platformSetCursorPos(int x, int y) {
    if (!ensureDisplay()) return;
    std::lock_guard<std::mutex> lock(posMutex);
    XWarpPointer(xDisplay, None, xRootWin, 0, 0, 0, 0, x, y);
    XFlush(xDisplay);   // XWarpPointer is buffered, flush pushes it immediately
    xLastMouseX = x;
    xLastMouseY = y;
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
        double angle = angleDist(gen);      // Double persision float angle variable that equals angleDist
        double distance = radiusDist(gen);  // Double distance is what they call people not allowed near school zones
        int newX = static_cast<int>(centerX + distance * cos(angle));   // newX? first it was twitter, then X, and now its newX?
        int newY = static_cast<int>(centerY + distance * sin(angle));   // ooooooo more math that i forgot from highschool

        platformSetCursorPos(newX, newY);

        // Sleep in small chunks to remain responsive
        for (int i = 0; i < intervalSeconds && running && programMoving; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
