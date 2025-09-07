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

Vector3 normalize2(const Vector3& vec) {
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    return (length != 0) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : vec;
}

Vector3 crossProduct2(const Vector3& vec1, const Vector3& vec2) {
    return {
        vec1.y * vec2.z - vec1.z * vec2.y,
        vec1.z * vec2.x - vec1.x * vec2.z,
        vec1.x * vec2.y - vec1.y * vec2.x
    };
}

Matrix3 LookAtToMatrix2(const Vector3& cameraPosition, const Vector3& targetPosition) {
    Vector3 forward = normalize2(Vector3{ (targetPosition.x - cameraPosition.x), (targetPosition.y - cameraPosition.y), (targetPosition.z - cameraPosition.z) });
    Vector3 right = normalize2(crossProduct2({ 0, 1, 0 }, forward));
    Vector3 up = crossProduct2(forward, right);

    Matrix3 lookAtMatrix{};
    lookAtMatrix.data[0] = -right.x;  lookAtMatrix.data[1] = up.x;  lookAtMatrix.data[2] = -forward.x;
    lookAtMatrix.data[3] = right.y;  lookAtMatrix.data[4] = up.y;  lookAtMatrix.data[5] = -forward.y;
    lookAtMatrix.data[6] = -right.z;  lookAtMatrix.data[7] = up.z;  lookAtMatrix.data[8] = -forward.z;

    return lookAtMatrix;
}

void hooks::orbit() {
    static bool isSpectating = false;
    static bool zellis = false;
    Vector3 lastpos;
    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(currentThread, 1);

    while (true) {
        globals::combat::orbitkeybind.update();
        if (globals::focused && globals::combat::orbit && globals::combat::orbitkeybind.enabled
            && ((globals::combat::aimbotkeybind.enabled || globals::combat::aimbot)
                || (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled)))
        {
            if (!zellis) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                lastpos = globals::instances::lp.hrp.get_pos();
                zellis = true;
            }
            auto character = globals::instances::cachedtarget;
            if (is_valid_address(character.instance.address)) {
                static float angle = 0.0f;
                static float previousAngle = 0.0f;
                const float TWO_PI = 6.28318530718f;

                previousAngle = angle;
                angle += 0.004f * globals::combat::orbitspeed;
                if (angle > TWO_PI) angle -= TWO_PI;

                globals::instances::localplayer.spectate(character.head.address);
                isSpectating = true;

                static bool orbitedy = false;
                static Vector3 orbitOffset = Vector3(1, 2, 1);

                const bool xaxis = (globals::combat::orbittype)[0] != 0;
                const bool yaxis = (globals::combat::orbittype)[1] != 0;
                const bool randaxis = (globals::combat::orbittype)[2] != 0;

                if (!xaxis && !yaxis && !randaxis) {
                    orbitOffset = { 1, 3, 1 };
                }
                else if (xaxis && !yaxis) {
                    orbitOffset = {
                        globals::combat::orbitrange * std::sin(angle),
                        globals::combat::orbitheight,
                        globals::combat::orbitrange * std::cos(angle)
                    };
                }
                else if (!xaxis && yaxis) {
                    orbitOffset = {
                        0,
                        globals::combat::orbitheight * std::cos(angle),
                        globals::combat::orbitheight * std::sin(angle)
                    };
                }
                else if (randaxis) {
                    static std::default_random_engine generator(static_cast<unsigned int>(timeGetTime()));
                    static std::uniform_real_distribution<float> distX(-globals::combat::orbitrange, globals::combat::orbitrange);
                    static std::uniform_real_distribution<float> distY(0.5f, globals::combat::orbitheight * 2.0f);
                    static std::uniform_real_distribution<float> distZ(-globals::combat::orbitrange, globals::combat::orbitrange);

                    orbitOffset = {
                        distX(generator),
                        distY(generator),
                        distZ(generator)
                    };
                }
                else {
                    if (orbitedy) {
                        orbitOffset = {
                            globals::combat::orbitrange * std::cos(angle),
                            globals::combat::orbitheight * std::cos(angle),
                            globals::combat::orbitheight * std::sin(angle)
                        };
                    }
                    else {
                        orbitOffset = {
                            globals::combat::orbitrange * std::sin(angle),
                            globals::combat::orbitheight,
                            globals::combat::orbitrange * std::cos(angle)
                        };
                    }
                }

                Vector3 characterPos = character.uppertorso.get_pos();
                Vector3 finalPos = characterPos + orbitOffset;
                Matrix3 lookvec = LookAtToMatrix2(finalPos, characterPos);
                for (int i = 0; i < 35; i++) {
                    globals::instances::lp.hrp.write_position(finalPos);
                    globals::instances::lp.hrp.write_velocity(Vector3(0, 0, 0));
                    globals::instances::lp.hrp.write_part_cframe(lookvec);
                }

                if (previousAngle > angle) {
                    orbitedy = !orbitedy;
                }
            }
        }
        else {
            if (zellis) {
                zellis = false;
            }
            if (isSpectating) {
                globals::instances::lp.humanoid.unspectate();
                isSpectating = false;
            }
        }

        static LARGE_INTEGER frequency;
        static LARGE_INTEGER lastTime;
        static bool timeInitialized = false;

        if (!timeInitialized) {
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&lastTime);
            timeBeginPeriod(1);
            timeInitialized = true;
        }

        const double targetFrameTime = 1.0 / 425;

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
}
