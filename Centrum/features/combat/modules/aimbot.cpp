#include "../combat.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include "../../../util/driver/driver.h"
#include "../../hook.h"
#include <windows.h>
#include "../../wallcheck/wallcheck.h"

#define max
#undef max
#define min
#undef min

using namespace roblox;
static bool foundTarget = false;

struct MouseSettings {
    float baseDPI = 800.0f;
    float currentDPI = 800.0f;
    float dpiScaleFactor = 1.0f;
    bool dpiAutoDetected = false;

    void updateDPIScale() {
        dpiScaleFactor = baseDPI / currentDPI;
    }

    float getDPIAdjustedSensitivity() const {
        return dpiScaleFactor;
    }
} mouseSettings;

float CalculateDistance(Vector2 first, Vector2 sec) {
    return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2));
}

float CalculateDistance1(Vector3 first, Vector3 sec) {
    return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2) + pow(first.z - sec.z, 2));
}

Vector3 lerp_vector3(const Vector3& a, const Vector3& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

Vector3 AddVector3(const Vector3& first, const Vector3& sec) {
    return { first.x + sec.x, first.y + sec.y, first.z + sec.z };
}

Vector3 DivVector3(const Vector3& first, const Vector3& sec) {
    return { first.x / sec.x, first.y / sec.y, first.z / sec.z };
}

Vector3 normalize(const Vector3& vec) {
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    return (length != 0) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : vec;
}

Vector3 crossProduct(const Vector3& vec1, const Vector3& vec2) {
    return {
        vec1.y * vec2.z - vec1.z * vec2.y,
        vec1.z * vec2.x - vec1.x * vec2.z,
        vec1.x * vec2.y - vec1.y * vec2.x
    };
}

Matrix3 LookAtToMatrix(const Vector3& cameraPosition, const Vector3& targetPosition) {
    Vector3 forward = normalize(Vector3{ (targetPosition.x - cameraPosition.x), (targetPosition.y - cameraPosition.y), (targetPosition.z - cameraPosition.z) });
    Vector3 right = normalize(crossProduct({ 0, 1, 0 }, forward));
    Vector3 up = crossProduct(forward, right);

    Matrix3 lookAtMatrix{};
    lookAtMatrix.data[0] = -right.x;  lookAtMatrix.data[1] = up.x;  lookAtMatrix.data[2] = -forward.x;
    lookAtMatrix.data[3] = right.y;  lookAtMatrix.data[4] = up.y;  lookAtMatrix.data[5] = -forward.y;
    lookAtMatrix.data[6] = -right.z;  lookAtMatrix.data[7] = up.z;  lookAtMatrix.data[8] = -forward.z;

    return lookAtMatrix;
}

bool detectMouseDPI() {
    HWND robloxWindow = FindWindowA(0, "Roblox");
    if (!robloxWindow) return false;

    HDC hdc = GetDC(robloxWindow);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(robloxWindow, hdc);

    HKEY hkey;
    DWORD sensitivity = 10;
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Control Panel\\Mouse", 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        RegQueryValueEx(hkey, "MouseSensitivity", NULL, NULL, (LPBYTE)&sensitivity, &size);
        RegCloseKey(hkey);
    }

    float estimatedDPI = 800.0f * (sensitivity / 10.0f) * (dpiX / 96.0f);
    mouseSettings.currentDPI = std::max(400.0f, std::min(3200.0f, estimatedDPI));
    mouseSettings.updateDPIScale();
    mouseSettings.dpiAutoDetected = true;

    return true;
}

