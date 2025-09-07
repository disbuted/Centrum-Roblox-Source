#include "../classes.h"

roblox::instance roblox::instance::read_userid() {
    return read<roblox::instance>(this->address + offsets::UserId);
}
void roblox::instance::writeprimgrav(float gravity) {
    write<float>(read<uintptr_t>(this->address + offsets::Primitive) + offsets::PrimitiveGravity, gravity);
}

float roblox::instance::read_primgrav() {
    return read<float>(read<uintptr_t>(this->address + offsets::Primitive) + offsets::PrimitiveGravity);
}
float roblox::instance::read_walkspeed() {
    if (!is_valid_address(address)) return 0;
    return read<float>(this->address + offsets::WalkSpeed);
}
void roblox::instance::write_walkspeed(float value) {
     write<float>(this->address + offsets::WalkSpeed, value);
     write<float>(this->address + offsets::WalkSpeedCheck, value);
}
float roblox::instance::read_jumppower() {
    return read<float>(this->address + offsets::JumpPower);
}
void roblox::instance::write_jumppower(float value) {
    write<float>(this->address + offsets::JumpPower, value);
}

float roblox::instance::read_hipheight() {
    return read<float>(this->address + offsets::HipHeight);
}
std::string roblox::instance::read_animationid() {
    return read<std::string>(this->address + offsets::AnimationId);
}
void roblox::instance::write_animationid(std::string animid) {
    write<std::string>(this->address + offsets::AnimationId, animid);
}
void roblox::instance::write_hipheight(float value) {
    write<float>(this->address + offsets::HipHeight, value);
}
bool roblox::instance::read_sitting() {
    return read<bool>(this->address + offsets::Sit);
}
void roblox::instance::write_sitting(bool value) {
    write<bool>(this->address + offsets::Sit, value);
}
