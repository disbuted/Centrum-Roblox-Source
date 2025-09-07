#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
static Vector3 lastpos;
static bool wason;

void hooks::voidhide() {
	while (true) {
		if (!globals::focused) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			continue;
		}
		auto hrp = globals::instances::lp.hrp;
		globals::misc::voidhidebind.update();
	
		if (globals::misc::voidhidebind.enabled && globals::misc::voidhide) {
			for (int i = 0; i < 5000; i++) {
				if (!globals::misc::voidhidebind.enabled || !globals::misc::voidhide)break;
				hrp.write_position(Vector3(hrp.get_pos().x + 9999999999999999999, hrp.get_pos().y + 9999999999999999999, hrp.get_pos().z + 9999999999999999999));
			}

			Sleep(16);
			wason = true;
		}

		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			if (wason) {
				for (int i = 0; i < 10000; i++) {
					hrp.write_position(lastpos);
					hrp.write_velocity(Vector3(0, 0, 0));
				}
				wason = false;
			}
			lastpos = hrp.get_pos();
		}


	}
}