#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <vector>
#include <future>
#include <memory>
#include <sstream>
#include "../hook.h"
#include "../../util/globals.h"
#include "../../util/classes/classes.h"
#include "../visuals/visuals.h"
#include "../../util/avatarmanager/avatarmanager.h"

template<typename T>
std::string to_string_compat(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> pending_count;

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) : stop(false), pending_count(0) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop.load() || !this->tasks.empty(); });

                        if (this->stop.load() && this->tasks.empty())
                            return;

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        this->pending_count--;
                    }
                    task();
                }
                });
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop.load())
                return;

            tasks.emplace(std::forward<F>(f));
            pending_count++;
        }
        condition.notify_one();
    }

    size_t pending_tasks() {
        return pending_count.load();
    }

    ~ThreadPool() {
        stop = true;
        condition.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};

class ThreadSafeGlobals {
private:
    mutable std::mutex players_mutex;
    mutable std::mutex lp_mutex;
    mutable std::mutex instances_mutex;

public:
    void updateCachedPlayers(const std::vector<roblox::player>& new_players) {
        std::lock_guard<std::mutex> lock(players_mutex);
        globals::instances::cachedplayers = new_players;
    }

    std::vector<roblox::player> getCachedPlayers() const {
        std::lock_guard<std::mutex> lock(players_mutex);
        return globals::instances::cachedplayers;
    }

    void updateLocalPlayer(const roblox::player& lp) {
        std::lock_guard<std::mutex> lock(lp_mutex);
        globals::instances::lp = lp;
    }

    roblox::player getLocalPlayer() const {
        std::lock_guard<std::mutex> lock(lp_mutex);
        return globals::instances::lp;
    }

    template<typename T>
    void updateInstance(T& instance, const T& new_value) {
        std::lock_guard<std::mutex> lock(instances_mutex);
        instance = new_value;
    }

    template<typename T>
    T getInstance(const T& instance) const {
        std::lock_guard<std::mutex> lock(instances_mutex);
        return instance;
    }
};

static std::unique_ptr<ThreadPool> g_thread_pool;
static std::unique_ptr<ThreadSafeGlobals> g_safe_globals;

extern class Logger {
public:
    static void CustomLog(int color, const std::string& start, const std::string& message);
} logger;

class ThreadSafePlayerCache {
private:
    std::atomic<bool> is_running;
    std::atomic<bool> should_stop;
    mutable std::mutex update_mutex;

    static std::unique_ptr<AvatarManager> avatar_manager;

public:
    ThreadSafePlayerCache() : is_running(false), should_stop(false) {}

    void Initialize() {
        if (!g_safe_globals) {
            g_safe_globals = std::unique_ptr<ThreadSafeGlobals>(new ThreadSafeGlobals());
        }

        if (is_valid_address(globals::instances::datamodel.address)) {
            auto players_service = globals::instances::datamodel.read_service("Players");
            g_safe_globals->updateInstance(globals::instances::players, players_service);
        }
    }

    void UpdateCache() {
        if (!g_safe_globals) {
            g_safe_globals = std::unique_ptr<ThreadSafeGlobals>(new ThreadSafeGlobals());
        }

        std::unique_lock<std::mutex> lock(update_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return;
        }

        if (should_stop.load()) {
            return;
        }

        std::vector<roblox::player> tempplayers;
        auto avatar_mgr = avatar_manager.get();

        try {
            if (!is_valid_address(globals::instances::players.address)) {
                return;
            }

            auto players_service = globals::instances::players;
            auto children = players_service.get_children();

            for (roblox::instance& player : children) {
                if (should_stop.load()) break;

                std::this_thread::sleep_for(std::chrono::microseconds(5));

                if (!is_valid_address(player.address))
                    continue;

                if (player.get_class_name() != "Player")
                    continue;

                auto character = player.model_instance();
                if (!is_valid_address(character.address))
                    continue;

                roblox::player entity = buildPlayerEntity(player, character);

                if (is_valid_address(globals::instances::localplayer.address)) {
                    if (entity.name == globals::instances::localplayer.get_name()) {
                        g_safe_globals->updateLocalPlayer(entity);
                    }
                }

                std::this_thread::sleep_for(std::chrono::microseconds(25));
                tempplayers.push_back(entity);
            }

            g_safe_globals->updateCachedPlayers(tempplayers);

        }
        catch (const std::exception& e) {
            logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "CACHE_ERROR",
                "Player cache update failed: " + std::string(e.what()));
        }
        catch (...) {
            logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "CACHE_ERROR",
                "Player cache update failed with unknown exception");
        }
    }

