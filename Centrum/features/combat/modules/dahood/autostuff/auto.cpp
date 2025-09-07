#include "auto.h"
#include "../../../../../util/globals.h"
#include "../../../../../drawing/overlay/overlay.h"
#include "../../../../hook.h"

Vector3 normalize3(const Vector3& vec) {
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    return (length > 0.001f) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : Vector3{ 0, 0, 0 };
}

Vector3 crossProduct3(const Vector3& vec1, const Vector3& vec2) {
    return {
        vec1.y * vec2.z - vec1.z * vec2.y,
        vec1.z * vec2.x - vec1.x * vec2.z,
        vec1.x * vec2.y - vec1.y * vec2.x
    };
}

Matrix3 LookAtToMatrix3(const Vector3& cameraPosition, const Vector3& targetPosition) {
    Vector3 forward = normalize3(Vector3{
        targetPosition.x - cameraPosition.x,
        targetPosition.y - cameraPosition.y,
        targetPosition.z - cameraPosition.z
        });

    if (forward.x == 0 && forward.y == 0 && forward.z == 0) {
        forward = Vector3{ 0, 0, -1 };
    }

    Vector3 right = normalize3(crossProduct3({ 0, 1, 0 }, forward));
    Vector3 up = crossProduct3(forward, right);

    Matrix3 lookAtMatrix{};
    lookAtMatrix.data[0] = -right.x;   lookAtMatrix.data[1] = up.x;   lookAtMatrix.data[2] = -forward.x;
    lookAtMatrix.data[3] = right.y;    lookAtMatrix.data[4] = up.y;   lookAtMatrix.data[5] = -forward.y;
    lookAtMatrix.data[6] = -right.z;   lookAtMatrix.data[7] = up.z;   lookAtMatrix.data[8] = -forward.z;

    return lookAtMatrix;
}

void waitForFocus() {
    while (!globals::focused) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void safeKeyPress(char key, int holdTime = 5, int cooldown = 5) {
    static std::chrono::steady_clock::time_point lastKeyPress = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyPress).count() < 200) {
        return;
    }

    BYTE vkCode = MapVirtualKey(key, 0);
    keybd_event(0, vkCode, KEYEVENTF_SCANCODE, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdTime));
    keybd_event(0, vkCode, KEYEVENTF_KEYUP, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(cooldown));

    lastKeyPress = now;
}

bool isTargetValid(roblox::player& target) {
    if (!is_valid_address(target.instance.address)) return false;
    if (!is_valid_address(target.head.address)) return false;

    try {
        if (target.bodyeffects.findfirstchild("Dead").read_bool_value()) return false;
        if (target.humanoid.read_health() <= 0) return false;
    }
    catch (...) {
        return false;
    }

    return true;
}

void hooks::listener() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (globals::bools::autokill) {
            waitForFocus();
            overlay::visible = false;
            autostuff::autokill(globals::bools::name);
            globals::bools::autokill = false;
            if (globals::instances::lp.humanoid.address) {
                globals::instances::lp.humanoid.unspectate();
            }
        }

        if (globals::bools::kill) {
            waitForFocus();
            overlay::visible = false;
            autostuff::kill(globals::bools::entity);
            globals::bools::kill = false;
            if (globals::instances::lp.humanoid.address) {
                globals::instances::lp.humanoid.unspectate();
            }
        }

        if (globals::bools::bring) {
            waitForFocus();
            overlay::visible = false;
            autostuff::bring(globals::bools::entity);
            globals::bools::bring = false;
            if (globals::instances::lp.humanoid.address) {
                globals::instances::lp.humanoid.unspectate();
            }
        }
    }
}

static bool isSpectating = false;

