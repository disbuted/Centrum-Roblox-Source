#include "AvatarManager.h"
#include "../../drawing/imgui/imgui.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#define min
#undef min
AvatarManager::AvatarManager(ID3D11Device* device, ID3D11DeviceContext* context)
    : d3d_device(device), d3d_context(context) {
    if (d3d_device) {
        d3d_device->AddRef();
    }
    if (d3d_context) {
        d3d_context->AddRef();
    }

    last_rate_limit_time = std::chrono::steady_clock::time_point{};
}

AvatarManager::~AvatarManager() {
    {
        std::lock_guard<std::mutex> lock(download_mutex);
        for (auto& pair : pending_downloads) {
            if (pair.second.valid()) {
                try {
                    pair.second.wait_for(std::chrono::milliseconds(100));
                }
                catch (...) {
                }
            }
        }
        pending_downloads.clear();
    }

    for (auto& pair : texture_cache) {
        if (pair.second.srv) {
            pair.second.srv->Release();
        }
    }

    if (d3d_device) {
        d3d_device->Release();
    }
    if (d3d_context) {
        d3d_context->Release();
    }
}

void AvatarManager::requestAvatar(uint64_t userId) {
    std::string userIdStr = std::to_string(userId);
    requestAvatar(userIdStr);
}

void AvatarManager::requestAvatar(const std::string& userId) {
    if (userId.empty()) return;

    std::lock_guard<std::mutex> lock(cache_mutex);

    if (image_cache.find(userId) != image_cache.end()) {
        return;
    }

    if (avatar_states.find(userId) != avatar_states.end()) {
        AvatarState state = avatar_states[userId];
        if (state == AvatarState::Ready || state == AvatarState::Downloading || state == AvatarState::Blocked) {
            return;
        }
    }

    auto now = std::chrono::steady_clock::now();
    auto time_since_rate_limit = now - last_rate_limit_time;
    auto seconds_since = std::chrono::duration_cast<std::chrono::seconds>(time_since_rate_limit).count();

    if (last_rate_limit_time != std::chrono::steady_clock::time_point{} &&
        time_since_rate_limit < std::chrono::seconds(15)) {
             avatar_states[userId] = AvatarState::Failed;
        return;
    }

    avatar_states[userId] = AvatarState::Downloading;
  
    std::lock_guard<std::mutex> download_lock(download_mutex);
    if (pending_downloads.find(userId) == pending_downloads.end()) {
        pending_downloads[userId] = std::async(std::launch::async, [this, userId]() {
            return downloadAvatarSync(userId);
            });
    }
}

