#include "../classes.h"
#include <Windows.h>
#include "../../../util/driver/driver.h"
#include <vector>
#include "../../../util/offsets.h"
#include "../../protection/hider.h"
#include "../../protection/keyauth/skStr.h"
#include <thread>
#include "../../../drawing/overlay/overlay.h"
#include <chrono>
#include "../../globals.h"
#include "../../../features/hook.h"

inline class Logger {
public:
    static void GetTime(char(&timeBuffer)[9])
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm localTime;
        localtime_s(&localTime, &now_time);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
    }

    static void Log(const std::string& message) {
        char timeBuffer[9];
        GetTime(timeBuffer);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "[" << timeBuffer << "] ";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << message << "\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void CustomLog(int color, const std::string& start, const std::string& message) {
        char timeBuffer[9];
        GetTime(timeBuffer);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, color | FOREGROUND_INTENSITY);
        std::cout << "[" << start << "] ";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << message << "\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Logf(const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Log(std::string(buffer));
    }

    static void LogfCustom(int color, const char* start, const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        CustomLog(color, start, std::string(buffer));
    }

    static void Banner() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << "[ROBLOX CHEEZE]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Separator() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << "================================================================================\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Section(const std::string& title) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "\n[== " << title << " ==============================================================]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Success(const std::string& message) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "[== " << message << " COMPLETE ==]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

}logger;

roblox::instance ReadVisualEngine() {
    return read<roblox::instance>(base_address + offsets::VisualEnginePointer);
}

roblox::instance ReadDatamodel() {
    uintptr_t padding = read<uintptr_t>(ReadVisualEngine().address + offsets::VisualEngineToDataModel1);
    return read<roblox::instance>(padding + offsets::VisualEngineToDataModel2);
}

