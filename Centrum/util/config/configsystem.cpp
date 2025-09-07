#include "configsystem.h"
#include <iostream>
#include "../notification/notification.h"

ConfigSystem::ConfigSystem() {
        char* appdata_path;
    size_t len;
    _dupenv_s(&appdata_path, &len, "APPDATA");

    if (appdata_path) {
        config_directory = std::string(appdata_path) + "\\centrum\\config";
        free(appdata_path);
    }

        if (!fs::exists(config_directory)) {
        fs::create_directories(config_directory);
    }

    refresh_config_list();
}

void ConfigSystem::refresh_config_list() {
    config_files.clear();

    if (fs::exists(config_directory)) {
        for (const auto& entry : fs::directory_iterator(config_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cent") {
                config_files.push_back(entry.path().stem().string());
            }
        }
    }
}

bool ConfigSystem::save_config(const std::string& name) {
    if (name.empty()) return false;

    json config_json;

        std::cout << "[COMBAT] Saving combat settings..." << "\n";
    config_json["combat"]["rapidfire"] = globals::combat::rapidfire;
    config_json["combat"]["autostomp"] = globals::combat::autostomp;
    config_json["combat"]["aimbot"] = globals::combat::aimbot;
    config_json["combat"]["stickyaim"] = globals::combat::stickyaim;
    config_json["combat"]["aimbottype"] = globals::combat::aimbottype;
    config_json["combat"]["usefov"] = globals::combat::usefov;
    config_json["combat"]["drawfov"] = globals::combat::drawfov;
    config_json["combat"]["fovsize"] = globals::combat::fovsize;
    config_json["combat"]["glowfov"] = globals::combat::glowfov;
    config_json["combat"]["fovcolor"] = std::vector<float>(globals::combat::fovcolor, globals::combat::fovcolor + 4);
    config_json["combat"]["fovglowcolor"] = std::vector<float>(globals::combat::fovglowcolor, globals::combat::fovglowcolor + 4);
    config_json["combat"]["smoothing"] = globals::combat::smoothing;
    config_json["combat"]["smoothingx"] = globals::combat::smoothingx;
    config_json["combat"]["smoothingy"] = globals::combat::smoothingy;
    config_json["combat"]["predictions"] = globals::combat::predictions;
    config_json["combat"]["predictionsx"] = globals::combat::predictionsx;
    config_json["combat"]["predictionsy"] = globals::combat::predictionsy;
    config_json["combat"]["teamcheck"] = globals::combat::teamcheck;
    config_json["combat"]["knockcheck"] = globals::combat::knockcheck;
    config_json["combat"]["rangecheck"] = globals::combat::rangecheck;
    config_json["combat"]["healthcheck"] = globals::combat::healthcheck;
    config_json["combat"]["range"] = globals::combat::range;
    config_json["combat"]["healththreshhold"] = globals::combat::healththreshhold;
    config_json["combat"]["aimpart"] = globals::combat::aimpart;
    config_json["combat"]["silentaim"] = globals::combat::silentaim;
    config_json["combat"]["stickyaimsilent"] = globals::combat::stickyaimsilent;
    config_json["combat"]["spoofmouse"] = globals::combat::spoofmouse;
    config_json["combat"]["hitchance"] = globals::combat::hitchance;
    config_json["combat"]["usesfov"] = globals::combat::usesfov;
    config_json["combat"]["drawsfov"] = globals::combat::drawsfov;
    config_json["combat"]["sfovsize"] = globals::combat::sfovsize;
    config_json["combat"]["glowsfov"] = globals::combat::glowsfov;
    config_json["combat"]["sfovcolor"] = std::vector<float>(globals::combat::sfovcolor, globals::combat::sfovcolor + 4);
    config_json["combat"]["sfovglowcolor"] = std::vector<float>(globals::combat::sfovglowcolor, globals::combat::sfovglowcolor + 4);
    config_json["combat"]["orbit"] = globals::combat::orbit;
    config_json["combat"]["orbitspeed"] = globals::combat::orbitspeed;
    config_json["combat"]["orbitrange"] = globals::combat::orbitrange;
    config_json["combat"]["orbitheight"] = globals::combat::orbitheight;
    config_json["combat"]["drawradiusring"] = globals::combat::drawradiusring;
    config_json["combat"]["triggerbot"] = globals::combat::triggerbot;
    config_json["combat"]["delay"] = globals::combat::delay;
    config_json["combat"]["releasedelay"] = globals::combat::releasedelay;
    config_json["combat"]["triggerbotrange"] = globals::combat::triggerbotrange;
    config_json["combat"]["triggerbotrangevalue"] = globals::combat::triggerbotrangevalue;

        std::cout << "[COMBAT][KEYBIND] Saving combat keybinds..." << "\n";
    std::cout << "[COMBAT][KEYBIND] Aimbot key: " << globals::combat::aimbotkeybind.key << "\n";
    config_json["combat"]["keybinds"]["aimbotkeybind"]["key"] = globals::combat::aimbotkeybind.key;
    config_json["combat"]["keybinds"]["aimbotkeybind"]["type"] = static_cast<int>(globals::combat::aimbotkeybind.type);
    std::cout << "[COMBAT][KEYBIND] Aimbot type: " << globals::combat::aimbotkeybind.type << "\n";
    config_json["combat"]["keybinds"]["silentaimkeybind"]["key"] = globals::combat::silentaimkeybind.key;
    config_json["combat"]["keybinds"]["silentaimkeybind"]["type"] = static_cast<int>(globals::combat::silentaimkeybind.type);
    config_json["combat"]["keybinds"]["orbitkeybind"]["key"] = globals::combat::orbitkeybind.key;
    config_json["combat"]["keybinds"]["orbitkeybind"]["type"] = static_cast<int>(globals::combat::orbitkeybind.type);
    config_json["combat"]["keybinds"]["triggerbotkeybind"]["key"] = globals::combat::triggerbotkeybind.key;
    config_json["combat"]["keybinds"]["triggerbotkeybind"]["type"] = static_cast<int>(globals::combat::triggerbotkeybind.type);

        std::cout << "[VISUALS] Saving visual settings..." << "\n";
    config_json["visuals"]["visuals"] = globals::visuals::visuals;
    config_json["visuals"]["boxes"] = globals::visuals::boxes;
    config_json["visuals"]["boxfill"] = globals::visuals::boxfill;
    config_json["visuals"]["lockedindicator"] = globals::visuals::lockedindicator;
    config_json["visuals"]["oofarrows"] = globals::visuals::oofarrows;
    config_json["visuals"]["snapline"] = globals::visuals::snapline;
    config_json["visuals"]["glowesp"] = globals::visuals::glowesp;
    config_json["visuals"]["boxtype"] = globals::visuals::boxtype;
    config_json["visuals"]["health"] = globals::visuals::health;
    config_json["visuals"]["healthbar"] = globals::visuals::healthbar;
    config_json["visuals"]["name"] = globals::visuals::name;
    config_json["visuals"]["nametype"] = globals::visuals::nametype;
    config_json["visuals"]["toolesp"] = globals::visuals::toolesp;
    config_json["visuals"]["distance"] = globals::visuals::distance;
    config_json["visuals"]["flags"] = globals::visuals::flags;
    config_json["visuals"]["chams"] = globals::visuals::chams;
    config_json["visuals"]["skeletons"] = globals::visuals::skeletons;
    config_json["visuals"]["localplayer"] = globals::visuals::localplayer;
    config_json["visuals"]["aimviewer"] = globals::visuals::aimviewer;
    config_json["visuals"]["esppreview"] = globals::visuals::esppreview;
    config_json["visuals"]["predictionsdot"] = globals::visuals::predictionsdot;
    config_json["visuals"]["boxcolors"] = std::vector<float>(globals::visuals::boxcolors, globals::visuals::boxcolors + 4);
    config_json["visuals"]["boxfillcolor"] = std::vector<float>(globals::visuals::boxfillcolor, globals::visuals::boxfillcolor + 4);
    config_json["visuals"]["glowcolor"] = std::vector<float>(globals::visuals::glowcolor, globals::visuals::glowcolor + 4);
    config_json["visuals"]["lockedcolor"] = std::vector<float>(globals::visuals::lockedcolor, globals::visuals::lockedcolor + 4);
    config_json["visuals"]["oofcolor"] = std::vector<float>(globals::visuals::oofcolor, globals::visuals::oofcolor + 4);
    config_json["visuals"]["snaplinecolor"] = std::vector<float>(globals::visuals::snaplinecolor, globals::visuals::snaplinecolor + 4);
    config_json["visuals"]["healthbarcolor"] = std::vector<float>(globals::visuals::healthbarcolor, globals::visuals::healthbarcolor + 4);
    config_json["visuals"]["healthbarcolor1"] = std::vector<float>(globals::visuals::healthbarcolor1, globals::visuals::healthbarcolor1 + 4);
    config_json["visuals"]["namecolor"] = std::vector<float>(globals::visuals::namecolor, globals::visuals::namecolor + 4);
    config_json["visuals"]["toolespcolor"] = std::vector<float>(globals::visuals::toolespcolor, globals::visuals::toolespcolor + 4);
    config_json["visuals"]["distancecolor"] = std::vector<float>(globals::visuals::distancecolor, globals::visuals::distancecolor + 4);
    config_json["visuals"]["chamscolor"] = std::vector<float>(globals::visuals::chamscolor, globals::visuals::chamscolor + 4);
    config_json["visuals"]["skeletonscolor"] = std::vector<float>(globals::visuals::skeletonscolor, globals::visuals::skeletonscolor + 4);
    config_json["visuals"]["fortniteindicator"] = globals::visuals::fortniteindicator;
    config_json["visuals"]["hittracer"] = globals::visuals::hittracer;
    config_json["visuals"]["trail"] = globals::visuals::trail;
    config_json["visuals"]["hitbubble"] = globals::visuals::hitbubble;
    config_json["visuals"]["targetchams"] = globals::visuals::targetchams;
    config_json["visuals"]["targetskeleton"] = globals::visuals::targetskeleton;
    config_json["visuals"]["localplayerchams"] = globals::visuals::localplayerchams;
    config_json["visuals"]["localgunchams"] = globals::visuals::localgunchams;
    config_json["visuals"]["enemycheck"] = globals::visuals::enemycheck;
    config_json["visuals"]["friendlycheck"] = globals::visuals::friendlycheck;
    config_json["visuals"]["teamcheck"] = globals::visuals::teamcheck;
    config_json["visuals"]["rangecheck"] = globals::visuals::rangecheck;
    config_json["visuals"]["range"] = globals::visuals::range;

        std::cout << "[MISC] Saving misc settings..." << "\n";
    config_json["misc"]["speed"] = globals::misc::speed;
    config_json["misc"]["speedtype"] = globals::misc::speedtype;
    config_json["misc"]["speedvalue"] = globals::misc::speedvalue;
    config_json["misc"]["jumppower"] = globals::misc::jumppower;
    config_json["misc"]["jumpowervalue"] = globals::misc::jumpowervalue;
    config_json["misc"]["flight"] = globals::misc::flight;
    config_json["misc"]["flighttype"] = globals::misc::flighttype;
    config_json["misc"]["flightvalue"] = globals::misc::flightvalue;
    config_json["misc"]["hipheight"] = globals::misc::hipheight;
    config_json["misc"]["hipheightvalue"] = globals::misc::hipheightvalue;
    config_json["misc"]["rapidfire"] = globals::misc::rapidfire;
    config_json["misc"]["autoarmor"] = globals::misc::autoarmor;
    config_json["misc"]["autoreload"] = globals::misc::autoreload;
    config_json["misc"]["autostomp"] = globals::misc::autostomp;
    config_json["misc"]["antistomp"] = globals::misc::antistomp;
    config_json["misc"]["vsync"] = globals::misc::vsync;
    config_json["misc"]["watermark"] = globals::misc::watermark;
    config_json["misc"]["targethud"] = globals::misc::targethud;
    config_json["misc"]["playerlist"] = globals::misc::playerlist;
    config_json["misc"]["keybinds"] = globals::misc::keybinds;
    config_json["misc"]["spotify"] = globals::misc::spotify;
    config_json["misc"]["explorer"] = globals::misc::explorer;
    config_json["misc"]["colors"] = globals::misc::colors;
    config_json["misc"]["voidhide"] = globals::misc::voidhide;
        std::cout << "[MISC][KEYBIND] Saving misc keybinds..." << "\n";
    try {
        config_json["misc"]["keybinds_data"]["speedkeybind"]["key"] = globals::misc::speedkeybind.key;
        config_json["misc"]["keybinds_data"]["speedkeybind"]["type"] = static_cast<int>(globals::misc::speedkeybind.type);
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["key"] = globals::misc::jumppowerkeybind.key;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["type"] = static_cast<int>(globals::misc::jumppowerkeybind.type);
        config_json["misc"]["keybinds_data"]["flightkeybind"]["key"] = globals::misc::flightkeybind.key;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["type"] = static_cast<int>(globals::misc::flightkeybind.type);
        config_json["misc"]["keybinds_data"]["stompkeybind"]["key"] = globals::misc::stompkeybind.key;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["type"] = static_cast<int>(globals::misc::stompkeybind.type);
        config_json["misc"]["keybinds_data"]["voidhidebind"]["type"] = static_cast<int>(globals::misc::voidhidebind.type);
        std::cout << "[MISC][KEYBIND] Successfully saved misc keybinds" << "\n";
    }
    catch (...) {
        std::cout << "[MISC][KEYBIND] Failed to save keybinds, using defaults" << "\n";
                config_json["misc"]["keybinds_data"]["speedkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["speedkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["type"] = 1;
    }

    std::string filepath = config_directory + "\\" + name + ".cent";
    std::ofstream file(filepath);

    if (file.is_open()) {
        file << config_json.dump(2);         file.close();

        refresh_config_list();
        current_config_name = name;
        std::cout << "[CONFIG] Successfully saved config: " << name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Failed to save config: " << name << "\n";
    return false;
}

bool ConfigSystem::load_config(const std::string& name) {
    if (name.empty()) return false;

    std::string filepath = config_directory + "\\" + name + ".cent";
    std::ifstream file(filepath);

    if (file.is_open()) {
        try {
            json config_json;
            file >> config_json;

            std::cout << "[CONFIG] Loading config: " << name << "\n";

                        if (config_json.contains("combat")) {
                std::cout << "[COMBAT] Loading combat settings..." << "\n";
                auto& combat = config_json["combat"];
                if (combat.contains("rapidfire")) globals::combat::rapidfire = combat["rapidfire"];
                if (combat.contains("autostomp")) globals::combat::autostomp = combat["autostomp"];
                if (combat.contains("aimbot")) globals::combat::aimbot = combat["aimbot"];
                if (combat.contains("stickyaim")) globals::combat::stickyaim = combat["stickyaim"];
                if (combat.contains("aimbottype")) globals::combat::aimbottype = combat["aimbottype"];
                if (combat.contains("usefov")) globals::combat::usefov = combat["usefov"];
                if (combat.contains("drawfov")) globals::combat::drawfov = combat["drawfov"];
                if (combat.contains("fovsize")) globals::combat::fovsize = combat["fovsize"];
                if (combat.contains("glowfov")) globals::combat::glowfov = combat["glowfov"];
                if (combat.contains("fovcolor")) {
                    auto colors = combat["fovcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::combat::fovcolor[i] = colors[i];
                }
                if (combat.contains("fovglowcolor")) {
                    auto colors = combat["fovglowcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::combat::fovglowcolor[i] = colors[i];
                }
                if (combat.contains("smoothing")) globals::combat::smoothing = combat["smoothing"];
                if (combat.contains("smoothingx")) globals::combat::smoothingx = combat["smoothingx"];
                if (combat.contains("smoothingy")) globals::combat::smoothingy = combat["smoothingy"];
                if (combat.contains("predictions")) globals::combat::predictions = combat["predictions"];
                if (combat.contains("predictionsx")) globals::combat::predictionsx = combat["predictionsx"];
                if (combat.contains("predictionsy")) globals::combat::predictionsy = combat["predictionsy"];
                if (combat.contains("teamcheck")) globals::combat::teamcheck = combat["teamcheck"];
                if (combat.contains("knockcheck")) globals::combat::knockcheck = combat["knockcheck"];
                if (combat.contains("rangecheck")) globals::combat::rangecheck = combat["rangecheck"];
                if (combat.contains("healthcheck")) globals::combat::healthcheck = combat["healthcheck"];
                if (combat.contains("range")) globals::combat::range = combat["range"];
                if (combat.contains("healththreshhold")) globals::combat::healththreshhold = combat["healththreshhold"];
                if (combat.contains("aimpart")) globals::combat::aimpart = combat["aimpart"];
                if (combat.contains("silentaim")) globals::combat::silentaim = combat["silentaim"];
                if (combat.contains("stickyaimsilent")) globals::combat::stickyaimsilent = combat["stickyaimsilent"];
                if (combat.contains("spoofmouse")) globals::combat::spoofmouse = combat["spoofmouse"];
                if (combat.contains("hitchance")) globals::combat::hitchance = combat["hitchance"];
                if (combat.contains("usesfov")) globals::combat::usesfov = combat["usesfov"];
                if (combat.contains("drawsfov")) globals::combat::drawsfov = combat["drawsfov"];
                if (combat.contains("sfovsize")) globals::combat::sfovsize = combat["sfovsize"];
                if (combat.contains("glowsfov")) globals::combat::glowsfov = combat["glowsfov"];
                if (combat.contains("sfovcolor")) {
                    auto colors = combat["sfovcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::combat::sfovcolor[i] = colors[i];
                }
                if (combat.contains("sfovglowcolor")) {
                    auto colors = combat["sfovglowcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::combat::sfovglowcolor[i] = colors[i];
                }
                if (combat.contains("orbit")) globals::combat::orbit = combat["orbit"];
                if (combat.contains("orbitspeed")) globals::combat::orbitspeed = combat["orbitspeed"];
                if (combat.contains("orbitrange")) globals::combat::orbitrange = combat["orbitrange"];
                if (combat.contains("orbitheight")) globals::combat::orbitheight = combat["orbitheight"];
                if (combat.contains("drawradiusring")) globals::combat::drawradiusring = combat["drawradiusring"];
                if (combat.contains("triggerbot")) globals::combat::triggerbot = combat["triggerbot"];
                if (combat.contains("delay")) globals::combat::delay = combat["delay"];
                if (combat.contains("releasedelay")) globals::combat::releasedelay = combat["releasedelay"];
                if (combat.contains("triggerbotrange")) globals::combat::triggerbotrange = combat["triggerbotrange"];
                if (combat.contains("triggerbotrangevalue")) globals::combat::triggerbotrangevalue = combat["triggerbotrangevalue"];

                                if (combat.contains("keybinds")) {
                    std::cout << "[COMBAT][KEYBIND] Loading combat keybinds..." << "\n";
                    auto& keybinds = combat["keybinds"];
                    if (keybinds.contains("aimbotkeybind")) {
                        if (keybinds["aimbotkeybind"].contains("key"))
                            globals::combat::aimbotkeybind.key = keybinds["aimbotkeybind"]["key"];
                        if (keybinds["aimbotkeybind"].contains("type"))
                            globals::combat::aimbotkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["aimbotkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("silentaimkeybind")) {
                        if (keybinds["silentaimkeybind"].contains("key"))
                            globals::combat::silentaimkeybind.key = keybinds["silentaimkeybind"]["key"];
                        if (keybinds["silentaimkeybind"].contains("type"))
                            globals::combat::silentaimkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["silentaimkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("orbitkeybind")) {
                        if (keybinds["orbitkeybind"].contains("key"))
                            globals::combat::orbitkeybind.key = keybinds["orbitkeybind"]["key"];
                        if (keybinds["orbitkeybind"].contains("type"))
                            globals::combat::orbitkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["orbitkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("triggerbotkeybind")) {
                        if (keybinds["triggerbotkeybind"].contains("key"))
                            globals::combat::triggerbotkeybind.key = keybinds["triggerbotkeybind"]["key"];
                        if (keybinds["triggerbotkeybind"].contains("type"))
                            globals::combat::triggerbotkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["triggerbotkeybind"]["type"].get<int>());
                    }
                    std::cout << "[COMBAT][KEYBIND] Combat keybinds loaded successfully" << "\n";
                }
            }

                        if (config_json.contains("visuals")) {
                std::cout << "[VISUALS] Loading visual settings..." << "\n";
                auto& visuals = config_json["visuals"];
                if (visuals.contains("visuals")) globals::visuals::visuals = visuals["visuals"];
                if (visuals.contains("boxes")) globals::visuals::boxes = visuals["boxes"];
                if (visuals.contains("boxfill")) globals::visuals::boxfill = visuals["boxfill"];
                if (visuals.contains("lockedindicator")) globals::visuals::lockedindicator = visuals["lockedindicator"];
                if (visuals.contains("oofarrows")) globals::visuals::oofarrows = visuals["oofarrows"];
                if (visuals.contains("snapline")) globals::visuals::snapline = visuals["snapline"];
                if (visuals.contains("glowesp")) globals::visuals::glowesp = visuals["glowesp"];
                if (visuals.contains("boxtype")) globals::visuals::boxtype = visuals["boxtype"];
                if (visuals.contains("health")) globals::visuals::health = visuals["health"];
                if (visuals.contains("healthbar")) globals::visuals::healthbar = visuals["healthbar"];
                if (visuals.contains("name")) globals::visuals::name = visuals["name"];
                if (visuals.contains("nametype")) globals::visuals::nametype = visuals["nametype"];
                if (visuals.contains("toolesp")) globals::visuals::toolesp = visuals["toolesp"];
                if (visuals.contains("distance")) globals::visuals::distance = visuals["distance"];
                if (visuals.contains("flags")) globals::visuals::flags = visuals["flags"];
                if (visuals.contains("chams")) globals::visuals::chams = visuals["chams"];
                if (visuals.contains("skeletons")) globals::visuals::skeletons = visuals["skeletons"];
                if (visuals.contains("localplayer")) globals::visuals::localplayer = visuals["localplayer"];
                if (visuals.contains("aimviewer")) globals::visuals::aimviewer = visuals["aimviewer"];
                if (visuals.contains("esppreview")) globals::visuals::esppreview = visuals["esppreview"];
                if (visuals.contains("predictionsdot")) globals::visuals::predictionsdot = visuals["predictionsdot"];
                if (visuals.contains("boxcolors")) {
                    auto colors = visuals["boxcolors"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::boxcolors[i] = colors[i];
                }
                if (visuals.contains("boxfillcolor")) {
                    auto colors = visuals["boxfillcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::boxfillcolor[i] = colors[i];
                }
                if (visuals.contains("glowcolor")) {
                    auto colors = visuals["glowcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::glowcolor[i] = colors[i];
                }
                if (visuals.contains("lockedcolor")) {
                    auto colors = visuals["lockedcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::lockedcolor[i] = colors[i];
                }
                if (visuals.contains("oofcolor")) {
                    auto colors = visuals["oofcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::oofcolor[i] = colors[i];
                }
                if (visuals.contains("snaplinecolor")) {
                    auto colors = visuals["snaplinecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::snaplinecolor[i] = colors[i];
                }
                if (visuals.contains("healthbarcolor")) {
                    auto colors = visuals["healthbarcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::healthbarcolor[i] = colors[i];
                }
                if (visuals.contains("healthbarcolor1")) {
                    auto colors = visuals["healthbarcolor1"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::healthbarcolor1[i] = colors[i];
                }
                if (visuals.contains("namecolor")) {
                    auto colors = visuals["namecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::namecolor[i] = colors[i];
                }
                if (visuals.contains("toolespcolor")) {
                    auto colors = visuals["toolespcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::toolespcolor[i] = colors[i];
                }
                if (visuals.contains("distancecolor")) {
                    auto colors = visuals["distancecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::distancecolor[i] = colors[i];
                }
                if (visuals.contains("chamscolor")) {
                    auto colors = visuals["chamscolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::chamscolor[i] = colors[i];
                }
                if (visuals.contains("skeletonscolor")) {
                    auto colors = visuals["skeletonscolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::skeletonscolor[i] = colors[i];
                }
                if (visuals.contains("fortniteindicator")) globals::visuals::fortniteindicator = visuals["fortniteindicator"];
                if (visuals.contains("hittracer")) globals::visuals::hittracer = visuals["hittracer"];
                if (visuals.contains("trail")) globals::visuals::trail = visuals["trail"];
                if (visuals.contains("hitbubble")) globals::visuals::hitbubble = visuals["hitbubble"];
                if (visuals.contains("targetchams")) globals::visuals::targetchams = visuals["targetchams"];
                if (visuals.contains("targetskeleton")) globals::visuals::targetskeleton = visuals["targetskeleton"];
                if (visuals.contains("localplayerchams")) globals::visuals::localplayerchams = visuals["localplayerchams"];
                if (visuals.contains("localgunchams")) globals::visuals::localgunchams = visuals["localgunchams"];
                if (visuals.contains("enemycheck")) globals::visuals::enemycheck = visuals["enemycheck"];
                if (visuals.contains("friendlycheck")) globals::visuals::friendlycheck = visuals["friendlycheck"];
                if (visuals.contains("teamcheck")) globals::visuals::teamcheck = visuals["teamcheck"];
                if (visuals.contains("rangecheck")) globals::visuals::rangecheck = visuals["rangecheck"];
                if (visuals.contains("range")) globals::visuals::range = visuals["range"];
            }

                        if (config_json.contains("misc")) {
                std::cout << "[MISC] Loading misc settings..." << "\n";
                auto& misc = config_json["misc"];
                if (misc.contains("speed")) globals::misc::speed = misc["speed"];
                if (misc.contains("speedtype")) globals::misc::speedtype = misc["speedtype"];
                if (misc.contains("speedvalue")) globals::misc::speedvalue = misc["speedvalue"];
                if (misc.contains("jumppower")) globals::misc::jumppower = misc["jumppower"];
                if (misc.contains("jumpowervalue")) globals::misc::jumpowervalue = misc["jumpowervalue"];
                if (misc.contains("flight")) globals::misc::flight = misc["flight"];
                if (misc.contains("flighttype")) globals::misc::flighttype = misc["flighttype"];
                if (misc.contains("flightvalue")) globals::misc::flightvalue = misc["flightvalue"];
                if (misc.contains("hipheight")) globals::misc::hipheight = misc["hipheight"];
                if (misc.contains("hipheightvalue")) globals::misc::hipheightvalue = misc["hipheightvalue"];
                if (misc.contains("rapidfire")) globals::misc::rapidfire = misc["rapidfire"];
                if (misc.contains("autoarmor")) globals::misc::autoarmor = misc["autoarmor"];
                if (misc.contains("autoreload")) globals::misc::autoreload = misc["autoreload"];
                if (misc.contains("autostomp")) globals::misc::autostomp = misc["autostomp"];
                if (misc.contains("antistomp")) globals::misc::antistomp = misc["antistomp"];
                if (misc.contains("voidhide")) globals::misc::voidhide = misc["voidhide"];
                if (misc.contains("vsync")) globals::misc::vsync = misc["vsync"];
                if (misc.contains("watermark")) globals::misc::watermark = misc["watermark"];
                if (misc.contains("targethud")) globals::misc::targethud = misc["targethud"];
                if (misc.contains("playerlist")) globals::misc::playerlist = misc["playerlist"];
                if (misc.contains("keybinds")) globals::misc::keybinds = misc["keybinds"];
                if (misc.contains("spotify")) globals::misc::spotify = misc["spotify"];
                if (misc.contains("explorer")) globals::misc::explorer = misc["explorer"];
                if (misc.contains("colors")) globals::misc::colors = misc["colors"];

                                if (misc.contains("keybinds_data")) {
                    std::cout << "[MISC][KEYBIND] Loading misc keybinds..." << "\n";
                    auto& keybinds = misc["keybinds_data"];
                    if (keybinds.contains("speedkeybind")) {
                        if (keybinds["speedkeybind"].contains("key"))
                            globals::misc::speedkeybind.key = keybinds["speedkeybind"]["key"];
                        if (keybinds["speedkeybind"].contains("type"))
                            globals::misc::speedkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["speedkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("voidhidebind")) {
                        if (keybinds["voidhidebind"].contains("key"))
                            globals::misc::voidhidebind.key = keybinds["voidhidebind"]["key"];
                        if (keybinds["voidhidebind"].contains("type"))
                            globals::misc::voidhidebind.type = static_cast<keybind::c_keybind_type>(keybinds["voidhidebind"]["type"].get<int>());
                    }
                    if (keybinds.contains("jumppowerkeybind")) {
                        if (keybinds["jumppowerkeybind"].contains("key"))
                            globals::misc::jumppowerkeybind.key = keybinds["jumppowerkeybind"]["key"];
                        if (keybinds["jumppowerkeybind"].contains("type"))
                            globals::misc::jumppowerkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["jumppowerkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("flightkeybind")) {
                        if (keybinds["flightkeybind"].contains("key"))
                            globals::misc::flightkeybind.key = keybinds["flightkeybind"]["key"];
                        if (keybinds["flightkeybind"].contains("type"))
                            globals::misc::flightkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["flightkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("stompkeybind")) {
                        if (keybinds["stompkeybind"].contains("key"))
                            globals::misc::stompkeybind.key = keybinds["stompkeybind"]["key"];
                        if (keybinds["stompkeybind"].contains("type"))
                            globals::misc::stompkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["stompkeybind"]["type"].get<int>());
                    }
                    std::cout << "[MISC][KEYBIND] Misc keybinds loaded successfully" << "\n";
                }
            }

            file.close();
            current_config_name = name;
            std::cout << "[CONFIG] Successfully loaded config: " << name << "\n";
            return true;
        }
        catch (const json::parse_error& e) {
            std::cout << "[CONFIG] Failed to parse config file: " << e.what() << "\n";
            file.close();
            return false;
        }
    }

    std::cout << "[CONFIG] Failed to open config file: " << name << "\n";
    return false;
}

bool ConfigSystem::delete_config(const std::string& name) {
    if (name.empty()) return false;

    std::string filepath = config_directory + "\\" + name + ".cent";

    if (fs::exists(filepath)) {
        fs::remove(filepath);
        refresh_config_list();

        if (current_config_name == name) {
            current_config_name.clear();
        }

        std::cout << "[CONFIG] Successfully deleted config: " << name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Config file not found: " << name << "\n";
    return false;
}

void ConfigSystem::render_config_ui(float width, float height) {
    ImAdd::BeginChild("CONFIG SYSTEM", ImVec2(width, height));
    {
                ImGui::Text("Config Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##config_name", config_name_buffer, sizeof(config_name_buffer));
        ImGui::Spacing();

                ImGui::Columns(3, nullptr, false);

        if (ImAdd::Button("Save Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Save button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                if (save_config(std::string(config_name_buffer))) {
                    Notifications::Success("Config '" + std::string(config_name_buffer) + "' saved successfully!");
                    memset(config_name_buffer, 0, sizeof(config_name_buffer));
                }
                else {
                    Notifications::Error("Failed to save config '" + std::string(config_name_buffer) + "'!");
                }
            }
            else {
                Notifications::Warning("Please enter a config name!");
            }
        }

        ImGui::NextColumn();

        if (ImAdd::Button("Load Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Load button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                if (load_config(std::string(config_name_buffer))) {
                    Notifications::Success("Config '" + std::string(config_name_buffer) + "' loaded successfully!");
                }
                else {
                    Notifications::Error("Failed to load config '" + std::string(config_name_buffer) + "'!");
                }
            }
            else {
                Notifications::Warning("Please enter a config name or select from list!");
            }
        }

        ImGui::NextColumn();

        if (ImAdd::Button("Delete Config", ImVec2(-1, 25))) {
            std::cout << "[CONFIG] Delete button pressed" << "\n";
            if (strlen(config_name_buffer) > 0) {
                std::string config_name = std::string(config_name_buffer);
                if (delete_config(config_name)) {
                    Notifications::Success("Config was deleted!");
                    memset(config_name_buffer, 0, sizeof(config_name_buffer));
                }
                else {
                    Notifications::Error("Failed To Delete!");
                }
            }
            else {
                Notifications::Warning("Select A Config!");
            }
        }

        ImGui::Columns(1);
        ImGui::Spacing();

                if (!config_files.empty()) {
            ImGui::Text("Available Configs:");
            ImGui::BeginChild("config_list", ImVec2(-1, -30));
            {
                for (const auto& config : config_files) {
                    bool is_selected = (current_config_name == config);

                    if (ImGui::Selectable(config.c_str(), is_selected)) {
                        std::cout << "[CONFIG] Selected config: " << config << "\n";
                        strcpy_s(config_name_buffer, sizeof(config_name_buffer), config.c_str());
                        Notifications::Info("Selected config: " + config, 2.0f);
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndChild();

            if (ImAdd::Button("Refresh List", ImVec2(-1, 25))) {
                std::cout << "[CONFIG] Refreshing config list..." << "\n";
                size_t old_count = config_files.size();
                refresh_config_list();
                size_t new_count = config_files.size();

                if (new_count != old_count) {
                    Notifications::Info("Config list refreshed - Found " + std::to_string(new_count) + " configs", 2.0f);
                }
                else {
                    Notifications::Info("Config list refreshed", 1.5f);
                }
            }
        }
        else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No configs found");

                        ImGui::Spacing();
            ImGui::TextWrapped("Create your first config by entering a name above and clicking 'Save Config'.");
        }
    }
    ImAdd::EndChild();
}

const std::string& ConfigSystem::get_current_config() const {
    return current_config_name;
}