std::vector<uint8_t> AvatarManager::downloadImageFromUrl(const std::string& host, const std::string& path) {
    std::vector<uint8_t> result;

    HINTERNET hSession = WinHttpOpen(L"RobloxOverlay/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
             return result;
    }

    WinHttpSetTimeouts(hSession, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT);

    std::wstring whost(host.begin(), host.end());

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (!hConnect) {
              WinHttpCloseHandle(hSession);
        return result;
    }

    std::wstring wpath(path.begin(), path.end());

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
            WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (bResults) {
             bResults = WinHttpReceiveResponse(hRequest, NULL);
        if (bResults) {
               }
        else {
                }
    }
    else {
          }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);

        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

      
        if (dwStatusCode == 200) {
            DWORD dwDownloaded = 0;
            char buffer[8192];

            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    if (dwSize == 0) break;

                    DWORD dwToRead = dwSize < sizeof(buffer) ? dwSize : sizeof(buffer);
                    if (WinHttpReadData(hRequest, buffer, dwToRead, &dwDownloaded)) {
                        if (dwDownloaded == 0) break;

                        size_t oldSize = result.size();
                        result.resize(oldSize + dwDownloaded);
                        memcpy(result.data() + oldSize, buffer, dwDownloaded);
                    }
                }
            } while (dwSize > 0);

                  }
        else {
                  }
    }
    else {
       // std::cout << "Request failed for image download. Error: " << GetLastError() << std::endl;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

std::vector<uint8_t> AvatarManager::downloadAvatarSync(const std::string& userId) {
    std::vector<uint8_t> result;
   // std::cout << "Starting download for user " << userId << std::endl;

    HINTERNET hSession = WinHttpOpen(L"RobloxOverlay/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
              return result;
    }

   // std::cout << "WinHTTP session created for user " << userId << std::endl;

    WinHttpSetTimeouts(hSession, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT, DOWNLOAD_TIMEOUT);

    HINTERNET hConnect = WinHttpConnect(hSession, L"thumbnails.roblox.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (!hConnect) {
             WinHttpCloseHandle(hSession);
        return result;
    }

  
    std::string path = "/v1/users/avatar-headshot?userIds=" + userId + "&size=420x420&format=Png&isCircular=false";
    std::wstring wpath(path.begin(), path.end());
  
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
       // std::cout << "Failed to open HTTP request for user " << userId << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);

        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

        if (dwStatusCode == 429) {
                   last_rate_limit_time = std::chrono::steady_clock::now();
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        if (dwStatusCode == 200) {
            DWORD dwDownloaded = 0;
            char buffer[8192];

            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    if (dwSize == 0) break;

                    DWORD dwToRead = dwSize < sizeof(buffer) ? dwSize : sizeof(buffer);
                    if (WinHttpReadData(hRequest, buffer, dwToRead, &dwDownloaded)) {
                        if (dwDownloaded == 0) break;

                        size_t oldSize = result.size();
                        result.resize(oldSize + dwDownloaded);
                        memcpy(result.data() + oldSize, buffer, dwDownloaded);
                    }
                }
            } while (dwSize > 0);

            if (result.size() > 0 && result.size() < 1000) {
                std::string response_text(result.begin(), result.end());
           
                if (response_text.find("blocked") != std::string::npos ||
                    response_text.find("Blocked") != std::string::npos ||
                    response_text.find("BLOCKED") != std::string::npos) {
               
                    {
                        std::lock_guard<std::mutex> lock(cache_mutex);
                        avatar_states[userId] = AvatarState::Blocked;
                    }

                    result.clear();
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return result;
                }

                if (response_text.find("\"data\"") != std::string::npos) {
                    size_t imageUrl_start = response_text.find("\"imageUrl\":\"");
                    if (imageUrl_start != std::string::npos) {
                        imageUrl_start += 12;
                        size_t imageUrl_end = response_text.find("\"", imageUrl_start);
                        if (imageUrl_end != std::string::npos) {
                            std::string imageUrl = response_text.substr(imageUrl_start, imageUrl_end - imageUrl_start);
                  
                            result.clear();

                            size_t host_start = imageUrl.find("://") + 3;
                            size_t path_start = imageUrl.find("/", host_start);

                            std::string host = imageUrl.substr(host_start, path_start - host_start);
                            std::string image_path = imageUrl.substr(path_start);

                            result = downloadImageFromUrl(host, image_path);
                        }
                    }
                }
            }
            else {
                                      }
        }
        else {
                 }
    }
    else {
          }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

