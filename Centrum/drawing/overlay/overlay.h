#pragma once
#include <memory>
#include "../../util/avatarmanager/avatarmanager.h"

namespace overlay {
	void load_interface();
	inline bool visible = false;

	inline std::unique_ptr<AvatarManager> avatar_manager;
	inline std::vector<uint64_t> last_known_players; 

	static void initialize_avatar_system();
	static void update_avatars();
	static void update_lobby_players(); 	static AvatarManager* get_avatar_manager();
	static void cleanup_avatar_system();
}