bool engine::startup() {
    system("cls");
    logger.Banner();

    logger.Section("CORE INITIALIZATION");
    logger.CustomLog(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "ENGINE", "Starting Core Systems...");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "DRIVER", "Initializing memory driver interface...");
    mem::initialize();

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "SCANNER", "Scanning for Roblox process...");
    std::cout << "GRABBING ROBLOX PID" << std::endl;
    std::cout << "CLOSING HANDLE" << std::endl;
    mem::grabroblox_h();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Memory interface established");

    logger.Section("INSTANCE RESOLUTION");
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RESOLVER", "Reading core Roblox instances...");

    roblox::instance visualengine = ReadVisualEngine();
    roblox::instance datamodel = ReadDatamodel();
    roblox::instance workspace = datamodel.read_workspace();
    roblox::instance players = datamodel.read_service("Players");
    roblox::instance localplayer = players.local_player();
    roblox::instance lighting = datamodel.read_service("Lighting");
    roblox::instance camera = workspace.read_service("Camera");

    logger.Section("ADDRESS VALIDATION");

    if (is_valid_address(visualengine.address)) {
        globals::instances::visualengine = visualengine;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "VisualEngine    -> 0x%p", visualengine.address);
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Base Address   -> 0x%p", base_address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "VisualEngine Address -> 0x0");
    }

    if (is_valid_address(datamodel.address)) {
        globals::instances::datamodel = datamodel;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "DataModel      -> 0x%p", datamodel.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "DataModel Address -> 0x0");
    }

    if (is_valid_address(workspace.address)) {
        globals::instances::workspace = workspace;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Workspace      -> 0x%p", workspace.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Workspace Address -> 0x0");
    }

    if (is_valid_address(players.address)) {
        globals::instances::players = players;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Players        -> 0x%p", players.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Players Address -> 0x0");
    }

    if (is_valid_address(localplayer.address)) {
        globals::instances::localplayer = localplayer;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "LocalPlayer    -> 0x%p", localplayer.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "LocalPlayer Address -> 0x0");
    }

    if (is_valid_address(lighting.address)) {
        globals::instances::lighting = lighting;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Lighting       -> 0x%p", lighting.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Lighting Address -> 0x0");
    }

    if (is_valid_address(camera.address)) {
        globals::instances::camera = static_cast<roblox::camera>(camera.address);
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Camera         -> 0x%p", camera.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Camera Address -> 0x0");
    }

    logger.Section("THREAD INITIALIZATION");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "OVERLAY", "Launching overlay interface...");
    std::thread overlay(overlay::load_interface);
    overlay.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Overlay thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "CACHE", "Initializing player cache system...");
    player_cache playercache;
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Player cache initialized");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MONITOR", "Starting teleport detection thread...");
    std::thread PlayerCacheThread([&playercache]() {
        while (true)
        {
            if (globals::unattach) break;

            try {
                roblox::instance datamodel = ReadDatamodel();
                if (globals::instances::datamodel.address != datamodel.address) {
                    system("cls");
                    logger.Banner();
                    logger.Section("TELEPORT RECOVERY");
                    logger.CustomLog(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY, "TELEPORT", "Game teleport detected! Emergency rescan initiated...");
                    globals::handlingtp = true;

                    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RECOVERY", "Reinitializing driver connection...");
                  //  mem::initialize();
                  //  mem::grabroblox_h();

                    std::this_thread::sleep_for(std::chrono::seconds(4));
                    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RECOVERY", "Resolving new instance addresses...");

                    roblox::instance visualengine = ReadVisualEngine();
                    roblox::instance datamodel = ReadDatamodel();
                    roblox::instance workspace = datamodel.read_workspace();
                    roblox::instance players = datamodel.read_service("Players");
                    roblox::instance localplayer = players.local_player();
                    roblox::instance lighting = datamodel.read_service("Lighting");
                    roblox::instance camera = workspace.read_service("Camera");
                    playercache.Refresh();

                    logger.Section("RECOVERY VALIDATION");

                    if (is_valid_address(datamodel.address)) {
                        globals::instances::datamodel = datamodel;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "DataModel      -> 0x%p", datamodel.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "DataModel recovery failed");
                    }

                    if (is_valid_address(workspace.address)) {
                        globals::instances::workspace = workspace;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Workspace      -> 0x%p", workspace.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Workspace recovery failed");
                    }

                    if (is_valid_address(players.address)) {
                        globals::instances::players = players;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Players        -> 0x%p", players.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Players recovery failed");
                    }

                    if (is_valid_address(localplayer.address)) {
                        globals::instances::localplayer = localplayer;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "LocalPlayer    -> 0x%p", localplayer.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "LocalPlayer recovery failed");
                    }

                    if (is_valid_address(lighting.address)) {
                        globals::instances::lighting = lighting;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Lighting       -> 0x%p", lighting.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Lighting recovery failed");
                    }

                    if (is_valid_address(camera.address)) {
                        globals::instances::camera = static_cast<roblox::camera>(camera.address);
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Camera         -> 0x%p", camera.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Camera recovery failed");
                    }

                    logger.Success("TELEPORT RECOVERY COMPLETED SUCCESSFULLY");
                    globals::handlingtp = false;
                }
                else {
                    playercache.UpdateCache();
                }
            }
            catch (...) {
                logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Exception in monitor thread");
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        });
    PlayerCacheThread.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Monitor thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "HOOKS", "Launching exploitation hooks...");
    try {
        hooking::launchThreads();

        logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "WAIT", "Waiting for all threads to initialize...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "All hook threads launched and initialized");
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Failed to launch hook threads");
    }


    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "RUNTIME", "Engine running... Press ENTER to terminate");
    logger.Separator();
    try {
        while (true) {
            if (globals::unattach) {
                logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Unattach signal received");
                break;
            }

            if (std::cin.peek() != EOF) {
                char c;
                if (std::cin.get(c)) {
                    if (c == '\n' || c == '\r') {
                        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "User requested termination");
                        break;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Exception in main loop");
    }

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Initiating cleanup sequence...");
    globals::unattach = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SHUTDOWN", "Engine terminated successfully");

    return true;
}