ID3D11ShaderResourceView* AvatarManager::createTextureFromBytes(const std::vector<uint8_t>& imageData, int& width, int& height) {
    if (imageData.empty() || !d3d_device) {
        return nullptr;
    }

    int channels;
    unsigned char* pixels = stbi_load_from_memory(imageData.data(), static_cast<int>(imageData.size()),
        &width, &height, &channels, 4);

    if (!pixels) {
            return nullptr;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = pixels;
    subResource.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = d3d_device->CreateTexture2D(&desc, &subResource, &texture);

    stbi_image_free(pixels);

    if (FAILED(hr)) {
           return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = d3d_device->CreateShaderResourceView(texture, &srvDesc, &srv);

    texture->Release();

    if (FAILED(hr)) {
           return nullptr;
    }

    return srv;
}

void AvatarManager::processCompletedDownloads() {
    std::lock_guard<std::mutex> download_lock(download_mutex);

    int processed = 0;
    for (auto it = pending_downloads.begin(); it != pending_downloads.end() && processed < 2;) {
        if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string userId = it->first;

            try {
                auto imageData = it->second.get();

                std::lock_guard<std::mutex> cache_lock(cache_mutex);

                if (avatar_states[userId] == AvatarState::Blocked) {
                    it = pending_downloads.erase(it);
                    processed++;
                    continue;
                }

                if (!imageData.empty()) {
                    image_cache[userId] = imageData;

                    int width, height;
                    ID3D11ShaderResourceView* srv = createTextureFromBytes(imageData, width, height);

                    if (srv) {
                        AvatarTexture texture;
                        texture.srv = srv;
                        texture.width = width;
                        texture.height = height;
                        texture.loaded = true;
                        texture.last_accessed = std::chrono::steady_clock::now();

                        texture_cache[userId] = texture;
                        avatar_states[userId] = AvatarState::Ready;

                                        }
                    else {
                        avatar_states[userId] = AvatarState::Failed;
                    }
                }
                else {
                    avatar_states[userId] = AvatarState::Failed;
                }

            }
            catch (const std::exception& e) {
                std::lock_guard<std::mutex> cache_lock(cache_mutex);
                avatar_states[userId] = AvatarState::Failed;
                       }

            it = pending_downloads.erase(it);
            processed++;
        }
        else {
            ++it;
        }
    }
}

void AvatarManager::cleanupOldTextures() {
    if (texture_cache.size() <= MAX_CACHE_SIZE) {
        return;
    }

    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> access_times;
    for (const auto& pair : texture_cache) {
        access_times.emplace_back(pair.first, pair.second.last_accessed);
    }

    std::sort(access_times.begin(), access_times.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    size_t to_remove = texture_cache.size() - MAX_CACHE_SIZE + 10;
    for (size_t i = 0; i < to_remove && i < access_times.size(); ++i) {
        const std::string& userId = access_times[i].first;

        auto it = texture_cache.find(userId);
        if (it != texture_cache.end()) {
            if (it->second.srv) {
                it->second.srv->Release();
            }
            texture_cache.erase(it);
        }

        image_cache.erase(userId);
        avatar_states.erase(userId);
    }
}

void AvatarManager::update() {
    processCompletedDownloads();

    static int cleanup_counter = 0;
    if (++cleanup_counter >= 100) {
        cleanup_counter = 0;
        std::lock_guard<std::mutex> lock(cache_mutex);
        cleanupOldTextures();
    }
}

ImTextureID AvatarManager::getAvatarTexture(uint64_t userId) {
    std::string userIdStr = std::to_string(userId);
    return getAvatarTexture(userIdStr);
}

ImTextureID AvatarManager::getAvatarTexture(const std::string& userId) {
    if (userId.empty()) return nullptr;

    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = texture_cache.find(userId);
    if (it != texture_cache.end() && it->second.loaded) {
        it->second.last_accessed = std::chrono::steady_clock::now();
        return (ImTextureID)(intptr_t)it->second.srv;
    }

    return nullptr;
}

AvatarState AvatarManager::getAvatarState(const std::string& userId) {
    if (userId.empty()) return AvatarState::NotRequested;

    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = avatar_states.find(userId);
    if (it != avatar_states.end()) {
        return it->second;
    }

    return AvatarState::NotRequested;
}

void AvatarManager::clearCache() {
    std::lock_guard<std::mutex> cache_lock(cache_mutex);
    std::lock_guard<std::mutex> download_lock(download_mutex);
    pending_downloads.clear();
    for (auto& pair : texture_cache) {
        if (pair.second.srv) {
            pair.second.srv->Release();
        }
    }

    texture_cache.clear();
    image_cache.clear();
    avatar_states.clear();
}

void AvatarManager::renderAvatar(uint64_t userId, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1) {
    std::string userIdStr = std::to_string(userId);
    renderAvatar(userIdStr, size, uv0, uv1);
}

void AvatarManager::renderAvatar(const std::string& userId, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1) {
    if (userId.empty()) {
        ImGui::Button("No User", size);
        return;
    }

    auto texture = getAvatarTexture(userId);
    auto state = getAvatarState(userId);

    if (texture) {
        ImGui::Image(texture, size, uv0, uv1);
    }
    else {
        switch (state) {
        case AvatarState::NotRequested:
            ImGui::Button("Click to Load", size);
            if (ImGui::IsItemClicked()) {
                requestAvatar(userId);
            }
            break;
        case AvatarState::Downloading:
            ImGui::Button("Loading...", size);
            break;
        case AvatarState::Blocked:
            ImGui::Button("Blocked", size);
            break;
        case AvatarState::Failed:
            ImGui::Button("Failed", size);
            if (ImGui::IsItemClicked()) {
                std::lock_guard<std::mutex> lock(cache_mutex);
                avatar_states[userId] = AvatarState::NotRequested;
                requestAvatar(userId);
            }
            break;
        default:
            ImGui::Button("Unknown", size);
            break;
        }
    }
}