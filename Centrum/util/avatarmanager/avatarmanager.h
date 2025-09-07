#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>
#include <d3d11.h>
#include <chrono>
#include "../../drawing/imgui/imgui.h"

struct ImVec2;
typedef void* ImTextureID;

struct AvatarTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int width = 0;
    int height = 0;
    bool loaded = false;
    std::chrono::steady_clock::time_point last_accessed;
};

enum class AvatarState {
    NotRequested,
    Downloading,
    Ready,
    Failed,
    Blocked
};

class AvatarManager {
private:
    ID3D11Device* d3d_device;
    ID3D11DeviceContext* d3d_context;

    std::unordered_map<std::string, AvatarTexture> texture_cache;
    std::unordered_map<std::string, std::vector<uint8_t>> image_cache;
    std::unordered_map<std::string, AvatarState> avatar_states;
    std::unordered_map<std::string, std::future<std::vector<uint8_t>>> pending_downloads;

    std::mutex cache_mutex;
    std::mutex download_mutex;

    std::chrono::steady_clock::time_point last_rate_limit_time;

    static constexpr size_t MAX_CACHE_SIZE = 100;
    static constexpr size_t MAX_MEMORY_MB = 50;
    static constexpr int DOWNLOAD_TIMEOUT = 30000;

    void cleanupOldTextures();
    std::vector<uint8_t> downloadAvatarSync(const std::string& userId);
    std::vector<uint8_t> downloadImageFromUrl(const std::string& host, const std::string& path);
    ID3D11ShaderResourceView* createTextureFromBytes(const std::vector<uint8_t>& imageData, int& width, int& height);
    void processCompletedDownloads();

public:
    AvatarManager(ID3D11Device* device, ID3D11DeviceContext* context);
    ~AvatarManager();

    void requestAvatar(const std::string& userId);
    void requestAvatar(uint64_t userId);
    ImTextureID getAvatarTexture(const std::string& userId);
    ImTextureID getAvatarTexture(uint64_t userId);
    AvatarState getAvatarState(const std::string& userId);

    void update();
    void clearCache();

    void renderAvatar(const std::string& userId, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1));
    void renderAvatar(uint64_t userId, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1));
};