void orbit(roblox::player e) {
    if (!isTargetValid(e)) {
        if (isSpectating && globals::instances::lp.humanoid.address) {
            globals::instances::lp.humanoid.unspectate();
            isSpectating = false;
        }
        return;
    }

    static float angle = 0.0f;
    const float TWO_PI = 6.28318530718f;
    const float ORBIT_SPEED = 6;
    const float ORBIT_RADIUS = 8.0f;

    if (globals::instances::lp.hrp.address && globals::instances::lp.hrp.read_primgrav() != 0) {
        globals::instances::lp.hrp.writeprimgrav(0);
    }

    angle += 0.004f *  16;
    if (angle > TWO_PI) angle -= TWO_PI;

    if (!isSpectating && globals::instances::localplayer.address) {
        globals::instances::localplayer.spectate(e.head.address);
        isSpectating = true;
    }

    Vector3 orbitOffset = {
        ORBIT_RADIUS * std::sin(angle),
        1.0f,
        ORBIT_RADIUS * std::cos(angle)
    };

    Vector3 characterPos = e.uppertorso.get_pos();
    Vector3 finalPos = characterPos + orbitOffset;
    Matrix3 lookvec = LookAtToMatrix3(finalPos, characterPos);

    for (int i = 0; i < 50 && globals::focused; i++) {
        if (globals::instances::lp.hrp.address) {
            globals::instances::lp.hrp.write_position(finalPos);
            globals::instances::lp.hrp.write_velocity(Vector3(0, 0, 0));
            globals::instances::lp.hrp.write_part_cframe(lookvec);
        }
    }
}

