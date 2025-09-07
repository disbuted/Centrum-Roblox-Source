#pragma once
#include "../util/classes/classes.h"
#include "../drawing/overlay/keybind/keybind.h"
namespace globals
{
	inline bool focused;
	inline std::vector<roblox::wall> walls;
	namespace bools {
		inline bool bring, autokill, kill;
		inline std::string name;
		inline roblox::player entity;

	}
	namespace instances {
		inline std::vector<std::string> whitelist;
		inline std::vector<std::string> blacklist;
		inline roblox::instance visualengine;
		inline roblox::instance datamodel;
		inline roblox::instance workspace;
		inline roblox::instance players;
		inline roblox::player lp;
		inline roblox::instance lighting;
		inline roblox::camera camera;
		inline roblox::instance localplayer;
		inline std::string gamename = "Universal";
		inline std::string username = "???";
		inline std::vector<roblox::player> cachedplayers;
		inline roblox::player cachedtarget;
		inline roblox::player cachedlasttarget;
		inline roblox::instance aim;
		inline uintptr_t mouseservice;
		inline ImDrawList* draw;
	}
	namespace combat {
		inline bool rapidfire;
		inline bool autostomp;
				inline bool aimbot = false;
		inline bool stickyaim = true;
		inline int aimbottype = 0;
		inline keybind aimbotkeybind("aimbotkeybind");
				inline bool usefov = false;
		inline bool drawfov = false;
		inline float fovsize = 50;
		inline bool glowfov = false;
		inline float fovcolor[4] = {1,1,1,1};
		inline float fovglowcolor[4] = {1,1,1,1};
				inline bool smoothing = false;
		inline float smoothingx = 5;
		inline float smoothingy = 5;
				inline bool predictions = false;
		inline float predictionsx = 5;
		inline float predictionsy = 5;
				inline bool autoswitch = false;
		inline bool teamcheck = false;
		inline bool knockcheck = false;
		inline bool rangecheck = false;
		inline bool healthcheck = false;
		inline bool wallcheck = false;
		inline std::vector<int>* flags = new std::vector<int>{ 0, 0, 0, 0, 0,0,0};
		inline float range = 1000;
		inline float healththreshhold = 10;
				inline int aimpart = 0;
				inline bool silentaim = false;
		inline bool stickyaimsilent = true;
		inline bool spoofmouse = true;
		inline int hitchance = 100;
		inline keybind silentaimkeybind("silentaimkeybind");
				inline bool usesfov = false;
		inline bool drawsfov = false;
		inline float sfovsize = 50;
		inline bool glowsfov = false;
		inline float sfovcolor[4] = { 1,1,1,1 };
		inline float sfovglowcolor[4] = { 1,1,1,1 };
				inline bool orbit = false;
				inline std::vector<int> orbittype = std::vector<int>(3, 0);
		inline float orbitspeed = 8;
		inline float orbitrange = 3;
		inline float orbitheight = 1;
		inline bool drawradiusring = false;
		inline keybind orbitkeybind("orbitkeybind                 ");
	//	inline std::vector<int>* orbittype;
				inline bool triggerbot = false;
				inline bool antiaim = false;
				inline bool underground_antiaim = false;
		inline float delay = 0;
		inline float releasedelay = 0;
		inline bool triggerbotrange = 0;
		inline float triggerbotrangevalue = 50;
		inline keybind triggerbotkeybind("triggerbotkeybind");
	}
	namespace visuals {
				inline bool visuals = true;
		inline bool boxes = false;
		inline bool boxfill = false;
		inline bool lockedindicator = false;
		inline bool oofarrows = false;
		inline bool snapline = false;
		inline bool glowesp = false;
		inline int boxtype = 0;
		inline bool health = false;
		inline bool healthbar = false;
		inline bool name = false;
		inline int nametype = 0;
		inline bool toolesp = false;
		inline bool distance = false;
		inline bool flags = false;		inline bool chams = false;
		inline bool skeletons = false;
		inline bool localplayer = false;
		inline bool aimviewer = false;
		inline bool esppreview = false;
		inline bool predictionsdot = false;

