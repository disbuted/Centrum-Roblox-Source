#include "../combat.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <thread>
#include <algorithm>
#include <random>
#include "../../hook.h"
#include <iostream>
#include <windows.h>

#pragma comment(lib, "winmm.lib")

Vector3 normalize22(const Vector3& vec) {
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    return (length != 0) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : vec;
}

Vector3 crossProduct22(const Vector3& vec1, const Vector3& vec2) {
    return {
        vec1.y * vec2.z - vec1.z * vec2.y,
        vec1.z * vec2.x - vec1.x * vec2.z,
        vec1.x * vec2.y - vec1.y * vec2.x
    };
}

Matrix3 LookAtToMatrix22(const Vector3& cameraPosition, const Vector3& targetPosition) {
    Vector3 forward = normalize22(Vector3{ (targetPosition.x - cameraPosition.x), (targetPosition.y - cameraPosition.y), (targetPosition.z - cameraPosition.z) });
    Vector3 right = normalize22(crossProduct22({ 0, 1, 0 }, forward));
    Vector3 up = crossProduct22(forward, right);

    Matrix3 lookAtMatrix{};
    lookAtMatrix.data[0] = -right.x;  lookAtMatrix.data[1] = up.x;  lookAtMatrix.data[2] = -forward.x;
    lookAtMatrix.data[3] = right.y;  lookAtMatrix.data[4] = up.y;  lookAtMatrix.data[5] = -forward.y;
    lookAtMatrix.data[6] = -right.z;  lookAtMatrix.data[7] = up.z;  lookAtMatrix.data[8] = -forward.z;

    return lookAtMatrix;
}

void hooks::anti_aim() {
    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(currentThread, 1);

    static LARGE_INTEGER frequency;
    static LARGE_INTEGER lastTime;
    static bool timeInitialized = false;

    if (!timeInitialized) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);
        timeBeginPeriod(1);
        timeInitialized = true;
    }

    const double targetFrameTime = 1.0 / 425.0;
    bool toggle = false;

    while (true) {
        roblox::instance playerservice = roblox::instance().read_service("Players");
        roblox::instance localplayer = playerservice.local_player();

        if (localplayer.is_valid()) {
            roblox::instance character = localplayer.findfirstchild("Character");  // change the logic for this if u wanna
            if (character.is_valid()) {
                roblox::instance hrp = character.findfirstchild("HumanoidRootPart");
                if (hrp.is_valid()) {
                    Vector3 currentpos = hrp.get_pos();

                    if (globals::combat::antiaim) {
                        float jitter = toggle ? 2.2f : -2.2f;
                        toggle = !toggle;

                        Vector3 newpos = {
                            currentpos.x + jitter,
                            currentpos.y + static_cast<float>((rand() % 3) - 1),
                            currentpos.z + jitter
                        };

                        hrp.write_position(newpos);
                    }
                    else if (globals::combat::underground_antiaim) {
                        float jitter = toggle ? -5.0f : 5.0f;
                        toggle = !toggle;

                        Vector3 newpos = {
                            currentpos.x + jitter,
                            currentpos.y - 50.0f + static_cast<float>((rand() % 3) - 1),  // underground
                            currentpos.z + jitter
                        };

                        hrp.write_position(newpos);
                    }
                }
            }
        }
    }

        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;

        if (elapsedSeconds < targetFrameTime) {
            DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
            if (sleepMilliseconds > 0) {
                Sleep(sleepMilliseconds);
            }
        }

        do {
            QueryPerformanceCounter(&currentTime);
            elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        } while (elapsedSeconds < targetFrameTime);

        lastTime = currentTime;
        SwitchToThread();
    }
