#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include "../../util/globals.h"

namespace wallcheck {

    struct WallDistance {
        roblox::wall wall;
        float distance_sq;
    };

    class OcclusionBuffer {
    public:
        OcclusionBuffer();
        uint64_t hash_ray(const Vector3& from, const Vector3& to) const;
        bool get_cached_result(const Vector3& from, const Vector3& to, bool& result);
        void cache_result(const Vector3& from, const Vector3& to, bool result);
        void clear_cache();

    private:
        struct CacheEntry {
            bool is_obstructed;
            std::chrono::steady_clock::time_point timestamp;
        };

        std::unordered_map<uint64_t, CacheEntry> cache_;
        std::mutex cache_mutex_;
        std::chrono::milliseconds cache_lifetime_;
        size_t max_cache_size_;
    };

    class WallChecker {
    public:
        WallChecker();
        ~WallChecker();
        void start();
        void stop();
        bool can_see(const Vector3& from, const Vector3& to);
        bool ray_intersects_obb_segment(const Vector3& rayOrigin, const Vector3& rayEnd, const roblox::wall& wall);

    private:
        void initialize_scanning();

        std::unique_ptr<OcclusionBuffer> occlusion_buffer_;
        std::thread scanner_thread_;
        bool should_stop_;
        bool is_running_;
        float min_distance_threshold_;
    };

    extern std::unique_ptr<WallChecker> g_wall_checker;
    extern std::mutex walls_mutex;

    bool is_valid_vector(const Vector3& v);
    bool is_valid_matrix(const Matrix3& m);
    bool is_valid_address(uintptr_t address);
    float distance_squared(const Vector3& a, const Vector3& b);
    float point_to_line_distance_squared(const Vector3& point, const Vector3& lineStart, const Vector3& lineEnd);

    void initialize();
    void shutdown();
    bool can_see(const Vector3& from, const Vector3& to);
}