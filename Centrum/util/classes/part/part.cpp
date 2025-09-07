#include "../classes.h"


Vector3 roblox::instance::get_part_size() {
	return read<Vector3>(this->primitive() + offsets::PartSize);
}

void roblox::instance::write_part_size(Vector3 size) {
	 write<Vector3>(this->primitive() + offsets::PartSize, size);
}

Vector3 roblox::instance::get_move_dir() {
	return read<Vector3>(this->address + offsets::MoveDirection);
}

void roblox::instance::write_move_dir(Vector3 size) {
	write<Vector3>(this->address + offsets::MoveDirection, size);
}