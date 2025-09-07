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


void hooks::antistomp() {
	while (true) {
		if (!globals::focused) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			continue;
		}
		if (globals::misc::antistomp) {
			auto instance = globals::instances::lp.instance;
			auto bodyeffects = globals::instances::lp.bodyeffects;
			if (!is_valid_address(instance.address))continue;
			if (!is_valid_address(bodyeffects.address))continue;
			if (bodyeffects.findfirstchild("K.O"). read_bool_value() || instance.findfirstchild("Humanoid").read_health() == 0) {
				bodyeffects.findfirstchild("Dead").write_bool_value(true);
				bodyeffects.findfirstchild("SDeath").write_bool_value(false);
				bodyeffects.findfirstchild("K.O").write_bool_value(true);
				instance.findfirstchild("Humanoid").write_health(0);
			}
		}
		Sleep(20);
	}
}