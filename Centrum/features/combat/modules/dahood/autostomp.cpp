#include <thread>
#include <chrono>
#include <thread>
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <memory>
#include <thread>
#include <iostream>
#include "../../combat.h"
#include "../../../hook.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

static bool stomped = false;

void stompTarget(roblox::player target) {
    auto localinstance = globals::instances::lp.hrp;
    auto savedPos = localinstance.get_pos();
      while (!target.bodyeffects.findfirstchild("Dead").read_bool_value()) {
       if (target.bodyeffects.findfirstchild("Dead").read_bool_value() || !target.bodyeffects.findfirstchild("K.O").read_bool_value())break;
       if (target.humanoid.read_health() == 0) break;
       if (!target.head.address) break;
       if (!globals::misc::autostomp) break;
       globals::misc::stompkeybind.update();
       if (!globals::misc::stompkeybind.enabled) break;
       for (int i = 0; i < 2500; i++) {
           auto tele = Vector3(target.lowertorso.get_pos().x, target.lowertorso.get_pos().y + 2.7f, target.lowertorso.get_pos().z);
           localinstance.write_position(tele);
       }
            keybd_event(0, MapVirtualKey('E', 0), KEYEVENTF_SCANCODE, 0);
            keybd_event(0, MapVirtualKey('E', 0), KEYEVENTF_KEYUP, 0);
    }
    std::cout << "LOOP BROKE" << std::endl;
    for (int i = 0; i < 2500; i++) {
        localinstance.write_position(savedPos);
    }
       
    stomped = false;
    target = {};
}


void hooks::autostomp() {
    static bool isEPressed = false;
    while (true) {

        if (!globals::focused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        globals::misc::stompkeybind.update();
        if (globals::misc::autostomp && globals::misc::stompkeybind.enabled && globals::focused) {
            auto target = globals::instances::cachedlasttarget;
            auto koValue = target.bodyeffects.findfirstchild("K.O").read_bool_value();
            if (target.uppertorso.address && target.hrp.address && target.head.address && target.lowertorso.address) {
                if (koValue == true) {
                    bool currentEState = globals::misc::stompkeybind.enabled;
                    if (currentEState && !isEPressed) {
                        stompTarget(target);
                    }
                    isEPressed = currentEState;
                }
            }
            else {
                continue;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}



