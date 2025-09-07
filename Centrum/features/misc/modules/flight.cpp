#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

static bool aircheck;

static Vector3 lookvec(const Matrix3& rotationMatrix) {
	return rotationMatrix.getColumn(2);
}

static Vector3 rightvec(const Matrix3& rotationMatrix) {
	return rotationMatrix.getColumn(0);
}


void hooks::flight() {
	using clock = std::chrono::high_resolution_clock;

	while (true) {
		auto start_time = clock::now();
		if (!globals::focused) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			continue;
		}
		if (globals::misc::flight) {
			globals::misc::flightkeybind.update();


			if (globals::misc::flightkeybind.enabled) {
				auto humanoid = globals::instances::lp.humanoid;
				auto hrp = globals::instances::lp.hrp;
				roblox::camera camera = globals::instances::camera;
				Matrix3 rotation_matrix = camera.getRot();
				Vector3 look_vector = lookvec(rotation_matrix);
				Vector3 right_vector = rightvec(rotation_matrix);
				Vector3 up_vector = { 0.0f, 1.0f, 0.0f };
				Vector3 direction = { 0.0f, 0.0f, 0.0f };

				switch (globals::misc::flighttype) {
				case 0:
					if (GetAsyncKeyState('W') & 0x8000) {
						direction = direction - look_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('S') & 0x8000) {
						direction = direction + look_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('A') & 0x8000) {
						direction = direction - right_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('D') & 0x8000) {
						direction = direction + right_vector;
						aircheck = true;
					}

					if (direction.magnitude() > 0) {
						direction = direction.normalize();
					}

					if (aircheck == false) {
						hrp.write_velocity({ 0.0f, 0.0f, 0.0f });
					}
					if (aircheck == true) {
						for (int i = 0; i < 4500; i++) {
							hrp.write_position(hrp.get_pos() + (direction * (globals::misc::flightvalue / 25000)));
							hrp.write_velocity({ 0.0f, 0.0f, 0.0f });

						}
					}
					break;
				case 1: {
					if (GetAsyncKeyState('W') & 0x8000) {
						direction = direction - look_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('S') & 0x8000) {
						direction = direction + look_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('A') & 0x8000) {
						direction = direction - right_vector;
						aircheck = true;
					}
					if (GetAsyncKeyState('D') & 0x8000) {
						direction = direction + right_vector;
						aircheck = true;
					}

					if (direction.magnitude() > 0) {
						direction = direction.normalize();
					}

					if (aircheck == false) {
						hrp.write_velocity({ 0.0f, 0.0f, 0.0f });
					}
					if (aircheck == true) {
						for (int i = 0; i < 1500; i++) {
							hrp.write_velocity(direction * (globals::misc::flightvalue ));



						}
					}
					break;
				}
				case 2: {

					break;
				}

				}
			
			
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		std::this_thread::sleep_for(std::chrono::microseconds(5));
			static LARGE_INTEGER frequency;
			static LARGE_INTEGER lastTime;
			static bool timeInitialized = false;

			if (!timeInitialized) {
				QueryPerformanceFrequency(&frequency);
				QueryPerformanceCounter(&lastTime);
				timeBeginPeriod(1);
				timeInitialized = true;
			}

			const double targetFrameTime = 1.0 / 165;

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