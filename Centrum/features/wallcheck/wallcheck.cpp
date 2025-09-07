#include "wallcheck.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include "../../util/globals.h"
#define max
#undef max
#define min
#undef min
namespace wallcheck {

    std::unique_ptr<WallChecker> g_wall_checker = nullptr;
    std::mutex walls_mutex;

    bool is_valid_vector(const Vector3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
            !std::isnan(v.x) && !std::isnan(v.y) && !std::isnan(v.z) &&
            std::abs(v.x) < 1e6f && std::abs(v.y) < 1e6f && std::abs(v.z) < 1e6f &&
            !(v.x == 0.0f && v.y == 0.0f && v.z == 0.0f);
    }

    bool is_valid_matrix(const Matrix3& m) {
        for (int i = 0; i < 9; i++) {
            float val = m.data[i];
            if (!std::isfinite(val) || std::isnan(val) || std::abs(val) > 10.0f) {
                return false;
            }
        }
        return true;
    }

    bool is_valid_address(uintptr_t address) {
        return address != 0 && address > 0x1000;
    }

    OcclusionBuffer::OcclusionBuffer() : cache_lifetime_(500), max_cache_size_(10000) {}

    uint64_t OcclusionBuffer::hash_ray(const Vector3& from, const Vector3& to) const {
        auto quantize = [](float f) -> uint32_t {
            return static_cast<uint32_t>(std::round(f * 100.0f));
            };

        uint64_t hash1 = (static_cast<uint64_t>(quantize(from.x)) << 32) | quantize(from.y);
        uint64_t hash2 = (static_cast<uint64_t>(quantize(from.z)) << 32) | quantize(to.x);
        uint64_t hash3 = (static_cast<uint64_t>(quantize(to.y)) << 32) | quantize(to.z);

        return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
    }

    bool OcclusionBuffer::get_cached_result(const Vector3& from, const Vector3& to, bool& result) {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        uint64_t key = hash_ray(from, to);
        auto it = cache_.find(key);

        if (it != cache_.end()) {
            auto now = std::chrono::steady_clock::now();
            if (now - it->second.timestamp < cache_lifetime_) {
                result = it->second.is_obstructed;
                return true;
            }
            else {
                cache_.erase(it);
            }
        }

        return false;
    }

    void OcclusionBuffer::cache_result(const Vector3& from, const Vector3& to, bool result) {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        if (cache_.size() >= max_cache_size_) {
            auto now = std::chrono::steady_clock::now();
            for (auto it = cache_.begin(); it != cache_.end();) {
                if (now - it->second.timestamp >= cache_lifetime_) {
                    it = cache_.erase(it);
                }
                else {
                    ++it;
                }
            }
            if (cache_.size() >= max_cache_size_) {
                cache_.clear();
            }
        }

        uint64_t key = hash_ray(from, to);
        cache_[key] = { result, std::chrono::steady_clock::now() };
    }

