#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../../drawing/overlay/overlay.h"

void reload() {
	keybd_event(0, MapVirtualKey('R', 0), KEYEVENTF_SCANCODE, 0);
	keybd_event(0, MapVirtualKey('R', 0), KEYEVENTF_KEYUP, 0);
}

void hooks::autoreload() {
	while (true) {
		if ((globals::misc::flightkeybind.enabled && globals::misc::flight)
			|| ((globals::combat::orbitkeybind.enabled && globals::combat::orbit)
				&& ((globals::combat::silentaimkeybind.enabled && globals::combat::silentaim)
					|| (globals::combat::aimbotkeybind.enabled && globals::combat::aimbot)))) {
			if (globals::instances::lp.hrp.read_primgrav() != 0) {
				globals::instances::lp.hrp.writeprimgrav(0);
			}
			else {
				Sleep(16);
			}
		}
		else {
			globals::instances::lp.hrp.writeprimgrav(196.2);
		}

		if (globals::misc::autoreload && globals::focused && !overlay::visible) {
			auto instance = globals::instances::lp.instance;
			auto tool = instance.read_service("Tool");

			if (tool.address && instance.address) {
				auto ammo = tool.findfirstchild("Ammo");
				auto maxammo = tool.findfirstchild("MaxAmmo");
				auto reloading = globals::instances::lp.bodyeffects.findfirstchild("Reload").read_bool_value();
				if (ammo.read_int_value() == 0 && ammo.address && !reloading) {
					reload();
				}
				else {
					Sleep(16);
				}
			}
			else {

			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}