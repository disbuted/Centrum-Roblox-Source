#include "../classes.h"

uint32_t roblox::Vector3ToIntBGR(Vector3 color) {
	int blue = static_cast<int>(color.z * 255.0f);
	int green = static_cast<int>(color.y * 255.0f);
	int red = static_cast<int>(color.x * 255.0f);

	if (blue > 255) blue = 255;
	if (blue < 0) blue = 0;
	if (green > 255) green = 255;
	if (green < 0) green = 0;
	if (red > 255) red = 255;
	if (red < 0) red = 0;

	uint32_t result = (static_cast<uint32_t>(blue) << 16) |
		(static_cast<uint32_t>(green) << 8) |
		static_cast<uint32_t>(red);
	return result;
}



int roblox::instance::read_color() {
	return read<int>(this->primitive() + offsets::MeshPartColor3);
}
void roblox::instance::write_color(int col) {
	write<int>(this->primitive() + offsets::MeshPartColor3, col);
}

void roblox::instance::write_ambiance(Vector3 col) {
	write<Vector3>(this->address + 0xd8, col);
	write<Vector3>(this->address + 0xdc, col);
	write<Vector3>(this->address + 0xd0, col);
	write<Vector3>(this->address + 0x108, col);
	write<Vector3>(this->address + 0xf0, col);
	write<Vector3>(this->address + 0xe4, col);
	write<Vector3>(this->address + 0x120, col);
	write<Vector3>(this->address + 0xd4, col);
	//write<Vector3>(this->address + 0xd8, col);
}
void roblox::instance::write_fogcolor(Vector3 col) {
	write<Vector3>(this->address + 0xFC, col);
}
void roblox::instance::write_fogstart(float distance) {
	write<float>(this->address + offsets::FogStart, distance);
}
void roblox::instance::write_fogend(float distance) {
	write<float>(this->address + offsets::FogEnd, distance);
}

void roblox::instance::write_cancollide(bool boolleanz)
{
	write<bool>(this->address + offsets::CanCollide, boolleanz);
}

bool roblox::instance::get_cancollide()
{
	return read<bool>(this->address + offsets::CanCollide);
}