private:
    roblox::player buildPlayerEntity(roblox::instance& player, roblox::instance& character) {
        std::unordered_map<std::string, roblox::instance> cachedchildren;

        for (auto& child : character.get_children()) {
            cachedchildren[child.get_name()] = child;
        }

        roblox::player entity;
        entity.name = player.get_name();
        entity.displayname = player.get_humdisplayname();
        entity.head = cachedchildren["Head"];
        entity.hrp = cachedchildren["HumanoidRootPart"];
        entity.humanoid = cachedchildren["Humanoid"];

        if (cachedchildren["LeftUpperLeg"].address) {
            entity.rigtype = 1;
            entity.uppertorso = cachedchildren["UpperTorso"];
            entity.lowertorso = cachedchildren["LowerTorso"];
            entity.lefthand = cachedchildren["LeftHand"];
            entity.righthand = cachedchildren["RightHand"];
            entity.leftlowerarm = cachedchildren["LeftLowerArm"];
            entity.rightlowerarm = cachedchildren["RightLowerArm"];
            entity.leftupperarm = cachedchildren["LeftUpperArm"];
            entity.rightupperarm = cachedchildren["RightUpperArm"];
            entity.leftfoot = cachedchildren["LeftFoot"];
            entity.leftlowerleg = cachedchildren["LeftLowerLeg"];
            entity.leftupperleg = cachedchildren["LeftUpperLeg"];
            entity.rightlowerleg = cachedchildren["RightLowerLeg"];
            entity.rightfoot = cachedchildren["RightFoot"];
            entity.rightupperleg = cachedchildren["RightUpperLeg"];
        }
        else {
            entity.rigtype = 0;
            entity.uppertorso = cachedchildren["Torso"];
            entity.lefthand = cachedchildren["Left Arm"];
            entity.righthand = cachedchildren["Right Arm"];
            entity.rightfoot = cachedchildren["Left Leg"];
            entity.leftfoot = cachedchildren["Right Leg"];
        }

        if (globals::instances::gamename != "Arsenal") {
            entity.health = entity.humanoid.read_health();
            entity.maxhealth = entity.humanoid.read_maxhealth();
        }
        else {
            try {
                entity.health = player.findfirstchild("NRPBS").findfirstchild("Health").read_double_value();
                entity.maxhealth = player.findfirstchild("NRPBS").findfirstchild("MaxHealth").read_double_value();
            }
            catch (...) {
                entity.health = 0;
                entity.maxhealth = 100;
            }
        }

        entity.userid = player.read_userid();
        entity.instance = character;
        entity.main = player;
        entity.bodyeffects = character.findfirstchild("BodyEffects");
        entity.displayname = player.get_displayname();

        if (entity.displayname.empty()) {
            entity.displayname = entity.humanoid.get_humdisplayname();
        }

        auto tool = character.read_service("Tool");
        entity.tool = tool;
        entity.toolname = tool.address ? tool.get_name() : "[NONE]";

        return entity;
    }

public:
    void Cleanup() {
        should_stop = true;
        std::lock_guard<std::mutex> lock(update_mutex);

        if (g_safe_globals) {
            auto empty_players = std::vector<roblox::player>();
            g_safe_globals->updateCachedPlayers(empty_players);

            auto players_service = roblox::instance();
            g_safe_globals->updateInstance(globals::instances::players, players_service);
        }
    }

    void Refresh() {
        Cleanup();
        should_stop = false;
        Initialize();
    }

    void stop() {
        should_stop = true;
    }
};


