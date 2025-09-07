#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

void hooks::jumppower() {
	while(true){
	if (!globals::focused) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		continue;
	}
	globals::misc::jumppowerkeybind.update();
	if (globals::misc::jumppowerkeybind.enabled && globals::misc::jumppower) {
		auto humanoid = globals::instances::lp.humanoid;
		if (humanoid.read_jumppower() != globals::misc::jumpowervalue) {
			humanoid.write_jumppower(globals::misc::jumpowervalue);
		}
		else {
		}
	}
	else {
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

	const double targetFrameTime = 1.0 / 45;

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