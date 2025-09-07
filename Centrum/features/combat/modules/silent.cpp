#include "../combat.h"
#include <thread>
#include <chrono>
#include <Windows.h>
#include "../../../util/console/console.h"
#include <iostream>
#include <random>
#include "../../hook.h"
#include "../../wallcheck/wallcheck.h"

#define max
#undef max
#define min
#undef min

using namespace roblox;

bool foundTarget = false;
static Vector2 partpos = {};
static uint64_t cachedPositionX = 0;
static uint64_t cachedPositionY = 0;
static bool dataReady = false;
static bool targetNeedsReset = false;

using namespace roblox;
inline float fastdist(const Vector2& a, const Vector2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool isAutoFunctionActive() {
    return globals::bools::bring || globals::bools::kill || globals::bools::autokill;
}

bool shouldSilentAimBeActive() {
    return (globals::focused && globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) || isAutoFunctionActive();
}

roblox::player gettargetclosesttomousesilent() {
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
    const bool useFov = globals::combat::usesfov;
    const float fovSize = globals::combat::sfovsize;
    const float fovSizeSquared = fovSize * fovSize;
    const float healthThreshold = globals::combat::healththreshhold;
    const bool isArsenal = (globals::instances::gamename == "Arsenal");
    const std::string& localPlayerName = globals::instances::localplayer.get_name();
    const Vector3 cameraPos = globals::instances::camera.getPos();

    roblox::player closest = {};
    float closestDistanceSquared = 9e18f;

    for (auto player : players) {
        if (!is_valid_address(player.main.address) ||
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


        if (useHealthCheck && player.health <= healthThreshold) continue;

        if (useWallCheck && !wallcheck::can_see(player.head.get_pos(), cameraPos)) continue;

        if (distanceSquared < closestDistanceSquared) {
            closestDistanceSquared = distanceSquared;
            closest = player;
        }
    }

    return closest;
}

void hooks::silentrun() {
    roblox::player target;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HWND robloxWindow = FindWindowA(0, "Roblox");
    const std::chrono::milliseconds sleepTime(10);

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        globals::combat::silentaimkeybind.update();

        if (!shouldSilentAimBeActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            dataReady = false;
            globals::instances::cachedtarget = {};
            target = {};
            foundTarget = false;
            targetNeedsReset = false;
            continue;
        }

        if (globals::instances::gamename == "Da Hood" || globals::instances::gamename == "Universal") {
            globals::instances::aim = globals::instances::localplayer.findfirstchild("PlayerGui").findfirstchild("MainScreenGui").findfirstchild("Aim");
        }
        else if (globals::instances::gamename == "Hood Customs") {
            globals::instances::aim = globals::instances::localplayer.findfirstchild("PlayerGui").findfirstchild("Main Screen").findfirstchild("Aim");
        }

        if (isAutoFunctionActive()) {
            target = globals::bools::entity;
            globals::instances::cachedtarget = target;
            foundTarget = (target.head.address != 0);
        }
        else if (!globals::combat::stickyaimsilent || !foundTarget) {
            target = gettargetclosesttomousesilent();
            globals::instances::cachedlasttarget = target;
            foundTarget = (target.head.address != 0);
            globals::instances::cachedtarget = target;
        }
        if (target.bodyeffects.findfirstchild("K.O").read_bool_value() && globals::combat::knockcheck && globals::combat::autoswitch) {
            foundTarget = false;
            continue;
        }
        if (foundTarget && globals::instances::cachedtarget.head.address != 0) {
            if (isAutoFunctionActive()) {
                        }
            roblox::instance part = globals::instances::cachedtarget.head;
            Vector3 part3d = part.get_pos();
            partpos = roblox::worldtoscreen(part3d);
            globals::instances::cachedlasttarget = target;

            Vector2 dimensions = globals::instances::visualengine.get_dimensions();
            POINT cursorPoint;
            GetCursorPos(&cursorPoint);
            ScreenToClient(robloxWindow, &cursorPoint);

            cachedPositionX = static_cast<uint64_t>(cursorPoint.x);
            cachedPositionY = static_cast<uint64_t>(dimensions.y - std::abs(dimensions.y - (cursorPoint.y)) - 58);
            dataReady = true;
        }
        else {
            dataReady = false;
                   }
    }
}

void hooks::silentrun2() {
    roblox::instance mouseServiceInstance;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> distrib(-100.0, 100.0);

    while (true) {
        if (!shouldSilentAimBeActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (globals::instances::cachedtarget.head.address != 0) targetNeedsReset = true;
            continue;
        }
        std::cout << "ARKUYH" << std::endl;


        if (globals::instances::cachedtarget.head.address != 0 && dataReady) {
            if (globals::combat::spoofmouse) {
                globals::instances::aim.setFramePosX(cachedPositionX);
                globals::instances::aim.setFramePosY(cachedPositionY);
                mouseServiceInstance.initialize_mouse_service(globals::instances::mouseservice);
                mouseServiceInstance.write_mouse_position(globals::instances::mouseservice, partpos.x, partpos.y);
            }
            else {
                mouseServiceInstance.initialize_mouse_service(globals::instances::mouseservice);
                mouseServiceInstance.write_mouse_position(globals::instances::mouseservice, partpos.x, partpos.y);
            }
        }

          }
}

void hooks::silent() {
    std::thread primaryThread(&hooks::silentrun);
    std::thread secondaryThread(&hooks::silentrun2);
    primaryThread.detach();
    secondaryThread.detach();
}