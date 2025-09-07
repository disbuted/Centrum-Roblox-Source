#include "../../util/classes/classes.h"
#include "../../features/hook.h"
#include "../globals.h"
#include "../../drawing/overlay/overlay.h"
#include <windows.h>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <wininet.h>
#include <string>
#include <ShlObj.h>
#include <functional>

namespace roblox {
	int roblox::instance::fetch_entity(std::uint64_t address) const {
		return read<int>(address);
	}
	std::atomic<bool> run = true;
	std::mutex cachedplayer_mx;

	void entitylist(std::vector<roblox::player>& oldlists, const std::vector<roblox::player>& newerlists) {
		::SetThreadPriority(::GetCurrentThread(), 0x80);
		if (newerlists.empty()) return;
		std::vector<roblox::player> entitylist1;
	}
}