    void OcclusionBuffer::clear_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.clear();
    }

    WallChecker::WallChecker()
        : should_stop_(false), is_running_(false), min_distance_threshold_(0.1f) {
        occlusion_buffer_ = std::make_unique<OcclusionBuffer>();
    }

    WallChecker::~WallChecker() {
        stop();
    }

    void WallChecker::start() {
        if (is_running_) return;

        should_stop_ = false;
        is_running_ = true;
        scanner_thread_ = std::thread([this]() { initialize_scanning(); });
    }

    void WallChecker::stop() {
        if (!is_running_) return;

        should_stop_ = true;

        if (scanner_thread_.joinable()) {
            scanner_thread_.join();
        }

        is_running_ = false;
    }

    bool WallChecker::can_see(const Vector3& from, const Vector3& to) {
        if (!is_valid_vector(from) || !is_valid_vector(to)) {
            return true;
        }

        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float dz = to.z - from.z;
        float distance_sq = dx * dx + dy * dy + dz * dz;
        float min_threshold_sq = min_distance_threshold_ * min_distance_threshold_;

        if (distance_sq < min_threshold_sq) {
            return true;
        }

        bool cached_result;
        if (occlusion_buffer_->get_cached_result(from, to, cached_result)) {
            return !cached_result;
        }

        std::vector<roblox::wall> local_walls;
        {
            std::lock_guard<std::mutex> lock(walls_mutex);
            local_walls = globals::walls;
        }

        local_walls.erase(
            std::remove_if(local_walls.begin(), local_walls.end(),
                [](const roblox::wall& wall) {
                    return wall.position.x == 0.0f && wall.position.y == 0.0f && wall.position.z == 0.0f;
                }),
            local_walls.end()
        );

        bool obstructed = false;
        int intersections = 0;

        for (const auto& wall : local_walls) {
            if (ray_intersects_obb_segment(from, to, wall)) {
                intersections++;
                obstructed = true;


                break;
            }
        }

        occlusion_buffer_->cache_result(from, to, obstructed);

        return !obstructed;
    }

    static bool firsttime = true;

    void WallChecker::initialize_scanning() {
        std::unordered_set<uintptr_t> knownPrimitives;
        std::vector<roblox::wall> wallCache;

        while (!should_stop_) {

            if (!globals::instances::workspace.address) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            if (!firsttime) {
                std::this_thread::sleep_for(std::chrono::seconds(15));
            }

            if (should_stop_) break;

            knownPrimitives.clear();
            wallCache.clear();

            if (!is_valid_address(globals::instances::workspace.address)) {
                std::cout << "Invalid workspace address, skipping wall scan" << std::endl;
                continue;
            }

                        for (roblox::player& player : globals::instances::cachedplayers) {
                if (!is_valid_address(player.head.address)) {
                    continue;
                }

                std::vector<roblox::instance> bones;

                try {
                    auto parentChildren = player.head.read_parent().get_children();
                    auto headChildren = player.head.get_children();

                    bones.insert(bones.end(), parentChildren.begin(), parentChildren.end());
                    bones.insert(bones.end(), headChildren.begin(), headChildren.end());

                    player.bones = bones;

                    for (const auto& bone : bones) {
                        if (!is_valid_address(bone.address)) {
                            continue;
                        }

                        uintptr_t primitive = read<uintptr_t>(bone.address + offsets::Primitive);
                        if (is_valid_address(primitive)) {
                            knownPrimitives.insert(primitive);
                        }
                    }
                }
                catch (...) {
                    continue;
                }
            }

                        uintptr_t walls1_addr = 0;
            uintptr_t walls2_addr = 0;
            uintptr_t primitiveBase = 0;

            try {
                walls1_addr = read<uintptr_t>(globals::instances::workspace.address + offsets::PrimitivesPointer1);
                if (!is_valid_address(walls1_addr)) {
                    std::cout << "Invalid walls1 address: 0x" << std::hex << walls1_addr << std::endl;
                    continue;
                }

                walls2_addr = read<uintptr_t>(walls1_addr + offsets::PrimitivesPointer2);
                if (!is_valid_address(walls2_addr)) {
                    std::cout << "Invalid walls2 address: 0x" << std::hex << walls2_addr << std::endl;
                    continue;
                }

                primitiveBase = walls2_addr;
            }
            catch (...) {
                std::cout << "Failed to read primitive base addresses" << std::endl;
                continue;
            }

            int maxCount = 0xffffff;
            int validWallCount = 0;
            int invalidAddressCount = 0;

            for (int i = 0; i < maxCount && validWallCount < 100000; i += sizeof(uintptr_t)) {
                if (should_stop_) break;

                uintptr_t currentPrimitive = 0;

                try {
                    currentPrimitive = read<uintptr_t>(primitiveBase + i);
                }
                catch (...) {
                    break;
                }

                if (!is_valid_address(currentPrimitive)) {
                    invalidAddressCount++;
                    if (invalidAddressCount > 1000) {
                        break;
                    }
                    continue;
                }

                invalidAddressCount = 0;

                try {
                    int validateValue = read<int>(currentPrimitive + 0x8);
                    if (validateValue != offsets::PrimitiveValidateValue) {
                        continue;
                    }

                    if (knownPrimitives.find(currentPrimitive) != knownPrimitives.end()) {
                        continue;
                    }

                    Vector3 size = read<Vector3>(currentPrimitive + offsets::PartSize);
                    bool anchored = read<bool>(currentPrimitive + offsets::Anchored);
                    float transparency = read<float>(currentPrimitive + offsets::Transparency);
                    bool cancollide = read<bool>(currentPrimitive + offsets::CanCollide);

                    if (!cancollide) continue;
                    if (!anchored) continue;
                    if (transparency >= 1.f) continue; 

                    if (!is_valid_vector(size) || size == Vector3(0, 0, 0)) {
                        continue;
                    }

                    if (size.x > 500.0f || size.y > 500.0f || size.z > 500.0f ||
                        size.x < 0.8f || size.y < 0.5f || size.z < 0.8f) {
                        continue;
                    }

                    CFrame cframe = read<CFrame>(currentPrimitive + offsets::CFrame);
                    if (!is_valid_vector(cframe.position)) {
                        continue;
                    }

                    if (cframe.position.x == 0.0f && cframe.position.y == 0.0f && cframe.position.z == 0.0f) {
                        continue;
                    }

                    float distance_from_origin = sqrt(cframe.position.x * cframe.position.x +
                        cframe.position.y * cframe.position.y +
                        cframe.position.z * cframe.position.z);
                    if (distance_from_origin < 5.0f) {
                        continue;
                    }

                    if (!is_valid_matrix(cframe.rotation_matrix)) {
                        continue;
                    }

                    roblox::wall wall;
                    wall.size = size;
                    wall.position = cframe.position;
                    wall.rotation = cframe.rotation_matrix;
                    wall.cframe = cframe;

                    wallCache.emplace_back(wall);
                    validWallCount++;
                }
                catch (...) {
                    continue;
                }
            }

            {
                std::lock_guard<std::mutex> lock(walls_mutex);
                globals::walls = std::move(wallCache);
            }
            firsttime = false;
            std::cout << "Scanned " << std::dec << globals::walls.size() << " valid walls!" << std::endl;
        }
    }

    bool WallChecker::ray_intersects_obb_segment(const Vector3& rayOrigin, const Vector3& rayEnd, const roblox::wall& wall) {
        if (!is_valid_vector(rayOrigin) || !is_valid_vector(rayEnd) ||
            !is_valid_vector(wall.position) || !is_valid_vector(wall.size)) {
            return false;
        }

        Vector3 direction = { rayEnd.x - rayOrigin.x, rayEnd.y - rayOrigin.y, rayEnd.z - rayOrigin.z };
        float segmentLength = sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

        if (segmentLength < 1e-6f) return false;

        direction.x /= segmentLength;
        direction.y /= segmentLength;
        direction.z /= segmentLength;

        Matrix3 inverseRotation = wall.rotation.Transpose();
        Vector3 relativeOrigin = { rayOrigin.x - wall.position.x, rayOrigin.y - wall.position.y, rayOrigin.z - wall.position.z };
        Vector3 localOrigin = inverseRotation.multiplyVector(relativeOrigin);
        Vector3 localDir = inverseRotation.multiplyVector(direction);

        if (!is_valid_vector(localOrigin) || !is_valid_vector(localDir)) {
            return false;
        }

        const float epsilon = 1e-6f;
        Vector3 halfSize = { wall.size.x / 2.0f - epsilon, wall.size.y / 2.0f - epsilon, wall.size.z / 2.0f - epsilon };
        Vector3 min = { -halfSize.x, -halfSize.y, -halfSize.z };
        Vector3 max = { halfSize.x, halfSize.y, halfSize.z };

        float tmin = 0.0f;
        float tmax = segmentLength;

        if (std::abs(localDir.x) < epsilon) {
            if (localOrigin.x < min.x || localOrigin.x > max.x) return false;
        }
        else {
            float t1 = (min.x - localOrigin.x) / localDir.x;
            float t2 = (max.x - localOrigin.x) / localDir.x;
            if (!std::isfinite(t1) || !std::isfinite(t2)) return false;

            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax + epsilon) return false;
        }
        if (std::abs(localDir.y) < epsilon) {
            if (localOrigin.y < min.y || localOrigin.y > max.y) return false;
        }
        else {
            float t1 = (min.y - localOrigin.y) / localDir.y;
            float t2 = (max.y - localOrigin.y) / localDir.y;
            if (!std::isfinite(t1) || !std::isfinite(t2)) return false;

            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax + epsilon) return false;
        }

        if (std::abs(localDir.z) < epsilon) {
            if (localOrigin.z < min.z || localOrigin.z > max.z) return false;
        }
        else {
            float t1 = (min.z - localOrigin.z) / localDir.z;
            float t2 = (max.z - localOrigin.z) / localDir.z;
            if (!std::isfinite(t1) || !std::isfinite(t2)) return false;

            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax + epsilon) return false;
        }

        bool intersects = tmin <= tmax && tmax >= -epsilon && tmin <= segmentLength + epsilon;

        if (intersects && (tmin < -epsilon || tmin > segmentLength + epsilon)) {
            intersects = false;
        }

        return intersects;
    }

    void initialize() {
        if (!g_wall_checker) {
            g_wall_checker = std::make_unique<WallChecker>();
            g_wall_checker->start();
        }
    }

    void shutdown() {
        if (g_wall_checker) {
            g_wall_checker->stop();
            g_wall_checker.reset();
        }
    }

    bool can_see(const Vector3& from, const Vector3& to) {
        if (!g_wall_checker) {
            initialize();
        }
        return g_wall_checker->can_see(from, to);
    }

}