void performCameraAimbot(const Vector3& targetPos, const Vector3& cameraPos) {
    roblox::camera camera = globals::instances::camera;
    Matrix3 currentRotation = camera.getRot();
    Matrix3 targetMatrix = LookAtToMatrix(cameraPos, targetPos);

    if (globals::combat::smoothing) {
        float smoothFactorX = 1.0f / globals::combat::smoothingx;
        float smoothFactorY = 1.0f / globals::combat::smoothingy;

        smoothFactorX = std::max(0.01f, std::min(1.0f, smoothFactorX));
        smoothFactorY = std::max(0.01f, std::min(1.0f, smoothFactorY));

        Matrix3 smoothedRotation{};
        for (int i = 0; i < 9; ++i) {
            if (i % 3 == 0) {
                smoothedRotation.data[i] = currentRotation.data[i] + (targetMatrix.data[i] - currentRotation.data[i]) * smoothFactorX;
            }
            else {
                smoothedRotation.data[i] = currentRotation.data[i] + (targetMatrix.data[i] - currentRotation.data[i]) * smoothFactorY;
            }
        }
        camera.writeRot(smoothedRotation);
    }
    else {
        camera.writeRot(targetMatrix);
    }
}
void performMouseAimbot(const Vector2& screenCoords, HWND robloxWindow) {
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    ScreenToClient(robloxWindow, &cursorPos);

    float deltaX = screenCoords.x - cursorPos.x;
    float deltaY = screenCoords.y - cursorPos.y;

    if (globals::combat::smoothing) {
        float smoothFactorX = 1.0f / (globals::combat::smoothingx * 0.05f);
        float smoothFactorY = 1.0f / (globals::combat::smoothingy * 0.05f);

        smoothFactorX = std::max(0.01f, std::min(1.0f, smoothFactorX));
        smoothFactorY = std::max(0.01f, std::min(1.0f, smoothFactorY));

        float dpiAdjustedSensitivity = mouseSettings.getDPIAdjustedSensitivity();
        smoothFactorX *= dpiAdjustedSensitivity;
        smoothFactorY *= dpiAdjustedSensitivity;

        deltaX *= smoothFactorX;
        deltaY *= smoothFactorY;
    }

    if (std::isfinite(deltaX) && std::isfinite(deltaY) && (abs(deltaX) > 0.1f || abs(deltaY) > 0.1f)) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(deltaX);
        input.mi.dy = static_cast<LONG>(deltaY);
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(input));
    }
}

roblox::player gettargetclosesttomouse() {
    static HWND robloxWindow = nullptr;
    static auto lastWindowCheck = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    if (!robloxWindow || std::chrono::duration_cast<std::chrono::seconds>(now - lastWindowCheck).count() > 5) {
        robloxWindow = FindWindowA(nullptr, "Roblox");
        lastWindowCheck = now;
    }

    if (!robloxWindow) return {};

    POINT point;
    if (!GetCursorPos(&point) || !ScreenToClient(robloxWindow, &point)) {
        return {};
    }

    const Vector2 curpos = { static_cast<float>(point.x), static_cast<float>(point.y) };
    const auto& players = globals::instances::cachedplayers;

    if (players.empty()) return {};

    const bool useKnockCheck = globals::combat::knockcheck;
    const bool useHealthCheck = globals::combat::healthcheck;
    const bool useWallCheck = globals::combat::wallcheck;
    const bool useFov = globals::combat::usefov;
    const float fovSize = globals::combat::fovsize;
    const float fovSizeSquared = fovSize * fovSize;
    const float healthThreshold = globals::combat::healththreshhold;
    const bool isArsenal = (globals::instances::gamename == "Arsenal");
    const std::string& localPlayerName = globals::instances::localplayer.get_name();
    const Vector3 cameraPos = globals::instances::camera.getPos();

    roblox::player closest = {};
    float closestDistanceSquared = 9e18f;

    for (auto player : players) {
        if (!is_valid_address(player.head.address) ||
            player.name == localPlayerName ||
            player.head.address == 0) {
            continue;
        }

        const Vector2 screenCoords = roblox::worldtoscreen(player.head.get_pos());
        if (screenCoords.x == -1.0f || screenCoords.y == -1.0f) continue;

        const float dx = curpos.x - screenCoords.x;
        const float dy = curpos.y - screenCoords.y;
        const float distanceSquared = dx * dx + dy * dy;

        if (useFov && distanceSquared > fovSizeSquared) continue;

        if (isArsenal) {
            auto nrpbs = player.main.findfirstchild("NRPBS");
            if (nrpbs.address) {
                auto health = nrpbs.findfirstchild("Health");
                if (health.address && (health.read_double_value() == 0.0 || player.hrp.get_pos().y < 0.0f)) {
                    continue;
                }
            }
        }

        auto bodyEffects = player.instance.findfirstchild("BodyEffects");
        if (bodyEffects.address) {
            if (bodyEffects.findfirstchild("Dead").read_bool_value()) continue;
            if (useKnockCheck && bodyEffects.findfirstchild("K.O").read_bool_value()) continue;
        }



        if (useHealthCheck && player.health <= healthThreshold) continue;

        const bool useforcefieldcheck = (*globals::combat::flags)[6];
        bool hasaforcefield = false;
        if (useforcefieldcheck) {
            auto children = player.instance.get_children();
            for (roblox::instance& instance : children) {
                if (instance.get_name() == "ForceField") {
                    hasaforcefield = true;
                    break;
                }
            }
            if (hasaforcefield) {
                continue;
            }
        }
        const bool usegrabbedcheck = (*globals::combat::flags)[5];
        bool isgrabbed = false;
        if (usegrabbedcheck) {
            auto children = player.instance.get_children();
            for (roblox::instance& instance : children) {
                if (instance.get_name() == "GRABBING_CONSTRAINT") {
                    isgrabbed = true;
                    break;
                }
            }
            if (isgrabbed) {
                continue;
            }
        }

        if (useWallCheck && !wallcheck::can_see(player.head.get_pos(), cameraPos)) {
                   continue;
        }

        if (distanceSquared < closestDistanceSquared) {
            closestDistanceSquared = distanceSquared;
            closest = player;
        }
    }

    return closest;
}