class ThreadSafeHookManager {
private:
    std::atomic<bool> hooks_running;
    std::vector<std::thread> hook_threads;

public:
    ThreadSafeHookManager() : hooks_running(false) {}

    void launchThreads() {
        bool expected = false;
        if (!globals::firstreceived)return;
        if (!hooks_running.compare_exchange_strong(expected, true)) {
            return;
        }

        logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            "HOOKS", "Launching individual hook threads...");

        try {
            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting cached_input_objectzz thread");
                roblox::cached_input_objectzz();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting cache thread");
                hooks::cache();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting silent thread");
                hooks::silent();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting speed thread");
                hooks::speed();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting jumppower thread");
                hooks::jumppower();
                });

            /*hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting antistomp thread");
                hooks::antistomp();
                });*/

            /*hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting flight thread");
                hooks::flight();
                });*/

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting rapidfire thread");
                //hooks::rapidfire();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting orbit thread");
                hooks::orbit();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "COMBAT", "Starting triggerbot thread");
                hooks::triggerbot();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting autoreload thread");
                //hooks::autoreload();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting spinbot thread");
                //hooks::spinbot();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting autostomp thread");
                //hooks::autostomp();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting voidhide thread");
                //hooks::voidhide();
                });
            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting hipheight thread");
                //hooks::hipheight();
                });
            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting listener thread");
                hooks::listener();
                });

            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "THREAD", "Starting antisit thread");
                //hooks::antisit();
                });
            hook_threads.emplace_back([]() {
                logger.CustomLog(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "COMBAT", "Starting AIMBOT (combat) thread");
                hooks::aimbot();
                });

            for (auto& thread : hook_threads) {
                thread.detach();
            }

            logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                "HOOKS", "Successfully launched " + to_string_compat(hook_threads.size()) + " individual hook threads");
            logger.CustomLog(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                "COMBAT", "Aimbot and Triggerbot threads are running independently");

        }
        catch (const std::exception& e) {
            logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY,
                "HOOK_ERROR", "Failed to launch hooks: " + std::string(e.what()));
            hooks_running = false;
        }
    }

    void stopHooks() {
        hooks_running = false;

        globals::unattach = true;

        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY,
            "HOOKS", "Signaling all hook threads to stop...");

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        hook_threads.clear();

        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            "HOOKS", "All hook threads stop signal sent");
    }

    bool isRunning() const {
        return hooks_running.load();
    }

    size_t getThreadCount() const {
        return hook_threads.size();
    }
};

std::unique_ptr<AvatarManager> ThreadSafePlayerCache::avatar_manager;

static ThreadSafePlayerCache g_player_cache;
static ThreadSafeHookManager g_hook_manager;

namespace hooking {
    void launchThreads() {
        if (!g_safe_globals) {
            g_safe_globals = std::unique_ptr<ThreadSafeGlobals>(new ThreadSafeGlobals());
        }

        logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            "INIT", "Launching individual threads for maximum performance...");

        g_hook_manager.launchThreads();

        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            "SUCCESS", "All " + to_string_compat(g_hook_manager.getThreadCount()) + " hooks running on separate threads");
    }

    void stopThreads() {
        g_hook_manager.stopHooks();
        g_player_cache.stop();
    }

    bool isRunning() {
        return g_hook_manager.isRunning();
    }
}

class SimplePlayerCache {
public:
    static void Initialize() {
        if (is_valid_address(globals::instances::datamodel.address)) {
            globals::instances::players = globals::instances::datamodel.read_service("Players");
        }
    }

