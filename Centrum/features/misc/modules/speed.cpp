#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

static bool gooonnznznz = false;
void hooks::speed() {
	while (true) {
		if (!globals::firstreceived)return;
				globals::misc::speedkeybind.update();
		if (globals::misc::speed && globals::misc::speedkeybind.enabled) {
			auto humanoid = globals::instances::lp.humanoid;
			auto hrp = globals::instances::lp.hrp;
			if (!is_valid_address(humanoid.address) || !is_valid_address(hrp.address))continue;
			switch (globals::misc::speedtype) {
						case 0:
				if (hrp.get_velocity().y > 55) {
					hrp.write_velocity(Vector3(0, 0, 0));
				}
				if (humanoid.read_walkspeed() != globals::misc::speedvalue) {
					humanoid.write_walkspeed(globals::misc::speedvalue);
				
				}
				else {

			
				}
				break;
			case 1:
							for (int i = 0; i < 10000; i++) {
					auto dir = humanoid.get_move_dir();
						hrp.write_velocity(Vector3(
							dir.x * globals::misc::speedvalue
							, hrp.get_velocity().y
							, dir.z * globals::misc::speedvalue
						));
				}		
				break;
			case 2:
								for (int i = 0; i < 10000; i++) {
					auto dir = humanoid.get_move_dir();
					auto pos = hrp.get_pos();
					auto finalpos = Vector3(pos.x + dir.x * globals::misc::speedvalue / 10000, pos.y, pos.z + dir.z * globals::misc::speedvalue / 10000);
					hrp.write_position(finalpos);
				}
				break;
			}

			
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

		const double targetFrameTime = 1.0 / 260;

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

	}
}