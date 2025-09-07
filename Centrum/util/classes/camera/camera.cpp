#include "../classes.h"
#include "../../driver/driver.h"
#include "../../offsets.h"
#include "../../globals.h"

void roblox::instance::spectate(uint64_t stringhere) {
	write<std::uint64_t>(globals::instances::camera.address + offsets::CameraSubject, stringhere);
}
void roblox::instance::unspectate() {
	write<std::uint64_t>(globals::instances::camera.address + offsets::CameraSubject, globals::instances::lp.humanoid.address);
}
Vector3 roblox::camera::getPos() {
	return read<Vector3>(this->address + offsets::CameraPos);
}
Matrix3 roblox::camera::getRot() {
	return read<Matrix3>(this->address + offsets::CameraRotation);
}
void roblox::camera::writePos(Vector3 arg) {
	write<Vector3>(this->address + offsets::CameraPos, arg);
}
void roblox::camera::writeRot(Matrix3 arg) {
	write<Matrix3>(this->address + offsets::CameraRotation, arg);
}

int roblox::camera::getType() {
	return read<int>(this->address + offsets::CameraType);
}
void roblox::camera::setType(int type) {
	write<int>(this->address + offsets::CameraType, type);
}
roblox::instance roblox::camera::getSubject() {
	return read<roblox::instance>(this->address + offsets::CameraSubject);
}



int roblox::camera::getMode() {
	return read<int>(this->address + offsets::CameraMode);
}
void roblox::camera::setMode(int type) {
	write<int>(this->address + offsets::CameraMode, type);
}