void hooks::aimbot() {
    if (!mouseSettings.dpiAutoDetected) {
        detectMouseDPI();
    }

    HWND robloxWindow = FindWindowA(0, "Roblox");
    roblox::player target;

    while (true) {
        globals::combat::aimbotkeybind.update();

        if (!globals::focused || !globals::combat::aimbot || !globals::combat::aimbotkeybind.enabled) {
            foundTarget = false;
            target = {};
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (!foundTarget) {
            target = gettargetclosesttomouse();
            if (is_valid_address(target.main.address) && target.head.address != 0) {
                foundTarget = true;
            }
            else {
                continue;
            }
        }

        roblox::instance targetPart;
        targetPart = target.hrp;


        if (targetPart.address == 0) {
            foundTarget = false;
            continue;
        }

        Vector3 targetPos = targetPart.get_pos();

        if (globals::instances::gamename == "Arsenal") {
            if (target.main.findfirstchild("NRPBS").findfirstchild("Health").read_double_value() == 0 || target.hrp.get_pos().y < 0) {
                foundTarget = false;
                continue;
            }
        }

        if (target.bodyeffects.findfirstchild("K.O").read_bool_value() && globals::combat::knockcheck && globals::combat::autoswitch) {
            foundTarget = false;
            continue;
        }

        Vector3 predictedPos = targetPos;
        if (globals::combat::predictions) {
            Vector3 velocity = targetPart.get_velocity();
            Vector3 veloVector = DivVector3(velocity, { globals::combat::predictionsx, globals::combat::predictionsy, globals::combat::predictionsx });
            predictedPos = AddVector3(targetPos, veloVector);
        }

        Vector2 screenCoords = roblox::worldtoscreen(predictedPos);
        if (screenCoords.x == -1.0f || screenCoords.y == -1.0f) {
            foundTarget = false;
            continue;
        }

        if (globals::combat::aimbottype == 0) {
            roblox::camera camera = globals::instances::camera;
            Vector3 cameraPos = camera.getPos();
            performCameraAimbot(predictedPos, cameraPos);
            if (globals::combat::smoothing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        else if (globals::combat::aimbottype == 1) {
            performMouseAimbot(screenCoords, robloxWindow);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (target.head.address) {
            globals::instances::cachedtarget = target;
            globals::instances::cachedlasttarget = target;
        }

    }
}