#include "../classes.h"
#include "../../offsets.h"
#include "../../globals.h"

std::uint64_t roblox::instance::cached_input_object = 0;


void roblox::instance::setFramePosX(uint64_t position) {
     write<uint64_t>(this->address + offsets::FramePositionOffsetX, position);
}

void roblox::instance::setFramePosY(uint64_t position) {
     write<uint64_t>(this->address + offsets::FramePositionOffsetY, position);
}

std::uint64_t get_current_input_object(std::uint64_t base_address) {


    std::uint64_t object_address = read<std::uint64_t>(base_address + 0X118);


    return object_address;
}

#include <thread>
void roblox::cached_input_objectzz() {
    std::uint64_t cachedobj = 0;
    while (true) {
        if (globals::instances::mouseservice) {
            std::uint64_t inputobj = get_current_input_object(globals::instances::mouseservice);

            if (inputobj != 0 && inputobj != 0xffffffffffffffff) {
                cached_input_object = inputobj;
                cachedobj = inputobj;
            }
        }
      //  std::cout << cachedobj << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void roblox::instance::initialize_mouse_service(std::uint64_t address) {
    cached_input_object = get_current_input_object(address);

    if (cached_input_object && cached_input_object != 0xFFFFFFFFFFFFFFFF) {
        const char* base_pointer = reinterpret_cast<const char*>(cached_input_object);

        _mm_prefetch(base_pointer + offsets::MousePosition, _MM_HINT_T0);
        _mm_prefetch(base_pointer + offsets::MousePosition + sizeof(Vector2), _MM_HINT_T0);
    }
}

void roblox::instance::write_mouse_position(std::uint64_t address, float x, float y) {
    cached_input_object = get_current_input_object(address);
    if (cached_input_object != 0 && cached_input_object != 0xFFFFFFFFFFFFFFFF) {
       Vector2 new_position = { x, y };

        write<Vector2>(cached_input_object + offsets::MousePosition, new_position);
    }
}