void stomp(roblox::player target) {
    if (!isTargetValid(target)) return;
    if (!globals::instances::lp.hrp.address) return;

    auto localinstance = globals::instances::lp.hrp;
    auto savedPos = localinstance.get_pos();

    int maxAttempts = 20;
    int attempts = 0;

    while (attempts < maxAttempts && globals::focused && !overlay::visible) {
        attempts++;

        if (!isTargetValid(target)) break;
        if (!target.bodyeffects.findfirstchild("K.O").read_bool_value()) break;

        Vector3 stompPos = Vector3(
            target.lowertorso.get_pos().x,
            target.lowertorso.get_pos().y + 2.5f,
            target.lowertorso.get_pos().z
        );

        for (int i = 0; i < 5000 && globals::focused; i++) {
            localinstance.write_position(stompPos);
        }

        safeKeyPress('E', 30, 150);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (int i = 0; i < 5000; i++) {
        localinstance.write_position(savedPos);
    }
}

void bringzz(roblox::player target) {
    if (!isTargetValid(target)) return;
    if (!globals::instances::lp.hrp.address) return;

    auto localinstance = globals::instances::lp.hrp;
    auto savedPos = localinstance.get_pos();

    int maxAttempts = 20;
    int attempts = 0;

    while (attempts < maxAttempts && globals::focused && !overlay::visible) {
        attempts++;

        if (!isTargetValid(target)) break;
        if (!target.bodyeffects.findfirstchild("K.O").read_bool_value()) break;

        Vector3 bringPos = Vector3(
            target.lowertorso.get_pos().x,
            target.lowertorso.get_pos().y + 2.5f,
            target.lowertorso.get_pos().z
        );

        for (int i = 0; i < 5000 && globals::focused; i++) {
            localinstance.write_position(bringPos);
        }

        safeKeyPress('G', 30, 150);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (int i = 0; i < 5000; i++) {
        localinstance.write_position(savedPos);
    }
}

void rekt(roblox::player entity) {
    if (!isTargetValid(entity)) return;
    if (!globals::instances::lp.hrp.address) return;

    globals::focused = true;
    overlay::visible = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    HWND robloxWindow = FindWindowA(nullptr, "Roblox");
    if (!robloxWindow) return;

    roblox::player e = entity;
    roblox::instance bodyeffects = e.bodyeffects;

    while (globals::focused && !overlay::visible) {
        if (!isTargetValid(e)) break;

        try {
            if (bodyeffects.findfirstchild("K.O").read_bool_value()) break;
            if (bodyeffects.findfirstchild("Dead").read_bool_value()) break;
        }
        catch (...) {
            break;         }


        globals::instances::lp.hrp.writeprimgrav(0);

        orbit(e);

        try {
            if (bodyeffects.findfirstchild("K.O").read_bool_value()) break;
        }
        catch (...) {
            break;
        }

        std::string weaponName;
        try {
            weaponName = globals::instances::lp.instance.read_service("Tool").get_name();
        }
        catch (...) {
            weaponName = "";
        }

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));

        if (weaponName == "[Revolver]" || weaponName == "[Shotgun]" ||
            weaponName == "[Double-Barrel SG]" || weaponName == "[TacticalShotgun]") {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
        }

      }

    if (globals::instances::lp.hrp.address) {
     
    }
    globals::instances::lp.hrp.writeprimgrav(196.2f);
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void autostuff::kill(roblox::player entity) {
    if (!isTargetValid(entity)) return;
    if (!globals::instances::lp.hrp.address) return;

    Vector3 savedpos = globals::instances::lp.hrp.get_pos();

    rekt(entity);

    if (globals::focused && !overlay::visible && isTargetValid(entity)) {
        try {
            if (entity.bodyeffects.findfirstchild("K.O").read_bool_value()) {
                stomp(entity);
            }
        }
        catch (...) {
        }
    }

    for (int i = 0; i < 5000; i++) {
        globals::instances::lp.hrp.write_position(savedpos);
    }

    if (isSpectating && globals::instances::lp.humanoid.address) {
        globals::instances::lp.humanoid.unspectate();
        isSpectating = false;
    }
}

void autostuff::autokill(std::string name) {
    if (name.empty()) return;
    if (!globals::instances::lp.hrp.address) return;

    Vector3 savedpos = globals::instances::lp.hrp.get_pos();
    roblox::player target;
    bool found = false;

    while (!overlay::visible && globals::focused) {
        found = false;
        for (const roblox::player& player : globals::instances::cachedplayers) {
            if (player.name == name) {
                target = player;
                found = true;
                break;
            }
        }

        if (!found || !isTargetValid(target)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        bool hasForceField = true;
        while (hasForceField && !overlay::visible && globals::focused) {
            hasForceField = false;
            try {
                auto children = target.instance.get_children();
                for ( roblox::instance& instance : children) {
                    if (instance.get_name() == "ForceField") {
                        hasForceField = true;
                        break;
                    }
                }
            }
            catch (...) {
                break;
            }

            if (hasForceField) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (overlay::visible || !globals::focused) break;

        globals::bools::entity = target;
        rekt(target);

        if (overlay::visible || !globals::focused) break;

        if (isTargetValid(target)) {
            try {
                if (target.bodyeffects.findfirstchild("K.O").read_bool_value()) {
                    stomp(target);
                }
            }
            catch (...) {
            }
        }

        for (int i = 0; i < 5000; i++) {
            globals::instances::lp.hrp.write_position(savedpos);
        }

        if (isSpectating && globals::instances::lp.humanoid.address) {
            globals::instances::lp.humanoid.unspectate();
            isSpectating = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    for (int i = 0; i < 5000; i++) {
        globals::instances::lp.hrp.write_position(savedpos);
    }

    if (isSpectating && globals::instances::lp.humanoid.address) {
        globals::instances::lp.humanoid.unspectate();
        isSpectating = false;
    }
}

void autostuff::bring(roblox::player entity) {
    if (!isTargetValid(entity)) return;
    if (!globals::instances::lp.hrp.address) return;

    Vector3 savedpos = globals::instances::lp.hrp.get_pos();

    rekt(entity);

    if (globals::focused && !overlay::visible && isTargetValid(entity)) {
        try {
            if (entity.bodyeffects.findfirstchild("K.O").read_bool_value()) {
                bringzz(entity);
            }
        }
        catch (...) {
        }
    }

    for (int i = 0; i < 5000; i++) {
        globals::instances::lp.hrp.write_position(savedpos);
    }

    globals::bools::entity = {};

    if (isSpectating && globals::instances::lp.humanoid.address) {
        globals::instances::lp.humanoid.unspectate();
        isSpectating = false;
    }
}