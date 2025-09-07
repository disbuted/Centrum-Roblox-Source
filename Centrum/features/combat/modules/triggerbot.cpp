#include "../combat.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../hook.h"

using namespace roblox;

float CalculateDistanceA(Vector2 first, Vector2 sec) {
    return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2));
}
float CalculateDistanceB(Vector3 first, Vector3 sec) {
    return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2) + pow(first.z - sec.z, 2));
}
constexpr auto SLEEP_DURATION = std::chrono::milliseconds(10);




roblox::player TriggerBotClosest()
{
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
    std::vector<roblox::player>& players = globals::instances::cachedplayers;
    roblox::player closest = {};
    float closestdistance = 9e9;
    POINT point;
    GetCursorPos(&point);
    ScreenToClient(FindWindowA(0, "Roblox"), &point);
    Vector2 curpos = { point.x, point.y };

    for (auto player : players) {
        if (!is_valid_address(player.head.address))continue;
        roblox::instance head = player.head;
        if (head.address == 0) continue;
        if (player.name == globals::instances::lp.name)continue;
        Vector2 screencoords = roblox::worldtoscreen(head.get_pos());
        if (screencoords.x == -1.0f || screencoords.y == -1.0f) continue;
        float distance = CalculateDistanceA(curpos, screencoords);
        if (distance < closestdistance) {
            closestdistance = distance;
            closest = player;
        }


    }
    return closest;
}


void hooks::triggerbot() {
    auto lastFireTime = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_for(SLEEP_DURATION);
     
        HWND hwnd = FindWindowA(0, "Roblox");
        if (hwnd == nullptr || !IsWindow(hwnd)) continue;

        globals::combat::triggerbotkeybind.update();

        if (!globals::focused || !globals::combat::triggerbotkeybind.enabled || !globals::combat::triggerbot) {
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            continue;
        }

        auto player = TriggerBotClosest();
        std::cout << player.name << std::endl;
        if (!player.head.address || !player.head.address || !player.uppertorso.address || !player.leftfoot.address) continue;
    
        RECT windowRect;
        if (!GetWindowRect(hwnd, &windowRect)) continue;

        POINT point;
        if (!GetCursorPos(&point)) continue;

        if (point.x < windowRect.left || point.x > windowRect.right ||
            point.y < windowRect.top || point.y > windowRect.bottom) continue;

        if (!ScreenToClient(hwnd, &point)) continue;

        Vector2 curpos = { static_cast<float>(point.x), static_cast<float>(point.y) };
        bool foundTarget = false;
    
        std::unordered_map<std::string, Vector2> screen_positions;
        auto getPos = [&](auto& part, const std::string& name) {
            if (!part.address) return;
            Vector3 pos = part.get_pos();
            Vector2 screen_pos = roblox::worldtoscreen(pos);
            if (screen_pos.x >= 0.0f && screen_pos.y >= 0.0f &&
                screen_pos.x <= (windowRect.right - windowRect.left) &&
                screen_pos.y <= (windowRect.bottom - windowRect.top)) {
                screen_positions[name] = screen_pos;
            }
            };

        getPos(player.head, "Head");
        getPos(player.hrp, "HumanoidRootPart");
        getPos(player.leftupperarm, "LeftUpperArm");
        getPos(player.rightupperarm, "RightUpperArm");
        getPos(player.leftfoot, "LeftFoot");
        getPos(player.rightfoot, "RightFoot");
        getPos(player.leftlowerarm, "LeftLowerArm");
        getPos(player.rightlowerarm, "RightLowerArm");
        getPos(player.lefthand, "LeftHand");
        getPos(player.righthand, "RightHand");

        for (const auto& part : screen_positions) {
               
            float distance = CalculateDistanceA(curpos, part.second);
            float range = CalculateDistanceB(globals::instances::lp.hrp.get_pos(), player.hrp.get_pos());
            if (distance <= 15 && !player.instance.findfirstchild("BodyEffects").findfirstchild("K.O").read_bool_value()) {
                if (globals::combat::triggerbot && range <= globals::combat::triggerbotrangevalue) {
                    foundTarget = true;
                    break;
                }
                else if (!globals::combat::triggerbotrange) {
                    foundTarget = true;
                    break;
                }
            }
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceLastFire = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFireTime).count();

        if (foundTarget && timeSinceLastFire >= globals::combat::delay) {
 
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = 0;
            input.mi.dy = 0;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            input.mi.mouseData = 0;
            input.mi.dwExtraInfo = 0;
            input.mi.time = 0;

            SendInput(1, &input, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));

            lastFireTime = currentTime;
            std::cout << "past check" << std::endl;
        }



    }
}