		inline float boxcolors[4] = { 1,1,1,1 };
		inline float boxfillcolor[4] = { 0.2,0.2,0.2,0.3 };
		inline float glowcolor[4] =  { 1,1,1,1};
		inline float lockedcolor[4] =  { 1,0,0,1 };
		inline float oofcolor[4] = { 1,1,1,1 };
		inline float snaplinecolor[4] = { 1,1,1,1 };
		inline float healthbarcolor[4] = { 1,0,0,1 };
		inline float healthbarcolor1[4] = { 0,1,0,1 };
		inline float namecolor[4] = { 1,1,1,1 };
		inline float toolespcolor[4] = { 1,1,1,1 };
		inline float distancecolor[4] = { 1,1,1,1 };
		inline float chamscolor[4] = { 1,1,1,0.3f };
		inline float skeletonscolor[4] = { 1,1,1,1 };

				inline bool fortniteindicator = false;
		inline bool hittracer = false;
		inline bool trail = false;
		inline bool hitbubble = false;
		inline bool targetchams = false;
		inline bool targetskeleton = false;
		inline bool localplayerchams = false;
		inline bool localgunchams = false;
				inline bool enemycheck = true;
		inline bool friendlycheck = true;
		inline bool teamcheck = false;
		inline bool rangecheck = false;
		inline float range = 1000;
			inline std::vector<int>* box_overlay_flags = new std::vector<int>{ 0, 0, 0 };
		inline std::vector<int>* name_overlay_flags = new std::vector<int>{ 0, 0 };
		inline std::vector<int>* healthbar_overlay_flags = new std::vector<int>{ 0, 0, 0 };
		inline std::vector<int>* tool_overlay_flags = new std::vector<int>{ 0, 0 };
		inline std::vector<int>* distance_overlay_flags = new std::vector<int>{ 0, 0 };
		inline std::vector<int>* skeleton_overlay_flags = new std::vector<int>{ 0, 0 };
		inline std::vector<int>* chams_overlay_flags = new std::vector<int>{ 0, 0, 0 };
		inline std::vector<int>* snapline_overlay_flags = new std::vector<int>{ 0, 0 };
		inline std::vector<int>* oof_overlay_flags = new std::vector<int>{ 0, 0, 0 };



	}
	namespace misc {
				inline bool speed = false;
		inline int speedtype = 0;
		inline float speedvalue = 16;
		inline keybind speedkeybind("speedkeybind");
				inline bool jumppower = false;
		inline float jumpowervalue = 50;
		inline keybind jumppowerkeybind("jumppowerkeybind");
				inline bool voidhide = false;
		inline keybind voidhidebind("voidehidkeybind");
				inline bool flight = false;
		inline int flighttype = 0;
		inline float flightvalue = 16;
		inline keybind flightkeybind("flightkeybind");
				inline bool hipheight = false;
		inline  float hipheightvalue = 16;
				inline bool rapidfire = false;
		inline bool autoarmor = false;
		inline bool autoreload = false;
		inline bool autostomp = false;
		inline bool antistomp = false;
		inline bool bikefly = false;
		inline keybind stompkeybind("stompkeybind");

		inline bool spectate = false;
		inline keybind spectatebind("spectatebind");

		

				inline bool vsync = true;
		inline bool watermark = true;
		inline std::vector<int>* watermarkstuff = new std::vector<int>{ 1, 1, 0 };
			inline int keybindsstyle;
		inline bool targethud = false;
		inline bool playerlist = false;
		inline bool streamproof = false;
		inline bool keybinds = false;
		inline bool spotify = false;
		inline bool explorer = false;
		inline bool colors = false;
	}

	inline bool unattach = false;
	inline bool stop;
	inline bool firstreceived = false;
	inline bool handlingtp = false;
}