    static void UpdateCache() {
        std::vector<roblox::player> tempplayers;

        if (!is_valid_address(globals::instances::players.address)) {
            return;
        }

        for (roblox::instance& player : globals::instances::players.get_children()) {
            std::this_thread::sleep_for(std::chrono::microseconds(5));

            if (!is_valid_address(player.address))
                continue;

            if (player.get_class_name() != "Player")
                continue;

            auto character = player.model_instance();
            if (!is_valid_address(character.address))
                continue;

            std::unordered_map<std::string, roblox::instance> cachedchildren;
            for (auto& child : character.get_children()) {
                cachedchildren[child.get_name()] = child;
            }

            roblox::player entity;
            entity.name = player.get_name();
            entity.displayname = player.get_humdisplayname();
            entity.head = cachedchildren["Head"];
            entity.hrp = cachedchildren["HumanoidRootPart"];
            entity.humanoid = cachedchildren["Humanoid"];

            if (cachedchildren["LeftUpperLeg"].address) {
                entity.rigtype = 1;
                entity.uppertorso = cachedchildren["UpperTorso"];
                entity.lowertorso = cachedchildren["LowerTorso"];
                entity.lefthand = cachedchildren["LeftHand"];
                entity.righthand = cachedchildren["RightHand"];
                entity.leftlowerarm = cachedchildren["LeftLowerArm"];
                entity.rightlowerarm = cachedchildren["RightLowerArm"];
                entity.leftupperarm = cachedchildren["LeftUpperArm"];
                entity.rightupperarm = cachedchildren["RightUpperArm"];
                entity.leftfoot = cachedchildren["LeftFoot"];
                entity.leftlowerleg = cachedchildren["LeftLowerLeg"];
                entity.leftupperleg = cachedchildren["LeftUpperLeg"];
                entity.rightlowerleg = cachedchildren["RightLowerLeg"];
                entity.rightfoot = cachedchildren["RightFoot"];
                entity.rightupperleg = cachedchildren["RightUpperLeg"];
            }
            else {
                entity.rigtype = 0;
                entity.uppertorso = cachedchildren["Torso"];
                entity.lefthand = cachedchildren["Left Arm"];
                entity.righthand = cachedchildren["Right Arm"];
                entity.rightfoot = cachedchildren["Left Leg"];
                entity.leftfoot = cachedchildren["Right Leg"];
            }

            if (globals::instances::gamename != "Arsenal") {
                entity.health = entity.humanoid.read_health();
                entity.maxhealth = entity.humanoid.read_maxhealth();
            }
            else {
                try {
                    entity.health = player.findfirstchild("NRPBS").findfirstchild("Health").read_double_value();
                    entity.maxhealth = player.findfirstchild("NRPBS").findfirstchild("MaxHealth").read_double_value();
                }
                catch (...) {
                    entity.health = 0;
                    entity.maxhealth = 100;
                }
            }

            entity.userid = player.read_userid();
            entity.instance = character;
            entity.main = player;
            entity.bodyeffects = character.findfirstchild("BodyEffects");
            entity.displayname = player.get_displayname();

            if (entity.displayname.empty()) {
                entity.displayname = entity.humanoid.get_humdisplayname();
            }

            if (entity.name == globals::instances::localplayer.get_name()) {
                globals::instances::lp = entity;
            }

            auto tool = character.read_service("Tool");
            entity.tool = tool;
            entity.toolname = tool.address ? tool.get_name() : "[NONE]";

            std::this_thread::sleep_for(std::chrono::microseconds(25));
            tempplayers.push_back(entity);
        }

        visuals::espData.cachedmutex.lock();
        globals::instances::cachedplayers = tempplayers;
        visuals::espData.cachedmutex.unlock();
    }

    static void Cleanup() {
        globals::instances::players.address = 0;
        globals::instances::cachedplayers.clear();
    }

    static void Refresh() {
        Cleanup();
        Initialize();
    }
};

void player_cache::Initialize() {
    try {
        SimplePlayerCache::Initialize();
        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "CACHE",
            "Player cache initialized successfully");
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "INIT_ERROR",
            "Failed to initialize player cache");
    }
}

void player_cache::UpdateCache() {
    try {
        SimplePlayerCache::UpdateCache();
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "UPDATE_ERROR",
            "Failed to update player cache");
    }
}

void player_cache::Cleanup() {
    try {
        SimplePlayerCache::Cleanup();
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "CLEANUP_ERROR",
            "Failed to cleanup player cache");
    }
}

void player_cache::Refresh() {
    try {
        SimplePlayerCache::Refresh();
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "REFRESH_ERROR",
            "Failed to refresh player cache");
    }
}