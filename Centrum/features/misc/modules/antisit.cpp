#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

void hooks::antisit() {
	while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if(globals::misc::bikefly){
        auto msdfjsdfsjdlfkjsdfgsdfg = globals::instances::workspace.findfirstchild("Vehicles").findfirstchild(globals::instances::lp.name);
        if (msdfjsdfsjdlfkjsdfgsdfg.address) {
            auto vehiclevelo = msdfjsdfsjdlfkjsdfgsdfg.get_velocity();

            msdfjsdfsjdlfkjsdfgsdfg.writeprimgrav(0);




            auto cframed = globals::instances::camera.getRot();
            Vector3 look = cframed.getColumn(2);
            Vector3 right = cframed.getColumn(0);

            Vector3 direction;

            if (GetAsyncKeyState('W') & 0x8000) {
                direction = direction - look;
            }
            if (GetAsyncKeyState('S') & 0x8000) {
                direction = direction + look;
            }

            if (GetAsyncKeyState('A') & 0x8000) {
                direction = direction - right;
            }
            if (GetAsyncKeyState('D') & 0x8000) {
                direction = direction + right;
            }

            if (direction.magnitude() > 0)
                direction = direction.normalize();
            for (int i = 0; i < 1500; i++) {
                auto pos = globals::instances::workspace.findfirstchild("Vehicles").findfirstchild(globals::instances::lp.name).get_pos();
                globals::instances::workspace.findfirstchild("Vehicles").findfirstchild(globals::instances::lp.name).write_velocity(Vector3(direction.x * 100, direction.y * 100, direction.z * 100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
	}
}