#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"

#include "../imgui/TextEditor.h"
#include <dwmapi.h>

#include <filesystem>
#include <thread>
#include <bitset>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#ifdef min
#undef min
#endif
#include <stack>
#include "../../util/notification/notification.h"
#ifdef max
#undef max
#endif
#include <Psapi.h>
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/misc/freetype/imgui_freetype.h"
#include "../imgui/addons/imgui_addons.h"
#include <dwmapi.h>
#include <d3dx11.h>
#include "../../util/globals.h"
#include "keybind/keybind.h"
#include "../../features/visuals/visuals.h"
#include "../../util/config/configsystem.h"
#include "../../features/combat/modules/dahood/autostuff/auto.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winhttp.lib")


bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

bool fullsc(HWND windowHandle);

void movewindow(HWND hw);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


static ConfigSystem g_config_system;

bool isAutoFunctionActivez() {
    return globals::bools::bring || globals::bools::kill || globals::bools::autokill;
}

bool shouldTargetHudBeActive() {
    return (globals::focused && globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) ||
        (globals::focused && globals::combat::aimbot && globals::combat::aimbotkeybind.enabled) ||
        isAutoFunctionActivez();
}

void render_target_hud() {
    if (!globals::misc::targethud) return;
    if (!shouldTargetHudBeActive() && !overlay::visible) return;

    roblox::player target;
    bool hasTarget = false;

    if (overlay::visible) {
        target = globals::instances::lp;
        hasTarget = true;
    }
    else {
        if (globals::instances::cachedtarget.head.address != 0) {
            target = globals::instances::cachedtarget;
            hasTarget = true;
        }
        else if (globals::instances::cachedlasttarget.head.address != 0) {
            target = globals::instances::cachedlasttarget;
            hasTarget = true;
        }
    }

    if (!hasTarget || target.name.empty()) return;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    static ImVec2 targethudpos = ImVec2(GetSystemMetrics(SM_CXSCREEN) / 2 - 90, GetSystemMetrics(SM_CYSCREEN) / 2 + 120);
    static bool first_time = true;
    static bool isDragging = false;
    static ImVec2 dragDelta;
    static float animatedHealth = 100.0f;
    static int lastHealth = 100;
    static float animationTimer = 0.0f;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || !overlay::visible) {
        ImGui::SetNextWindowPos(targethudpos, ImGuiCond_Always);
        first_time = false;
    }

    int health = target.humanoid.read_health();
    int maxHealth = target.humanoid.read_maxhealth();

    if (maxHealth <= 0) maxHealth = 100;
    if (health < 0) health = 0;

    if (lastHealth != health) {
        if (health < lastHealth) {
            animationTimer = 1.0f;
        }
        lastHealth = health;
    }

    float targetHealthPercentage = std::clamp(static_cast<float>(health) / maxHealth, 0.0f, 1.0f);
    float currentHealthPercentage = std::clamp(animatedHealth / maxHealth, 0.0f, 1.0f);

    if (animationTimer > 0.0f) {
        animationTimer = std::max(0.0f, animationTimer - ImGui::GetIO().DeltaTime);
    }

    if (std::abs(currentHealthPercentage - targetHealthPercentage) > 0.001f) {
        float animationSpeed = 1.0f * ImGui::GetIO().DeltaTime;
        if (targetHealthPercentage < currentHealthPercentage) {
            currentHealthPercentage = std::max(targetHealthPercentage, currentHealthPercentage - animationSpeed);
        }
        else {
            currentHealthPercentage = std::min(targetHealthPercentage, currentHealthPercentage + animationSpeed);
        }
        animatedHealth = currentHealthPercentage * maxHealth;
    }
    else {
        currentHealthPercentage = targetHealthPercentage;
    }

    const float PADDING = 5;
    float totalHeight = 50;
    ImVec2 windowSize(180, totalHeight);

    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin("TargetHUD", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();

    if (overlay::visible) {
        targethudpos = window_pos;
    }

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    ImRect headerRect(targethudpos, targethudpos + windowSize);
    ImU32 accent_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

    ImU32 healthBarColor = accent_color;
    if (animationTimer > 0.0f && health < lastHealth) {
        float flashIntensity = std::min(1.0f, animationTimer * 2.0f);
        healthBarColor = IM_COL32(
            static_cast<int>(105 + (150 * flashIntensity)),
            static_cast<int>(0 + (50 * flashIntensity)),
            static_cast<int>(255 - (100 * flashIntensity)),
            255
        );
    }

    if (ImGui::IsMouseClicked(0) && headerRect.Contains(mousePos) && overlay::visible) {
        isDragging = true;
        dragDelta = mousePos - targethudpos;
    }
    if (isDragging && ImGui::IsMouseDown(0)) {
        targethudpos = mousePos - dragDelta;
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        targethudpos.x = ImClamp(targethudpos.x, 0.0f, screenSize.x - windowSize.x);
        targethudpos.y = ImClamp(targethudpos.y, 0.0f, screenSize.y - totalHeight);
    }
    else {
        isDragging = false;
    }

    draw->AddRectFilled(
        targethudpos,
        targethudpos + windowSize,
        IM_COL32(47, 48, 46, 255),
        0.0f
    );
    draw->AddRectFilled(
        targethudpos + ImVec2(2, 2),
        targethudpos + windowSize - ImVec2(2, 2),
        IM_COL32(21, 24, 21, 255),
        0.0f
    );
    draw->AddRectFilled(
        targethudpos + ImVec2(2, 2),
        targethudpos + ImVec2(windowSize.x - 2, 4),
        accent_color,
        0.0f
    );

    int barwidth = static_cast<int>(windowSize.x - 60);
    int healthbarwidth = static_cast<int>(currentHealthPercentage * barwidth);

    ImVec2 healthBarBg_start = ImVec2(targethudpos.x + 55, targethudpos.y + 35);
    ImVec2 healthBarBg_end = ImVec2(targethudpos.x + 55 + barwidth, targethudpos.y + 39);

    draw->AddRectFilled(
        healthBarBg_start,
        healthBarBg_end,
        IM_COL32(60, 60, 60, 255),
        1.0f
    );

    if (healthbarwidth > 0) {
        draw->AddRectFilled(
            healthBarBg_start,
            ImVec2(targethudpos.x + 55 + healthbarwidth, targethudpos.y + 39),
            healthBarColor,
            1.0f
        );
    }

    draw->AddRect(
        healthBarBg_start,
        healthBarBg_end,
        IM_COL32(0, 0, 0, 255),
        1.0f
    );

    std::string healthText = std::to_string(health) + " / " + std::to_string(maxHealth);
    ImVec2 textSize = ImGui::CalcTextSize(healthText.c_str());

    float textX = targethudpos.x + 55 + (barwidth - textSize.x) / 2;
    float textY = targethudpos.y + 35 + (4 - textSize.y) / 2;

    auto* avatar_mgr = overlay::get_avatar_manager();
    if (avatar_mgr) {
        ImTextureID avatar_texture = avatar_mgr->getAvatarTexture(target.userid.address);

        if (avatar_texture) {
            draw->AddImage(
                avatar_texture,
                targethudpos + ImVec2(5, 5),
                targethudpos + ImVec2(45, 45)
            );
        }
        else {
            draw->AddRectFilled(
                targethudpos + ImVec2(5, 5),
                targethudpos + ImVec2(45, 45),
                IM_COL32(40, 40, 40, 255),
                2.0f
            );
            draw->AddText(
                targethudpos + ImVec2(25 - ImGui::CalcTextSize("IMG").x / 2, 25 - ImGui::CalcTextSize("IMG").y / 2),
                IM_COL32(120, 120, 120, 255),
                "IMG"
            );
        }
    }
    else {
        draw->AddRectFilled(
            targethudpos + ImVec2(5, 5),
            targethudpos + ImVec2(45, 45),
            IM_COL32(40, 40, 40, 255),
            2.0f
        );
        draw->AddText(
            targethudpos + ImVec2(25 - ImGui::CalcTextSize("IMG").x / 2, 25 - ImGui::CalcTextSize("IMG").y / 2),
            IM_COL32(120, 120, 120, 255),
            "IMG"
        );
    }

    draw->AddText(
        ImVec2(textX + 1, textY + 1),
        IM_COL32(0, 0, 0, 180),
        healthText.c_str()
    );

    draw->AddText(
        ImVec2(textX, textY),
        IM_COL32(255, 255, 255, 255),
        healthText.c_str()
    );

    std::string display_name = target.name.length() > 16 ? target.name.substr(0, 13) + "..." : target.name;
    draw->AddText(
        ImVec2(targethudpos.x + 55, targethudpos.y + 8),
        IM_COL32(255, 255, 255, 255),
        display_name.c_str()
    );

    std::string tool_name = "No Tool";
    try {
        auto tool = target.instance.read_service("Tool");
        if (tool.address) {
            std::string tool_str = tool.get_name();
            if (!tool_str.empty() && tool_str != "nil") {
                tool_name = tool_str.length() > 15 ? tool_str.substr(0, 12) + "..." : tool_str;
            }
        }
    }
    catch (...) {
        tool_name = "Unknown";
    }

    draw->AddText(
        ImVec2(targethudpos.x + 55, targethudpos.y + 21),
        IM_COL32(255, 255, 255, 255),
        tool_name.c_str()
    );

    style.WindowRounding = rounded;
    ImGui::End();
}
void render_explorer() {
    if (!globals::misc::explorer) return;

    static roblox::instance selectedPart;
    static std::unordered_set<uint64_t> spectatedParts;
    static std::unordered_map<uint64_t, std::vector<roblox::instance>> nodeCache;
    static std::unordered_map<uint64_t, bool> nodeExpandedCache;
    static std::unordered_map<uint64_t, std::string> nodeNameCache;
    static std::unordered_map<uint64_t, std::string> nodeClassNameCache;
    static std::unordered_map<uint64_t, std::string> nodeTeamCache;
    static std::unordered_map<uint64_t, std::string> nodeUniqueIdCache;
    static std::unordered_map<uint64_t, std::string> nodeModel;
    static char searchQuery[128] = "";
    static std::vector<roblox::instance> searchResults;
    static bool showSearchResults = false;
    static bool isCacheInitialized = false;
    static auto lastCacheRefresh = std::chrono::steady_clock::now();
    static std::unordered_map<uint64_t, std::string> nodePath;
    static bool showProperties = true;
    static int selectedTab = 0;
    static float splitRatio = 0.65f;
    static ImVec2 explorer_pos = ImVec2(GetSystemMetrics(SM_CXSCREEN) - 680, 80);
    static bool first_time = true;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || !overlay::visible) {
        ImGui::SetNextWindowPos(explorer_pos, ImGuiCond_Always);
        first_time = false;
    }

    static std::unordered_map<std::string, std::string> classPrefixes = {
        {"Workspace", "[WS] "},
        {"Players", "[P] "},
        {"Folder", "[F] "},
        {"Part", "[PT] "},
        {"BasePart", "[BP] "},
        {"Script", "[S] "},
        {"LocalScript", "[LS] "},
        {"ModuleScript", "[MS] "}
    };

    auto cacheNode = [&](roblox::instance& node) {
        if (nodeCache.find(node.address) == nodeCache.end()) {
            nodeCache[node.address] = node.get_children();
            nodeNameCache[node.address] = node.get_name();
            nodeClassNameCache[node.address] = node.get_class_name();
            nodeUniqueIdCache[node.address] = std::to_string(node.address);

            std::string path = node.get_name();
            roblox::instance parent = node.read_parent();
            while (parent.address != 0) {
                if (nodeCache.find(parent.address) == nodeCache.end()) {
                    nodeCache[parent.address] = parent.get_children();
                    nodeNameCache[parent.address] = parent.get_name();
                }
                std::string parentName = nodeNameCache[parent.address];
                if (!parentName.empty()) {
                    path = parentName + "." + path;
                }
                parent = parent.read_parent();
            }
            nodePath[node.address] = path;
        }
        };

    try {
        auto& datamodel = globals::instances::datamodel;
        roblox::instance root_instance(datamodel.address);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheRefresh).count() >= 2) {
            nodeCache.clear();
            nodeNameCache.clear();
            nodeClassNameCache.clear();
            nodeUniqueIdCache.clear();
            isCacheInitialized = false;
            lastCacheRefresh = now;
        }

        if (!isCacheInitialized) {
            cacheNode(root_instance);
            isCacheInitialized = true;
        }

        float content_width = 650.0f;
        float padding = 8.0f;
        float header_height = 30.0f;
        float total_height = 700.0f;

        ImGui::SetNextWindowSize(ImVec2(content_width + (padding * 2), total_height), ImGuiCond_Always);
        ImGui::Begin("Explorer", nullptr, window_flags);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();

        if (overlay::visible) {
            explorer_pos = window_pos;
        }

        ImU32 text_color = IM_COL32(255, 255, 255, 255);
        ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

        draw->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 2), top_line_color, 0.0f);
        draw->AddText(ImVec2(window_pos.x + padding, window_pos.y + 8), text_color, "Explorer");

        ImGui::SetCursorPos(ImVec2(padding, header_height));

        ImGui::PushItemWidth(content_width - 100);
        bool searchChanged = ImGui::InputTextWithHint("##Search", "Search...", searchQuery, IM_ARRAYSIZE(searchQuery), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        if (searchChanged) {
            searchResults.clear();
            showSearchResults = strlen(searchQuery) > 0;

            if (showSearchResults && strlen(searchQuery) >= 1) {
                std::string query = searchQuery;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                std::function<void(roblox::instance&)> searchInstance = [&](roblox::instance& instance) {
                    if (searchResults.size() >= 100) return;

                    cacheNode(instance);

                    std::string name = nodeNameCache[instance.address];
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                    if (name.find(query) != std::string::npos) {
                        searchResults.push_back(instance);
                    }

                    for (auto& child : instance.get_children()) {
                        searchInstance(child);
                    }
                    };

                if (auto workspace = root_instance.findfirstchild("Workspace"); workspace.address != 0) {
                    searchInstance(workspace);
                }

                if (globals::instances::players.address != 0) {
                    for (auto& player : globals::instances::players.get_children()) {
                        searchInstance(player);
                    }
                }

                if (auto teams = root_instance.findfirstchild("Teams"); teams.address != 0) {
                    searchInstance(teams);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(70, 0))) {
            nodeCache.clear();
            nodeNameCache.clear();
            nodeClassNameCache.clear();
            nodeUniqueIdCache.clear();
            isCacheInitialized = false;
            searchResults.clear();
            showSearchResults = false;
            memset(searchQuery, 0, sizeof(searchQuery));
        }

        float treeHeight = 400.0f;
        ImGui::BeginChild("ExplorerTree", ImVec2(content_width, treeHeight), true);

        if (showSearchResults && strlen(searchQuery) > 0) {
            ImGui::Text("Search Results (%d):", static_cast<int>(searchResults.size()));
            ImGui::Separator();

            if (!searchResults.empty()) {
                for (auto& node : searchResults) {
                    if (!node.address) continue;

                    if (nodeCache.find(node.address) == nodeCache.end()) {
                        cacheNode(node);
                    }

                    ImGui::PushID(nodeUniqueIdCache[node.address].c_str());

                    std::string displayText = nodeNameCache[node.address];
                    std::string className = nodeClassNameCache[node.address];
                    std::string fullText = displayText + " [" + className + "]";

                    bool is_selected = (selectedPart.address == node.address);

                    if (ImGui::Selectable(fullText.c_str(), is_selected)) {
                        selectedPart = node;
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        ImGui::OpenPopup("NodeContextMenu");
                        selectedPart = node;
                    }

                    if (ImGui::BeginPopup("NodeContextMenu")) {
                        ImGui::Text("Operations for: %s", displayText.c_str());
                        ImGui::Separator();

                        if (ImGui::MenuItem("Copy Address")) {
                            ImGui::SetClipboardText(nodeUniqueIdCache[node.address].c_str());
                        }

                        if (ImGui::MenuItem("Set Collision True")) {
                            selectedPart.write_cancollide(true);
                        }

                        if (ImGui::MenuItem("Set Collision False")) {
                            selectedPart.write_cancollide(false);
                        }

                        if (ImGui::MenuItem("Teleport To Part")) {
                            Vector3 partPosition = node.get_pos();
                            const float verticalOffset = 5.0f;
                            partPosition.y += verticalOffset;
                            globals::instances::lp.hrp.write_position(partPosition);
                        }

                        if (spectatedParts.count(node.address)) {
                            if (ImGui::MenuItem("Stop Spectating")) {
                                selectedPart.unspectate();
                                spectatedParts.erase(node.address);
                            }
                        }
                        else {
                            if (ImGui::MenuItem("Spectate Part")) {
                                selectedPart = node;
                                selectedPart.spectate(node.address);
                                spectatedParts.insert(node.address);
                            }
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }
            else {
                ImGui::Text("No results found");
            }
        }
        else {
            for (auto& child : nodeCache[root_instance.address]) {
                std::stack<std::pair<roblox::instance, int>> stack;
                stack.push({ child, 0 });

                while (!stack.empty()) {
                    auto [node, indentLevel] = stack.top();
                    stack.pop();

                    cacheNode(node);

                    ImGui::SetCursorPosX(20.0f * indentLevel);
                    ImGui::PushID(nodeUniqueIdCache[node.address].c_str());

                    const std::vector<roblox::instance>& children = nodeCache[node.address];
                    bool hasChildren = !children.empty();

                    std::string className = nodeClassNameCache[node.address];
                    std::string prefix = "";

                    if (classPrefixes.find(className) != classPrefixes.end()) {
                        prefix = classPrefixes[className];
                    }

                    std::string displayText = prefix + nodeNameCache[node.address] + " [" + className + "]";

                    ImGuiTreeNodeFlags flags = hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;
                    flags |= ImGuiTreeNodeFlags_OpenOnArrow;

                    if (selectedPart.address == node.address) {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    bool isExpanded = ImGui::TreeNodeEx(displayText.c_str(), flags);

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        selectedPart = node;
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        ImGui::OpenPopup("NodeContextMenu");
                        selectedPart = node;
                    }

                    if (ImGui::BeginPopup("NodeContextMenu")) {
                        ImGui::Text("%s", nodeNameCache[node.address].c_str());
                        ImGui::Separator();

                        if (ImGui::MenuItem("Copy Address")) {
                            ImGui::SetClipboardText(nodeUniqueIdCache[node.address].c_str());
                        }

                        if (ImGui::MenuItem("Set Collision True")) {
                            selectedPart.write_cancollide(true);
                        }

                        if (ImGui::MenuItem("Set Collision False")) {
                            selectedPart.write_cancollide(false);
                        }

                        if (ImGui::MenuItem("Teleport To Part")) {
                            Vector3 partPosition = node.get_pos();
                            const float verticalOffset = 5.0f;
                            partPosition.y += verticalOffset;
                            globals::instances::lp.hrp.write_position(partPosition);
                        }

                        if (spectatedParts.count(node.address)) {
                            if (ImGui::MenuItem("Stop Spectating")) {
                                selectedPart.unspectate();
                                spectatedParts.erase(node.address);
                            }
                        }
                        else {
                            if (ImGui::MenuItem("Spectate Part")) {
                                selectedPart = node;
                                selectedPart.spectate(node.address);
                                spectatedParts.insert(node.address);
                            }
                        }

                        ImGui::EndPopup();
                    }

                    if (isExpanded) {
                        for (auto it = children.rbegin(); it != children.rend(); ++it) {
                            stack.push({ *it, indentLevel + 1 });
                        }
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }
        }

        ImGui::EndChild();

        ImGui::BeginChild("Properties", ImVec2(content_width, 240), true);

        ImGui::Text("Properties");
        ImGui::Separator();

        if (selectedPart.address != 0) {
            if (ImGui::BeginTabBar("PropertiesTabBar")) {
                if (ImGui::BeginTabItem("Basic")) {
                    ImGui::BeginChild("BasicScrollRegion", ImVec2(0, 160), false);

                    const std::string& nodeName = nodeNameCache[selectedPart.address];
                    const std::string& className = nodeClassNameCache[selectedPart.address];
                    roblox::instance parent = selectedPart.read_parent();
                    std::string parentName = nodeNameCache[parent.address];

                    float col1Width = 120.0f;

                    ImGui::Text("Path:");
                    ImGui::SameLine(col1Width);
                    ImGui::TextWrapped("%s", nodePath[selectedPart.address].c_str());

                    ImGui::Text("Name:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", nodeName.c_str());

                    ImGui::Text("Class:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", className.c_str());

                    ImGui::Text("Parent:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", parentName.c_str());

                    ImGui::Text("Address:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("0x%llX", selectedPart.address);

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Transform")) {
                    ImGui::BeginChild("TransformScrollRegion", ImVec2(0, 160), false);

                    float col1Width = 120.0f;

                    Vector3 position = selectedPart.get_pos();
                    ImGui::Text("Position:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", position.x, position.y, position.z);

                    Vector3 size = selectedPart.get_part_size();
                    ImGui::Text("Size:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", size.x, size.y, size.z);

                    ImGui::Separator();
                    ImGui::Text("Quick Actions:");

                    if (ImGui::Button("Teleport To", ImVec2(120, 25))) {
                        Vector3 partPosition = selectedPart.get_pos();
                        const float verticalOffset = 5.0f;
                        partPosition.y += verticalOffset;
                        globals::instances::lp.hrp.write_position(partPosition);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Toggle Collide", ImVec2(120, 25))) {
                        bool currentState = selectedPart.get_cancollide();
                        selectedPart.write_cancollide(!currentState);
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        else {
            ImGui::Text("No object selected");
        }

        ImGui::EndChild();

        ImGui::Separator();

        int totalNodes = nodeCache.size();
        int spectatingCount = spectatedParts.size();

        ImGui::Text("Nodes: %d | Spectating: %d", totalNodes, spectatingCount);

        ImGui::End();
        style.WindowRounding = rounded;
    }
    catch (const std::exception& e) {
        ImGui::Text("Error: %s", e.what());
    }
    catch (...) {
        ImGui::Text("Unknown error occurred");
    }
}


void overlay::initialize_avatar_system() {
    if (g_pd3dDevice && !avatar_manager) {
        avatar_manager = std::make_unique<AvatarManager>(g_pd3dDevice, g_pd3dDeviceContext);

    }
}

void overlay::update_avatars() {
    if (avatar_manager) {
        avatar_manager->update();
    }
}

AvatarManager* overlay::get_avatar_manager() {
    return avatar_manager.get();
}

void overlay::cleanup_avatar_system() {
    if (avatar_manager) {
               avatar_manager.reset();
    }
}


void render_player_list() {
    if (!globals::misc::playerlist) return;

    if (!overlay::get_avatar_manager()) {
        overlay::initialize_avatar_system();
    }

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    static ImVec2 playerlist_pos = ImVec2(500, 10);
    static bool first_time = true;
    static int selected_player = -1;
    static float side_panel_animation = 0.0f;
    static std::vector<std::string> status_options = { "Enemy", "Friendly", "Neutral", "Client" };
    static std::vector<int> player_status;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || !overlay::visible) {
        ImGui::SetNextWindowPos(playerlist_pos, ImGuiCond_Always);
        first_time = false;
    }

    std::vector<roblox::player> players;
    if (globals::instances::cachedplayers.size() > 0) {
        players = globals::instances::cachedplayers;
    }

    if (player_status.size() != players.size()) {
        player_status.resize(players.size(), 2);

        for (size_t i = 0; i < players.size(); i++) {
            auto& player = players[i];

            if (player.name == globals::instances::lp.name) {
                player_status[i] = 3;
            }
            else if (std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), player.name) != globals::instances::whitelist.end()) {
                player_status[i] = 1;
            }
            else if (std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), player.name) != globals::instances::blacklist.end()) {
                player_status[i] = 0;
            }
        }
    }

    auto isWhitelisted = [](const std::string& name) {
        return std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), name) != globals::instances::whitelist.end();
        };

    auto isBlacklisted = [](const std::string& name) {
        return std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), name) != globals::instances::blacklist.end();
        };

    auto addToWhitelist = [](const std::string& name) {
        if (std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), name) == globals::instances::whitelist.end()) {
            globals::instances::whitelist.push_back(name);
        }
        auto it = std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), name);
        if (it != globals::instances::blacklist.end()) {
            globals::instances::blacklist.erase(it);
        }
        };

    auto addToBlacklist = [](const std::string& name) {
        if (std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), name) == globals::instances::blacklist.end()) {
            globals::instances::blacklist.push_back(name);
        }
        auto it = std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), name);
        if (it != globals::instances::whitelist.end()) {
            globals::instances::whitelist.erase(it);
        }
        };

    auto removeFromLists = [](const std::string& name) {
        auto it = std::find(globals::instances::whitelist.begin(), globals::instances::whitelist.end(), name);
        if (it != globals::instances::whitelist.end()) {
            globals::instances::whitelist.erase(it);
        }
        it = std::find(globals::instances::blacklist.begin(), globals::instances::blacklist.end(), name);
        if (it != globals::instances::blacklist.end()) {
            globals::instances::blacklist.erase(it);
        }
        };

    float content_width = 300.0f;
    float padding = 8.0f;
    float header_height = 30.0f;
    float player_item_height = 45.0f;
    float max_height = 500.0f;
    float side_panel_width = 350.0f;

    float target_animation = (selected_player >= 0) ? 1.0f : 0.0f;
    side_panel_animation += (target_animation - side_panel_animation) * 0.15f;
    float animated_side_width = side_panel_width * side_panel_animation;

    float total_width = content_width + (padding * 2);
    float total_height = header_height + max_height + padding;

    ImGui::SetNextWindowSize(ImVec2(total_width + animated_side_width, total_height), ImGuiCond_Always);
    ImGui::Begin("PlayerList", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();

    if (overlay::visible) {
        playerlist_pos = window_pos;
    }
    auto* avatar_mgr = overlay::get_avatar_manager();
    ImU32 text_color = IM_COL32(255, 255, 255, 255);
    ImU32 enemy_color = IM_COL32(255, 100, 100, 255);
    ImU32 friendly_color = IM_COL32(100, 255, 100, 255);
    ImU32 neutral_color = IM_COL32(200, 200, 200, 255);
    ImU32 client_color = IM_COL32(100, 150, 255, 255);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

    draw->AddRectFilled(window_pos, ImVec2(window_pos.x + total_width, window_pos.y + 2), top_line_color, 0.0f);
    draw->AddText(ImVec2(window_pos.x + padding, window_pos.y + 8), text_color, "Players");

    ImGui::SetCursorPos(ImVec2(padding, header_height));

    if (ImGui::BeginChild("PlayerList", ImVec2(total_width - padding * 2, max_height), true)) {
        if (players.empty()) {
            ImGui::Text("No players found");
        }
        else {
            for (size_t i = 0; i < players.size(); i++) {
                auto& player = players[i];
                ImGui::PushID(static_cast<int>(i));

                bool is_selected = (selected_player == static_cast<int>(i));
                bool is_client = (player.name == globals::instances::lp.name);

                if (is_selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.41f, 0.0f, 1.0f, 0.4f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.41f, 0.0f, 1.0f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.41f, 0.0f, 1.0f, 0.6f));
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.2f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

                if (ImGui::Button("##player", ImVec2(-1, player_item_height))) {
                    if (!is_client) {
                        selected_player = (selected_player == static_cast<int>(i)) ? -1 : static_cast<int>(i);
                    }
                }

                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(3);

                ImVec2 button_min = ImGui::GetItemRectMin();
                ImDrawList* window_draw_list = ImGui::GetWindowDrawList();

                ImVec2 avatar_start = ImVec2(button_min.x + 3, button_min.y + 3);
                ImVec2 avatar_end = ImVec2(avatar_start.x + 39, avatar_start.y + 39);

                ImTextureID avatar_texture = avatar_mgr->getAvatarTexture(player.userid.address);
                ImVec2 avatar_size2 = ImVec2(36, 36);

                window_draw_list->AddRectFilled(avatar_start, avatar_end, IM_COL32(40, 40, 40, 255), 4.0f);
                window_draw_list->AddRect(avatar_start, avatar_end, IM_COL32(80, 80, 80, 255), 4.0f);
                ImVec2 center_offset = ImVec2(
                    (avatar_end.x - avatar_start.x - avatar_size2.x) / 2.0f,
                    (avatar_end.y - avatar_start.y - avatar_size2.y) / 2.0f
                );
                ImGui::SetCursorScreenPos(avatar_start);

                ImVec2 image_pos = ImVec2(avatar_start.x + center_offset.x, avatar_start.y + center_offset.y);

                ImGui::SetCursorScreenPos(image_pos);
                if (avatar_texture) {
                    ImGui::Image(avatar_texture, avatar_size2);
                }
                else {
                    window_draw_list->AddText(ImVec2(avatar_start.x + 19.5f - ImGui::CalcTextSize("IMG").x / 2,
                        avatar_start.y + 19.5f - ImGui::CalcTextSize("IMG").y / 2), IM_COL32(120, 120, 120, 255), "IMG");
                }

                float info_x = avatar_end.x + 8;
                std::string display_name = player.name.length() > 20 ? player.name.substr(0, 17) + "..." : player.name;
                window_draw_list->AddText(ImVec2(info_x, button_min.y + 5), text_color, display_name.c_str());

                ImU32 status_color = neutral_color;
                std::string status_text = "Neutral";

                if (is_client) {
                    player_status[i] = 3;
                    status_color = client_color;
                    status_text = "Client";
                }
                else if (i < player_status.size()) {
                    switch (player_status[i]) {
                    case 0: status_color = enemy_color; status_text = "Enemy"; break;
                    case 1: status_color = friendly_color; status_text = "Friendly"; break;
                    case 2: status_color = neutral_color; status_text = "Neutral"; break;
                    case 3: status_color = client_color; status_text = "Client"; break;
                    }
                }

                window_draw_list->AddText(ImVec2(info_x, button_min.y + 17), status_color, status_text.c_str());
                window_draw_list->AddText(ImVec2(info_x, button_min.y + 29), IM_COL32(180, 180, 180, 255),
                    ("ID: " + std::to_string(player.userid.address)).c_str());

                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    static bool spectating = false;
    if (side_panel_animation > 0.01f && selected_player >= 0 && selected_player < static_cast<int>(players.size())) {
        auto& selected = players[selected_player];
        bool is_client = (selected.name == globals::instances::lp.name);

        float panel_x = window_pos.x + total_width;
        float panel_y = window_pos.y;

        draw->AddRectFilled(ImVec2(panel_x, panel_y), ImVec2(panel_x + animated_side_width, panel_y + total_height),
            IM_COL32(15, 15, 15, 200), 0.0f);
        draw->AddRect(ImVec2(panel_x, panel_y), ImVec2(panel_x + animated_side_width, panel_y + total_height),
            IM_COL32(60, 60, 60, 255), 0.0f);
        draw->AddRectFilled(ImVec2(panel_x, panel_y), ImVec2(panel_x + animated_side_width, panel_y + 2),
            top_line_color, 0.0f);

        if (side_panel_animation > 0.8f && !is_client) {
            float pad = 20.0f;
            float y = panel_y + 25.0f;

            if (avatar_mgr) {
                ImTextureID avatar_texture = avatar_mgr->getAvatarTexture(selected.userid.address);

                ImVec2 avatar_start = ImVec2(panel_x + pad, y);
                ImVec2 avatar_size = ImVec2(150, 150);

                ImGui::SetCursorScreenPos(avatar_start);

                if (avatar_texture) {
                    ImGui::Image(avatar_texture, avatar_size);
                }
                else {
                    std::string userId = std::to_string(selected.userid.address);
                    AvatarState state = avatar_mgr->getAvatarState(userId);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                    switch (state) {
                    case AvatarState::NotRequested:
                        if (ImGui::Button("Load Avatar", avatar_size)) {
                            avatar_mgr->requestAvatar(selected.userid.address);
                        }
                        break;
                    case AvatarState::Downloading:
                        ImGui::Button("Loading...", avatar_size);
                        break;
                    case AvatarState::Failed:
                        if (ImGui::Button("Retry Avatar", avatar_size)) {
                            avatar_mgr->requestAvatar(selected.userid.address);
                        }
                        break;
                    default:
                        ImGui::Button("Avatar", avatar_size);
                        break;
                    }

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }
            }
            else {
                draw->AddRectFilled(ImVec2(panel_x + pad, y), ImVec2(panel_x + pad + 150, y + 150),
                    IM_COL32(40, 40, 40, 255), 6.0f);
                draw->AddRect(ImVec2(panel_x + pad, y), ImVec2(panel_x + pad + 150, y + 150),
                    IM_COL32(80, 80, 80, 255), 6.0f);
                draw->AddText(ImVec2(panel_x + pad + 75 - ImGui::CalcTextSize("AVATAR").x / 2,
                    y + 75 - ImGui::CalcTextSize("AVATAR").y / 2),
                    IM_COL32(120, 120, 120, 255), "AVATAR");
            }

            float info_x = panel_x + pad + 150 + 15.0f;
            draw->AddText(ImVec2(info_x, y), text_color, selected.name.c_str());
            draw->AddText(ImVec2(info_x, y + 25), IM_COL32(180, 180, 180, 255), "Position:");
            Vector3 pos = selected.hrp.get_pos();

            int health = selected.humanoid.read_health();
            int maxhealth = selected.humanoid.read_maxhealth();

            auto x = std::to_string(static_cast<int>(pos.x));
            std::string yz = std::to_string(static_cast<int>(pos.y));
            auto z = std::to_string(static_cast<int>(pos.z));
            std::string posz = "X: " + x + ", Y: " + yz + ", Z: " + z;
            std::string healthz = "Health: " + std::to_string(health) + "/" + std::to_string(maxhealth);
            draw->AddText(ImVec2(info_x, y + 43), text_color, posz.c_str());
            draw->AddText(ImVec2(info_x, y + 68), text_color, healthz.c_str());

            std::string status_display = "Status: " +
                (selected_player < static_cast<int>(player_status.size()) ?
                    status_options[player_status[selected_player]] : "Neutral");
            draw->AddText(ImVec2(info_x, y + 93), text_color, status_display.c_str());

            float btn_y = y + 150 + 30;
            float btn_w = (animated_side_width - pad * 3) * 0.5f;
            float btn_h = 22.0f;
            float btn_spacing = 6.0f;

            ImVec2 mouse_pos = ImGui::GetMousePos();
            bool mouse_clicked = ImGui::IsMouseClicked(0);

            auto draw_button = [&](ImVec2 pos, ImVec2 size, const char* text, bool& clicked) {
                bool hovered = mouse_pos.x >= pos.x && mouse_pos.x <= pos.x + size.x &&
                    mouse_pos.y >= pos.y && mouse_pos.y <= pos.y + size.y;

                ImU32 btn_color = IM_COL32(23, 23, 23, 255);
                if (hovered) {
                    btn_color = IM_COL32(38, 38, 38, 255);
                    if (mouse_clicked) {
                        clicked = true;
                        btn_color = IM_COL32(51, 51, 51, 255);
                    }
                }

                draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), btn_color, 2.0f);
                draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 80, 80, 255), 2.0f);

                ImVec2 text_size = ImGui::CalcTextSize(text);
                ImVec2 text_pos = ImVec2(pos.x + size.x * 0.5f - text_size.x * 0.5f,
                    pos.y + size.y * 0.5f - text_size.y * 0.5f);
                draw->AddText(text_pos, text_color, text);
                };

            bool spectate_clicked = false, kill_clicked = false, auto_kill_clicked = false,
                bring_clicked = false, teleport_clicked = false;
            static std::string cool = "Spectate";
            if (spectating)
                cool = "Unspectate";
            else
                cool = "Spectate";

            draw_button(ImVec2(panel_x + pad, btn_y), ImVec2(btn_w, btn_h), cool.c_str(), spectate_clicked);
            draw_button(ImVec2(panel_x + pad + btn_w + pad, btn_y), ImVec2(btn_w, btn_h), "Kill", kill_clicked);

            draw_button(ImVec2(panel_x + pad, btn_y + btn_h + btn_spacing), ImVec2(btn_w, btn_h), "Auto Kill", auto_kill_clicked);
            draw_button(ImVec2(panel_x + pad + btn_w + pad, btn_y + btn_h + btn_spacing), ImVec2(btn_w, btn_h), "Bring", bring_clicked);

            draw_button(ImVec2(panel_x + pad, btn_y + (btn_h + btn_spacing) * 2),
                ImVec2(animated_side_width - pad * 2, btn_h), "Teleport", teleport_clicked);

            float combo_y = btn_y + (btn_h + btn_spacing) * 3;
            ImVec2 combo_pos = ImVec2(panel_x + pad, combo_y);
            ImVec2 combo_size = ImVec2(animated_side_width - pad * 2, btn_h);

            bool combo_hovered = mouse_pos.x >= combo_pos.x && mouse_pos.x <= combo_pos.x + combo_size.x &&
                mouse_pos.y >= combo_pos.y && mouse_pos.y <= combo_pos.y + combo_size.y;

            static bool combo_open = false;
            if (combo_hovered && mouse_clicked) {
                combo_open = !combo_open;
            }

            ImU32 combo_color = combo_hovered ? IM_COL32(38, 38, 38, 255) : IM_COL32(23, 23, 23, 255);
            draw->AddRectFilled(combo_pos, ImVec2(combo_pos.x + combo_size.x, combo_pos.y + combo_size.y), combo_color, 2.0f);
            draw->AddRect(combo_pos, ImVec2(combo_pos.x + combo_size.x, combo_pos.y + combo_size.y), IM_COL32(80, 80, 80, 255), 2.0f);

            std::string combo_text = selected_player < static_cast<int>(player_status.size()) ?
                status_options[player_status[selected_player]] : "Neutral";
            ImVec2 combo_text_pos = ImVec2(combo_pos.x + 8, combo_pos.y + combo_size.y * 0.5f - ImGui::CalcTextSize(combo_text.c_str()).y * 0.5f);
            draw->AddText(combo_text_pos, text_color, combo_text.c_str());

            draw->AddText(ImVec2(combo_pos.x + combo_size.x - 15, combo_text_pos.y), text_color, "v");

            if (combo_open) {
                float dropdown_y = combo_pos.y + combo_size.y + 2;
                for (int i = 0; i < 3; i++) {
                    ImVec2 item_pos = ImVec2(combo_pos.x, dropdown_y + i * btn_h);
                    bool item_hovered = mouse_pos.x >= item_pos.x && mouse_pos.x <= item_pos.x + combo_size.x &&
                        mouse_pos.y >= item_pos.y && mouse_pos.y <= item_pos.y + btn_h;

                    ImU32 item_color = item_hovered ? IM_COL32(38, 38, 38, 255) : IM_COL32(23, 23, 23, 255);
                    draw->AddRectFilled(item_pos, ImVec2(item_pos.x + combo_size.x, item_pos.y + btn_h), item_color, 0.0f);
                    draw->AddRect(item_pos, ImVec2(item_pos.x + combo_size.x, item_pos.y + btn_h), IM_COL32(80, 80, 80, 255), 0.0f);

                    ImVec2 item_text_pos = ImVec2(item_pos.x + 8, item_pos.y + btn_h * 0.5f - ImGui::CalcTextSize(status_options[i].c_str()).y * 0.5f);
                    draw->AddText(item_text_pos, text_color, status_options[i].c_str());

                    if (item_hovered && mouse_clicked && selected_player >= 0 && selected_player < static_cast<int>(player_status.size())) {
                        int old_status = player_status[selected_player];
                        player_status[selected_player] = i;

                        if (i == 0) {
                            addToBlacklist(selected.name);
                        }
                        else if (i == 1) {
                            addToWhitelist(selected.name);
                        }
                        else if (i == 2) {
                            removeFromLists(selected.name);
                        }

                        combo_open = false;
                    }
                }
            }

            if (mouse_clicked && !combo_hovered && combo_open) {
                combo_open = false;
            }

            if (spectate_clicked) {
                roblox::instance cam;
                if (!spectating) {
                    spectating = true;
                    cam.spectate(selected.hrp.address);
                }
                else {
                    spectating = false;
                    cam.unspectate();
                }
            }
            if (kill_clicked) {
                            globals::bools::name = selected.name;
                globals::bools::entity = selected;
                globals::bools::kill = true;
            }
            if (auto_kill_clicked) {
                               globals::bools::name = selected.name;
                globals::bools::entity = selected;
                globals::bools::autokill = true;
            }
            if (bring_clicked) {
                               globals::bools::name = selected.name;
                globals::bools::entity = selected;
                globals::bools::bring = true;
            }
            if (teleport_clicked) {
                globals::instances::lp.hrp.write_position(selected.hrp.get_pos());
            }
        }
        else if (is_client && side_panel_animation > 0.8f) {
            float pad = 20.0f;
            float info_x = panel_x + pad;
            float y = panel_y + 25.0f;

            draw->AddText(ImVec2(info_x, y), client_color, "This is you!");
            draw->AddText(ImVec2(info_x, y + 25), text_color, selected.name.c_str());
            draw->AddText(ImVec2(info_x, y + 50), IM_COL32(180, 180, 180, 255), "Status: Client");
        }
    }

    style.WindowRounding = rounded;
    ImGui::End();
}

void render_watermark() {
    if (!globals::misc::watermark) return;

    static ImVec2 watermark_pos = ImVec2(10, 10);
    static bool first_time = true;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || !overlay::visible) {
        ImGui::SetNextWindowPos(watermark_pos, ImGuiCond_Always);
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm local_time;
    localtime_s(&local_time, &time_t);

    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
    char date_str[64];
    std::strftime(date_str, sizeof(date_str), "%d-%m-%Y", &local_time);

    ImGuiIO& io = ImGui::GetIO();
    int fps = (int)(1.0f / io.DeltaTime);

    std::string watermark_text = "Centrum";
    std::vector<std::string> info_parts;

    if (first_time && globals::misc::watermarkstuff != nullptr) {
        (*globals::misc::watermarkstuff)[0] = 1;
        (*globals::misc::watermarkstuff)[1] = 1;
    }

    if (globals::misc::watermarkstuff != nullptr) {
        if ((*globals::misc::watermarkstuff)[1] == 1) {
            info_parts.push_back(globals::instances::username);
        }
        if ((*globals::misc::watermarkstuff)[2] == 1) {
            info_parts.push_back(std::string(date_str));
        }
        if ((*globals::misc::watermarkstuff)[0] == 1) {
            info_parts.push_back("FPS: " + std::to_string(fps));
        }
    }

    if (!info_parts.empty()) {
        watermark_text += " | ";
        for (size_t i = 0; i < info_parts.size(); i++) {
            if (i > 0) watermark_text += " | ";
            watermark_text += info_parts[i];
        }
    }

    ImVec2 text_size = ImGui::CalcTextSize(watermark_text.c_str());
    float padding_x = 3.0f;
    float padding_y = 3.0f;
    float total_width = text_size.x + (padding_x * 2) + 3.0f;
    float total_height = text_size.y + (padding_y * 2) + 1.0f;

    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
    ImGui::Begin("Watermark", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    if (overlay::visible) {
        watermark_pos = window_pos;
    }

    ImU32 bg_color = IM_COL32(15, 15, 15, 200);
    ImU32 text_color = IM_COL32(255, 255, 255, 255);
    ImU32 outline_color = IM_COL32(60, 60, 60, 255);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

    draw->AddRectFilled(
        window_pos,
        ImVec2(window_pos.x + window_size.x, window_pos.y + 2),
        top_line_color,
        1.0f
    );

    ImVec2 text_pos = ImVec2(
        window_pos.x + padding_x,
        window_pos.y + padding_y
    );
    draw->AddText(text_pos, text_color, watermark_text.c_str());

    if (first_time) {
        first_time = false;
    }

    style.WindowRounding = rounded;
    ImGui::End();
}

void render_keybind_list() {
    if (!globals::misc::keybinds) return;


    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    float rounded;
    rounded = style.WindowRounding;
    style.WindowRounding = 0;



    static ImVec2 keybind_pos = ImVec2(5, GetSystemMetrics(SM_CYSCREEN) / 2 - 10);
    static bool first_time = true;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || (!overlay::visible)) {
        ImGui::SetNextWindowPos(keybind_pos, ImGuiCond_Always);
        first_time = false;
    }

    std::vector<std::pair<std::string, std::string>> active_keybinds;

    if (globals::combat::aimbot && globals::combat::aimbotkeybind.enabled) {
        active_keybinds.push_back({ "Aimbot", globals::combat::aimbotkeybind.get_key_name() });
    }

    if (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) {
        active_keybinds.push_back({ "Silent Aim", globals::combat::silentaimkeybind.get_key_name() });
    }

    if (globals::combat::orbit && globals::combat::orbitkeybind.enabled) {
        active_keybinds.push_back({ "Orbit", globals::combat::orbitkeybind.get_key_name() });
    }

    if (globals::combat::triggerbot && globals::combat::triggerbotkeybind.enabled) {
        active_keybinds.push_back({ "TriggerBot", globals::combat::triggerbotkeybind.get_key_name() });
    }

    if (globals::misc::speed && globals::misc::speedkeybind.enabled) {
        active_keybinds.push_back({ "Speed", globals::misc::speedkeybind.get_key_name() });
    }

    if (globals::misc::jumppower && globals::misc::jumppowerkeybind.enabled) {
        active_keybinds.push_back({ "Jump Power", globals::misc::jumppowerkeybind.get_key_name() });
    }

    if (globals::misc::flight && globals::misc::flightkeybind.enabled) {
        active_keybinds.push_back({ "Flight", globals::misc::flightkeybind.get_key_name() });
    }

    if (globals::misc::voidhide && globals::misc::voidhidebind.enabled) {
        active_keybinds.push_back({ "Void Hide", globals::misc::voidhidebind.get_key_name() });
    }

    if (globals::misc::autostomp && globals::misc::stompkeybind.enabled) {
        active_keybinds.push_back({ "Auto Stomp", globals::misc::stompkeybind.get_key_name() });
    }

    if (globals::misc::keybindsstyle == 1) {
        struct KeybindInfo {
            std::string name;
            keybind* bind;
            bool* enabled;
        };

        std::vector<KeybindInfo> all_keybinds = {
            {"Aimbot", &globals::combat::aimbotkeybind, &globals::combat::aimbot},
            {"Silent Aim", &globals::combat::silentaimkeybind, &globals::combat::silentaim},
            {"Orbit", &globals::combat::orbitkeybind, &globals::combat::orbit},
            {"TriggerBot", &globals::combat::triggerbotkeybind, &globals::combat::triggerbot},
            {"Speed", &globals::misc::speedkeybind, &globals::misc::speed},
            {"Jump Power", &globals::misc::jumppowerkeybind, &globals::misc::jumppower},
            {"Flight", &globals::misc::flightkeybind, &globals::misc::flight},
            {"Void Hide", &globals::misc::voidhidebind, &globals::misc::voidhide},
            {"Auto Stomp", &globals::misc::stompkeybind, &globals::misc::autostomp}
        };

        active_keybinds.clear();
        for (const auto& info : all_keybinds) {
            if (*info.enabled) {
                active_keybinds.push_back({ info.name, info.bind->get_key_name() });
            }
        }
    }

    ImVec2 title_size = ImGui::CalcTextSize("Keybinds");
    float content_width = title_size.x;

    for (const auto& bind : active_keybinds) {
        std::string full_text = bind.first + ": " + bind.second;
        ImVec2 text_size = ImGui::CalcTextSize(full_text.c_str());
        content_width = std::max(content_width, text_size.x);
    }

    float padding_x = 3.0f;
    float padding_y = 3.0f;
    float line_spacing = ImGui::GetTextLineHeight() + 2.0f;

    float total_width = content_width + (padding_x * 2) + 1.0f;
    float total_height = padding_y * 2 + title_size.y + 2;

    if (!active_keybinds.empty()) {
        total_height += active_keybinds.size() * line_spacing;
    }

    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);

    ImGui::Begin("Keybinds", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    if (overlay::visible) {
        keybind_pos = window_pos;
    }

    ImU32 bg_color = IM_COL32(15, 15, 15, 200);
    ImU32 text_color = IM_COL32(255, 255, 255, 255);
    ImU32 outline_color = IM_COL32(60, 60, 60, 255);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    ImU32 active_color = IM_COL32(105, 0, 255, 255);

                        
                                
    draw->AddRectFilled(
        window_pos,
        ImVec2(window_pos.x + window_size.x, window_pos.y + 2),
        top_line_color,
        0.0f
    );

    ImVec2 title_pos = ImVec2(
        window_pos.x + padding_x,
        window_pos.y + padding_y
    );

    draw->AddText(
        title_pos,
        text_color,
        "Keybinds"
    );

    if (!active_keybinds.empty()) {
        float current_y = title_pos.y + title_size.y + 4.0f;

        std::sort(active_keybinds.begin(), active_keybinds.end(),
            [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
                std::string full_a = a.first + ": " + a.second;
                std::string full_b = b.first + ": " + b.second;
                return full_a.length() > full_b.length();
            });

        for (const auto& bind : active_keybinds) {
            std::string full_text = bind.first + ": " + bind.second;

            ImVec2 keybind_pos = ImVec2(window_pos.x + padding_x, current_y);

            if (globals::misc::keybindsstyle == 1) {
                bool is_active = false;
                if (bind.first == "Aimbot") is_active = globals::combat::aimbotkeybind.enabled;
                else if (bind.first == "Silent Aim") is_active = globals::combat::silentaimkeybind.enabled;
                else if (bind.first == "Orbit") is_active = globals::combat::orbitkeybind.enabled;
                else if (bind.first == "TriggerBot") is_active = globals::combat::triggerbotkeybind.enabled;
                else if (bind.first == "Speed") is_active = globals::misc::speedkeybind.enabled;
                else if (bind.first == "Jump Power") is_active = globals::misc::jumppowerkeybind.enabled;
                else if (bind.first == "Flight") is_active = globals::misc::flightkeybind.enabled;
                else if (bind.first == "Void Hide") is_active = globals::misc::voidhidebind.enabled;
                else if (bind.first == "Auto Stomp") is_active = globals::misc::stompkeybind.enabled;

                draw->AddText(keybind_pos, is_active ? active_color : text_color, full_text.c_str());
            }
            else {
                draw->AddText(keybind_pos, text_color, full_text.c_str());
            }

            current_y += line_spacing;
        }
    }
    style.WindowRounding = rounded;
    ImGui::End();
}



bool Bind(keybind* keybind, const ImVec2& size_arg = ImVec2(0, 0), bool clicked = false, ImGuiButtonFlags flags = 0) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(keybind->get_name().c_str());
    const ImVec2 label_size = ImGui::CalcTextSize(keybind->get_name().c_str(), NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
        style.FramePadding.y < window->DC.CurrLineTextBaseOffset)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;

    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    if (g.CurrentItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_PressedOnRelease;

    bool hovered, held;
    bool Pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

    bool value_changed = false;
    int key = keybind->key;

    auto io = ImGui::GetIO();

    std::string name = keybind->get_key_name();

    if (keybind->waiting_for_input) {
        name = "[Waiting]";
        keybind->name = name.c_str();
    }


    if (ImGui::GetIO().MouseClicked[0] && hovered)
    {
        if (g.ActiveId == id)
        {
            keybind->waiting_for_input = true;
        }
    }
    else if (ImGui::GetIO().MouseClicked[1] && hovered)
    {
        ImGui::OpenPopup(keybind->get_name().c_str());
    }
    else if (ImGui::GetIO().MouseClicked[0] && !hovered)
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }

    if (keybind->waiting_for_input)
    {
        if (ImGui::GetIO().MouseClicked[0] && !hovered)
        {
            keybind->key = VK_LBUTTON;
            ImGui::ClearActiveID();
            keybind->waiting_for_input = false;
        }
        else
        {
            if (keybind->set_key())
            {
                ImGui::ClearActiveID();
                keybind->waiting_for_input = false;
            }
        }
    }

    ImColor bgColor = ImColor(254, 208, 2);
    ImVec4 borderColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    window->DrawList->AddText(bb.Min + ImVec2(size_arg.x / 2 - ImGui::CalcTextSize(name.c_str()).x / 2, size_arg.y / 2 - ImGui::CalcTextSize(name.c_str()).y / 2), ImGui::GetColorU32(textColor), name.c_str());

    ImGuiWindowFlags window_flags =  ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::BeginPopup(keybind->get_name().c_str()))
    {
        {

            
                if (ImGui::Selectable("Hold", keybind->type == keybind::HOLD)) keybind->type = keybind::HOLD;
                if (ImGui::Selectable("Toggle", keybind->type == keybind::TOGGLE)) keybind->type = keybind::TOGGLE;
                if (ImGui::Selectable("Always", keybind->type == keybind::ALWAYS)) keybind->type = keybind::ALWAYS;

        }
        ImGui::EndPopup();

    }

    return Pressed;
}

void draw_shadowed_text(const char* label) {
 ImGuiContext& g = *GImGui;

   ImGuiIO& io = ImGui::GetIO(); (void)io;
   const ImGuiStyle& style = g.Style;
   ImVec2 pos = ImGui::GetWindowPos();
   ImVec2 size = ImGui::GetWindowSize();
   ImDrawList* pDrawList = ImGui::GetWindowDrawList();
   float HeaderHeight = ImGui::GetFontSize() + style.WindowPadding.y * 2 + style.ChildBorderSize * 2;
   pos.y = pos.y - 4;
  
   pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
   pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
   pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
   pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
   pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2), ImGui::GetColorU32(ImGuiCol_Text), label);

   ImGui::SetCursorPosY(HeaderHeight - style.WindowPadding.y + 2);
}

void overlay::load_interface()
{

    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"SUNK OVERLAY", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);

    wc.cbClsExtra = NULL;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.cbWndExtra = NULL;
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = TEXT(L"base");
    wc.lpszMenuName = nullptr;
    wc.style = CS_VREDRAW | CS_HREDRAW;

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margin = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margin);



    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;



    ImGuiStyle& style = ImGui::GetStyle();
       style.WindowRounding = 6;
   style.ChildRounding = 3;
   ImVec4* colors = ImGui::GetStyle().Colors;
   colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
   colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
   colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
   colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
   colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
   colors[ImGuiCol_Border] = ImVec4(0.35f, 0.35f, 0.35f, 0.50f);
   colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
   colors[ImGuiCol_FrameBgHovered] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
   colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
   colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
   colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
   colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
   colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
   colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
   colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
   colors[ImGuiCol_CheckMark] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_SliderGrab] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_SliderGrabActive] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_Button] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_ButtonHovered] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_ButtonActive] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_Header] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_HeaderHovered] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_HeaderActive] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);
   colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
   colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
   colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
   colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
   colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
   colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
   colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
   colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.19f, 0.34f, 0.86f);
   colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
   colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
   colors[ImGuiCol_TabDimmed] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
   colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
   colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
   colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
   colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
   colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
   colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
   colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
   colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
   colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
   colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
   colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
   colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
   colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
   colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
   colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
   colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
   colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
   colors[ImGuiCol_WindowShadow] = ImVec4(0.41f, 0.00f, 1.00f, 1.00f);





    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);

    bool done = false;

    initialize_avatar_system();

    while (done == false)
    {
        if (!globals::firstreceived)return;
        

        auto avatar_mgr = overlay::get_avatar_manager();
        for (roblox::player entity : globals::instances::cachedplayers) {
         
            if (avatar_mgr) {
                if (!entity.pictureDownloaded) {
                    avatar_mgr->requestAvatar(entity.userid.address);
                }
                else {
                    continue;
                }
             
            }
            else {
                break;
            }
        }

 
            static HWND robloxWindow = FindWindowA(0, "Roblox");
            robloxWindow = FindWindowA(0, "Roblox");

            update_avatars();

            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);

                if (msg.message == WM_QUIT)
                {
                    done = true;
                    break;
                }
            }

            if (done == true)
            {
                break;
            }

            if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
            {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                g_ResizeWidth = g_ResizeHeight = 0;
                CreateRenderTarget();
            }
                      ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
          			movewindow(hwnd);


         


                            
                                                             
            const bool team = (*globals::combat::flags)[0] != 0;
            const bool knock = (*globals::combat::flags)[1] != 0;
            const bool range = (*globals::combat::flags)[2] != 0;
            const bool health = (*globals::combat::flags)[3] != 0;
            const bool walls = (*globals::combat::flags)[4] != 0;

            globals::combat::teamcheck = team;
            globals::combat::knockcheck = knock;
            globals::combat::rangecheck = range;
            globals::combat::healthcheck = health;
            globals::combat::wallcheck = walls;

            



            if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hwnd)) {
                globals::focused = true;
            }
            else {
                globals::focused = false;
            }
            if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hwnd))
            {


                static bool firsssssssssssssssss = true;
                if (globals::focused && firsssssssssssssssss) {
                    overlay::visible = true;
                    firsssssssssssssssss = false;
                }
               auto drawbglist =  ImGui::GetBackgroundDrawList();
               POINT cursor_pos;
               GetCursorPos(&cursor_pos);
               ScreenToClient(robloxWindow, &cursor_pos);
               ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
               render_keybind_list();
               render_watermark();
               render_target_hud();
       
               if (overlay::visible)
               {
                   static ImVec2 current_dimensions;
                           render_player_list();
                           render_explorer();
                   current_dimensions = ImVec2(globals::instances::visualengine.GetDimensins().x, globals::instances::visualengine.GetDimensins().y);

                   ImGui::GetBackgroundDrawList()->AddRectFilled(
                       ImVec2(0, 0),
                       current_dimensions,
                       ImGui::GetColorU32(ImVec4(0.12, 0.12, 0.12, 0.89f)),
                       0
                   );

               }
               if (globals::combat::drawfov) {
                   if (globals::combat::glowfov) {
                    drawbglist->AddShadowCircle(mousepos, globals::combat::fovsize, ImGui::ColorConvert(globals::combat::fovglowcolor), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 64);
                   }
                   drawbglist->AddCircle(mousepos, globals::combat::fovsize-1, IM_COL32(0, 0, 0, 255));
                   drawbglist->AddCircle(mousepos, globals::combat::fovsize, ImGui::ColorConvert(globals::combat::fovcolor));
                   drawbglist->AddCircle(mousepos, globals::combat::fovsize+1, IM_COL32(0, 0, 0, 255));
               }
               if (globals::combat::drawsfov) {
                   POINT cursor_pos;
                   GetCursorPos(&cursor_pos);
                   ScreenToClient(robloxWindow, &cursor_pos);
                   ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
                   if (globals::combat::glowsfov) {
                           drawbglist->AddShadowCircle(mousepos, globals::combat::sfovsize, ImGui::ColorConvert(globals::combat::sfovglowcolor), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 64);
                    }
                       drawbglist->AddCircle(mousepos, globals::combat::sfovsize - 1, IM_COL32(0, 0, 0, 255));
                       drawbglist->AddCircle(mousepos, globals::combat::sfovsize, ImGui::ColorConvert(globals::combat::sfovcolor));
                       drawbglist->AddCircle(mousepos, globals::combat::sfovsize + 1, IM_COL32(0, 0, 0, 255));
               }
                if (GetAsyncKeyState(VK_RSHIFT) & 1)
                {
                    overlay::visible = !overlay::visible;
                }
                if (GetAsyncKeyState(VK_F1) & 1)
                {
                    overlay::visible = !overlay::visible;
                }
                if (GetAsyncKeyState(VK_INSERT) & 1)
                {
                    overlay::visible = !overlay::visible;
                }
                if (GetAsyncKeyState(VK_HOME) & 1)
                {
                    overlay::visible = !overlay::visible;
                }
                if (overlay::visible)
                {

                    style.WindowShadowSize = 0;

                    style.Colors[ImGuiCol_WindowShadow] = style.Colors[ImGuiCol_SliderGrab];
                    ImGui::PopStyleColor(1);
                    ImGui::SetNextWindowSize(ImVec2(600, 750), ImGuiCond_Once);
                    ImGui::Begin("cigan ware", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    ImVec2 window_pos = ImGui::GetWindowPos();
                    float window_height = ImGui::GetWindowHeight();
                    float window_width = ImGui::GetWindowWidth();
                    ImVec2 window_size = ImGui::GetWindowSize();


                    ImGui::SetCursorPosY(4);
                    ImAdd::Text(style.Colors[ImGuiCol_Text], "centrum");
                    ImGui::SameLine();
                    ImAdd::Text(style.Colors[ImGuiCol_Text], "/");
                    ImGui::SameLine();
                    ImAdd::Text(ImVec4(0.8,0.8,0.8,1), "@");
                    ImGui::SameLine();
                    ImAdd::Text(style.Colors[ImGuiCol_Text], globals::instances::username.c_str());
            
                                     draw->AddShadowRect(ImVec2(window_pos.x, window_pos.y + 21.25), ImVec2(window_pos.x + window_size.x, window_pos.y + 22.25), ImGui::GetColorU32(ImGuiCol_SliderGrab), 15, ImVec2(0, 0), NULL, 0);
                    draw->AddLine(ImVec2(window_pos.x, window_pos.y + 21.25), ImVec2(window_pos.x + window_size.x, window_pos.y + 21.25), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1);
                                              static int iTabID = 0;
                    float fRadioWidth = -4.5f + (ImGui::GetWindowWidth() - 325 - style.WindowPadding.x * 2) /5;
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - fRadioWidth);

                    ImGui::SetCursorPosX(ImGui::GetWindowWidth()/2 - (fRadioWidth * 2.5f));
                    ImGui::BeginGroup();
                    {
                        ImAdd::FrameRadio("Combat", &iTabID, 0, ImVec2(fRadioWidth, fRadioWidth)); ImGui::SameLine(0, 0);
                        ImAdd::FrameRadio("Visuals", &iTabID, 1, ImVec2(fRadioWidth, fRadioWidth)); ImGui::SameLine(0, 0);
                        ImAdd::FrameRadio("World", &iTabID, 2, ImVec2(fRadioWidth, fRadioWidth)); ImGui::SameLine(0, 0);
                        ImAdd::FrameRadio("Lua", &iTabID, 3, ImVec2(fRadioWidth, fRadioWidth)); ImGui::SameLine(0, 0);
                        ImAdd::FrameRadio("Settings", &iTabID, 4, ImVec2(fRadioWidth, fRadioWidth)); ImGui::SameLine(0, 0);
                    }

                    ImGui::EndGroup();


                    draw->AddShadowRect(ImVec2(window_pos.x, window_pos.y + ImGui::GetWindowHeight() - fRadioWidth), ImVec2(window_pos.x + window_size.x, window_pos.y + ImGui::GetWindowHeight() - fRadioWidth), ImGui::GetColorU32(ImGuiCol_SliderGrab), 35, ImVec2(0, 0), NULL, 0);
                    draw->AddLine(ImVec2(window_pos.x, window_pos.y + ImGui::GetWindowHeight() - fRadioWidth), ImVec2(window_pos.x + window_size.x, window_pos.y + ImGui::GetWindowHeight() - fRadioWidth), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1);
                    ImGui::SetCursorPosY(27.5f);

                    draw->AddRectFilled(ImVec2(window_pos.x, window_pos.y + 22.25), ImVec2(window_pos.x + window_size.x, window_pos.y + ImGui::GetWindowHeight() - fRadioWidth), ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered));
                    style.FrameBorderSize = 1;
                    ImGui::BeginGroup(); {
                                                if (iTabID == 0) {
                            ImGui::BeginGroup();
                            {
                                ImGui::BeginGroup();
                                {
                                    ImAdd::BeginChild("Aimbot", ImVec2(window_width / 2 - 12, 105));
                                    {
                                        draw_shadowed_text("Aimbot");
                                        ImAdd::CheckBox("Aimbot", &globals::combat::aimbot);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::aimbotkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::combat::aimbotkeybind, ImVec2(40, 10));
                                        ImAdd::CheckBox("Sticky Aim", &globals::combat::stickyaim);
                                        ImAdd::Combo("Type", &globals::combat::aimbottype, "Camera\0Mouse\0");

                                    }
                                    ImAdd::EndChild();

                                    ImAdd::BeginChild("FOV1", ImVec2(window_width / 2 - 12, 120));
                                    {
                                        draw_shadowed_text("FOV");
                                        ImAdd::CheckBox("Show Fov", &globals::combat::drawfov);
                                        ImGui::SameLine(); ImAdd::ColorEdit4("                                                               ", globals::combat::fovcolor);
                                        ImAdd::CheckBox("Use Fov", &globals::combat::usefov);
                                        ImAdd::SliderFloat("Fov Radius", &globals::combat::fovsize, 10, 1000);
                                        ImAdd::CheckBox("Fov Glow", &globals::combat::glowfov);
                                        ImGui::SameLine(); ImAdd::ColorEdit4("                                                      ", globals::combat::fovglowcolor);

                                    }
                                    ImAdd::EndChild();

                                    ImAdd::BeginChild("Smoothing", ImVec2(window_width / 2 - 12, 120));
                                    {
                                        draw_shadowed_text("Smoothing");
                                        ImAdd::CheckBox("Smoothing", &globals::combat::smoothing);
                                        ImAdd::SliderFloat("Smoothing X", &globals::combat::smoothingx, 1, 50);
                                        ImAdd::SliderFloat("Smoothing Y", &globals::combat::smoothingy, 1, 50);


                                    }
                                    ImAdd::EndChild();

                                    ImAdd::BeginChild("Predictions", ImVec2(window_width / 2 - 12, 120));
                                    {
                                        draw_shadowed_text("Predictions");
                                        ImAdd::CheckBox("Predictions", &globals::combat::predictions);
                                        ImAdd::SliderFloat("Predictions X", &globals::combat::predictionsx, 1, 15);
                                        ImAdd::SliderFloat("Predictions Y", &globals::combat::predictionsy, 1, 15);
                                    }
                                    ImAdd::EndChild();

                                    ImAdd::BeginChild("Flags", ImVec2(window_width / 2 - 12, 170));
                                    {
                                        std::vector<const char*>  stuff = {"Team", "Knocked", "Range", "Health", "WallCheck", "Grabbed", "Forcefield"};

                                        if (globals::combat::flags == nullptr) {
                                            globals::combat::flags = new std::vector<int>(stuff.size(), 0);
                                        }

                                        ImAdd::MultiCombo("Flags", globals::combat::flags, stuff);
                                        ImAdd::CheckBox("Auto-Switch On Condition", &globals::combat::autoswitch);
                                        ImAdd::SliderFloat("Range", &globals::combat::range, 1, 25);
                                        ImAdd::SliderFloat("Health", &globals::combat::healththreshhold, 0, 100);
                                    }
                                    ImAdd::EndChild();
                                }
                                ImGui::EndGroup();
                                ImGui::SameLine();
                                ImGui::BeginGroup();
                                {
                                    ImAdd::BeginChild("Silent", ImVec2(window_width / 2 - 12, 105));
                                    {
                                        draw_shadowed_text("Silent Aim");
                                        ImAdd::CheckBox("Silent", &globals::combat::silentaim);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::silentaimkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::combat::silentaimkeybind, ImVec2(40, 10));
                                        ImAdd::CheckBox("Sticky Aim  ", &globals::combat::stickyaimsilent);
                                        ImAdd::CheckBox("Spoof Mouse", &globals::combat::spoofmouse);
                                        ImAdd::SliderInt("Hit Chance", &globals::combat::hitchance, 1, 100);
                                    }
                                }
                                ImAdd::EndChild();
                                ImAdd::BeginChild("Silent FOV", ImVec2(window_width / 2 - 12, 150));
                                {
                                    draw_shadowed_text("Silent FOV");
                                    ImAdd::CheckBox("Show Fov ", &globals::combat::drawsfov);
                                    ImGui::SameLine(); ImAdd::ColorEdit4("                                             ", globals::combat::sfovcolor);
                                    ImAdd::CheckBox("Use Fov ", &globals::combat::usesfov);
                                    ImAdd::SliderFloat("Fov Radius ", &globals::combat::sfovsize, 10, 1000);
                                    ImAdd::CheckBox("Fov Glow ", &globals::combat::glowsfov);
                                    ImGui::SameLine(); ImAdd::ColorEdit4("                                                       ", globals::combat::sfovglowcolor);
                                }
                                ImAdd::EndChild();

                                ImAdd::BeginChild("Orbit", ImVec2(window_width / 2 - 12, 185));
                                {
                                    draw_shadowed_text("Orbit");
                                    ImAdd::CheckBox("Orbit", &globals::combat::orbit);
                                    ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::orbitkeybind.get_key_name().c_str()).x / 2 - 40, 0);


                                    Bind(&globals::combat::orbitkeybind, ImVec2(40, 10));
                                    std::vector<const char*> stuff = { "X-AXIS", "Y-AXIS", "Random" };

                                    if (globals::combat::orbittype.size() != stuff.size()) {
                                        globals::combat::orbittype.resize(stuff.size(), 0);
                                    }

                                    ImAdd::MultiCombo("Orbit Axis", &globals::combat::orbittype, stuff);


                                    ImAdd::SliderFloat("Speed                                 ", &globals::combat::orbitspeed, 0.1f, 24);
                                    ImAdd::SliderFloat("Height      ", &globals::combat::orbitheight, -10, 25);
                                    ImAdd::SliderFloat("Range       ", &globals::combat::orbitrange, 1, 25);


                                }
                                ImAdd::EndChild();
                                ImAdd::BeginChild("TriggerBot", ImVec2(window_width / 2 - 12, 162));
                                {
                                    draw_shadowed_text("Triggerbot");

                                    ImAdd::CheckBox("TriggerBot", &globals::combat::triggerbot);
                                    ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::combat::triggerbotkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                    Bind(&globals::combat::triggerbotkeybind, ImVec2(40, 10));
                                    ImAdd::CheckBox("Range                   ", &globals::combat::triggerbotrange);
                                    ImAdd::SliderFloat("Range        ", &globals::combat::triggerbotrangevalue, 10, 500);
                                    ImAdd::SliderFloat("Delay M/S", &globals::combat::delay, 0, 50);
                                    ImAdd::SliderFloat("Release Delay M/S", &globals::combat::releasedelay, 0, 50);


                                }
                                ImAdd::EndChild();

                            }
                            ImGui::EndGroup();
                        }
                                                if (iTabID == 1) {
                            ImGui::BeginGroup();
                            {
                                ImGui::BeginGroup();
                                {
                                    ImAdd::BeginChild("ESP", ImVec2(window_width / 2 - 12, 650));
                                    {
                                        draw_shadowed_text("Visuals");
                                        ImAdd::CheckBox("Visuals", &globals::visuals::visuals);

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Boxes", &globals::visuals::boxes);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##boxcolor", globals::visuals::boxcolors);
                                        if (globals::visuals::boxes) {

                                            ImAdd::Combo("Box Type", &globals::visuals::boxtype, "Bounding\0Corners\0");

                                            std::vector<const char*> box_overlays = { "Outline", "Glow", "Fill" };
                                            ImAdd::MultiCombo("Box Overlays", globals::visuals::box_overlay_flags, box_overlays);

                                                                                     ImGui::Spacing();
                                                draw_shadowed_text("Fill Color");            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                                ImAdd::ColorEdit4("Fill Cosdfsdflor", globals::visuals::boxfillcolor);
                                                                                        ImGui::Spacing();
                                                                                           draw_shadowed_text("Glow Color");            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                                ImAdd::ColorEdit4("Glow Csdfsdfolor", globals::visuals::glowcolor);
                                                                                    }

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Name ESP", &globals::visuals::name);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##namecolor", globals::visuals::namecolor);
                                        ImAdd::Combo("Name Type", &globals::visuals::nametype, "Username\0Display Name\0");


                                        ImGui::Separator();

                                        ImAdd::CheckBox("Health Bar", &globals::visuals::healthbar);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##healthcolor1", globals::visuals::healthbarcolor);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48, 0);
                                        ImAdd::ColorEdit4("##healthcolor2", globals::visuals::healthbarcolor1);
                                        if (globals::visuals::healthbar) {
                                            std::vector<const char*> health_overlays = { "Outline", "Gradient", "Glow" };
                                            ImAdd::MultiCombo("Health Overlays", globals::visuals::healthbar_overlay_flags, health_overlays);
                                        }

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Tool Name", &globals::visuals::toolesp);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##toolcolor", globals::visuals::toolespcolor);

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Distance", &globals::visuals::distance);  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##distancecolor", globals::visuals::distancecolor);


                                        ImGui::Separator();

                                        ImAdd::CheckBox("Skeleton", &globals::visuals::skeletons);
                                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##skeletoncolor", globals::visuals::skeletonscolor);

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Snaplines", &globals::visuals::snapline);   ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##snaplinecolor", globals::visuals::snaplinecolor);

                                        ImGui::Separator();

                                        ImAdd::CheckBox("Chams", &globals::visuals::chams);  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##chamscolor", globals::visuals::chamscolor);

                                        if (globals::visuals::chams) {
                             
                                            std::vector<const char*> chams_overlays = { "Outline", "Glow", "Fill" };
                                            ImAdd::MultiCombo("Chams Overlays", globals::visuals::chams_overlay_flags, chams_overlays);
                                        }

                                        ImGui::Separator();

                                        ImAdd::CheckBox("OOF Arrows", &globals::visuals::oofarrows);   ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                                        ImAdd::ColorEdit4("##oofcolor", globals::visuals::oofcolor);
                                    }
                                    ImAdd::EndChild();
                                }
                                ImGui::EndGroup();
                                ImGui::SameLine();
                                ImGui::BeginGroup();
                                {

                                    ImAdd::EndChild();
                                }
                                ImGui::EndGroup();
                            }
                            ImGui::EndGroup();
                        }
                                                if (iTabID == 2) {
                            ImGui::BeginGroup();
                            {
                                ImGui::BeginGroup();
                                {
                                    ImAdd::BeginChild("Self", ImVec2(window_width / 2 - 12, 415));
                                    {
                                        draw_shadowed_text("Self");

                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
                                        ImAdd::CheckBox("Speed   ", &globals::misc::speed);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::speedkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::speedkeybind, ImVec2(40, 10));
                                        ImAdd::Combo("Mode", &globals::misc::speedtype, "WalkSpeed\0Velocity\0Position\0");
                                        ImAdd::SliderFloat("Speed", &globals::misc::speedvalue, 1, 750);

                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                        ImGui::Separator();
                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

                                        ImAdd::CheckBox("JumpPower", &globals::misc::jumppower);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::jumppowerkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::jumppowerkeybind, ImVec2(40, 10));
                                        ImAdd::SliderFloat("Power", &globals::misc::jumpowervalue, 1, 750);

                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                        ImGui::Separator();
                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

                                        ImAdd::CheckBox("Flight", &globals::misc::flight);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::flightkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::flightkeybind, ImVec2(40, 10));
                                        ImAdd::Combo("Mode   ", &globals::misc::flighttype, "Position\0Velocity\0");
                                        ImAdd::SliderFloat("Speed         ", &globals::misc::flightvalue, 1, 750);

                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                        ImGui::Separator();
                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

                                        ImAdd::CheckBox("VoidHide", &globals::misc::voidhide);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::voidhidebind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::voidhidebind, ImVec2(40, 10));


                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                        ImGui::Separator();
                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

                                        ImAdd::CheckBox("AutoStomp", &globals::misc::autostomp);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::stompkeybind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::stompkeybind, ImVec2(40, 10));

                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
                                        ImGui::Separator();
                                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

                                        ImAdd::CheckBox("Freecam", &globals::misc::spectate);
                                        ImGui::SameLine(window_width / 2 - ImGui::CalcTextSize(globals::misc::spectatebind.get_key_name().c_str()).x / 2 - 40, 0);
                                        Bind(&globals::misc::spectatebind, ImVec2(40, 10));

                                        if (ImAdd::Button("Force-Reset", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
                                            globals::instances::lp.humanoid.write_health(0);
                                        }

                                    }
                                    ImAdd::EndChild();
                                    ImAdd::BeginChild("PlayerMods", ImVec2(window_width / 2 - 15, 125));
                                    {
                                        draw_shadowed_text("PlayerMods");
                                        ImAdd::CheckBox("RapidFire", &globals::misc::rapidfire);
                                        ImAdd::CheckBox("Antistomp", &globals::misc::antistomp);
                                        ImAdd::CheckBox("AutoReload", &globals::misc::autoreload);
                                        ImAdd::CheckBox("AutoArmor", &globals::misc::autoarmor);
                                        ImAdd::CheckBox("Vehicle-Fly", &globals::misc::bikefly);
                                        ImAdd::CheckBox("Anti Aim (Prediction Breaker)", &globals::combat::antiaim);
                                        ImAdd::CheckBox("Underground AA", &globals::combat::underground_antiaim);
                                    }
                                    ImAdd::EndChild();
                                }
                                ImGui::EndGroup();
                                ImGui::SameLine();
                                ImGui::BeginGroup();
                                {
                                }
                                ImGui::EndGroup();
                            }

                            ImGui::EndGroup();
                        }
                                                if (iTabID == 3) {
                            ImGui::BeginGroup();
                            {
                                static char luaCode[8192] = "-- Write your Lua code here\nprint(\"Hello World\")\n\nfunction test()\n    return 42\nend";
                                static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

                                ImVec2 availableSize = ImGui::GetContentRegionAvail();
                                availableSize.y = availableSize.y - fRadioWidth;
                                float lineNumberWidth = 50.0f;
                                float buttonHeight = 25.0f;
                                float spacing = 5.0f;

                                ImVec2 textSize = ImVec2(availableSize.x - lineNumberWidth - spacing, availableSize.y - buttonHeight - spacing * 2);
                                static float currenty;
                                ImGui::BeginGroup();
                                {
                                    int lineCount = 1;
                                    for (int i = 0; luaCode[i] != '\0'; i++) {
                                        if (luaCode[i] == '\n') lineCount++;
                                    }

                                    ImGui::BeginGroup();
                                    {

                                        ImGui::BeginChild("LineNumbers", ImVec2(lineNumberWidth, textSize.y), true, ImGuiWindowFlags_NoScrollbar);
                                        {
                                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));


                                            float lineHeight = ImGui::GetTextLineHeight();
                                            for (int i = 1; i <= lineCount; i++) {
                                                ImGui::Text("%3d", i);
                                                if (i < lineCount) {
                                                    ImGui::SetCursorPosY(ImGui::GetCursorPosY());
                                                }
                                            }

                                            ImGui::PopStyleColor();
                                        }
                                        ImGui::EndChild();

                                        ImGui::SameLine();

                                        ImGui::BeginChild("CodeEditor", textSize, true);
                                        {
                                            ImGui::PushFont(ImGui::GetFont());
                                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));

                                            if (ImGui::InputTextMultiline("##LuaCode", luaCode, sizeof(luaCode),
                                                ImVec2(-1, -1), flags)) {
                  
                                            }

                                            ImGui::PopStyleVar();
                                            ImGui::PopFont();
                                        }
                                        ImGui::EndChild();
                                    }
                                    ImGui::EndGroup();

                                    ImGui::Spacing();
                                    if (ImGui::Button("Execute", ImVec2(80, buttonHeight))) {

                                    }

                                    ImGui::SameLine();
                                    if (ImGui::Button("Clear", ImVec2(80, buttonHeight))) {
                                        strcpy_s(luaCode, sizeof(luaCode), "");
                                    }

                                    ImGui::SameLine();
                                    if (ImGui::Button("Load File", ImVec2(80, buttonHeight))) {
                                    }

                                    ImGui::SameLine();
                                    if (ImGui::Button("Save File", ImVec2(80, buttonHeight))) {

                                    }

                                    ImGui::SameLine();
                                    ImGui::Text("Lines: %d", lineCount);
                                }
                                ImGui::EndGroup();
                            }
                            ImGui::EndGroup();
                        }
                                                if (iTabID == 4) {
                            ImGui::BeginGroup();
                            {
                                ImGui::BeginGroup();
                                {
                                    ImAdd::BeginChild("UI SETTINGS", ImVec2(window_width / 2 - 12, 300));
                                    {
                                        draw_shadowed_text("UI SETTINGS");

                                        if (ImAdd::CheckBox("Watermark", &globals::misc::watermark)) {
                                                                                }
                                        std::vector<const char*>  stuff = {"FPS", "Username", "Date"};

                                        if (globals::misc::watermarkstuff == nullptr) {
                                            globals::misc::watermarkstuff = new std::vector<int>(stuff.size(), 0);
                                        }

                                        ImAdd::MultiCombo("Styling", globals::misc::watermarkstuff, stuff);

                                        if (ImAdd::CheckBox("VSYNC", &globals::misc::vsync)) {
                     

                                        }

                                        ImAdd::CheckBox("Keybind List", &globals::misc::keybinds);
                                        ImAdd::Combo("Keybind Style", &globals::misc::keybindsstyle, "Dynamic\0Static\0");
                                        ImAdd::CheckBox("Playerlist", &globals::misc::playerlist);
                                        ImAdd::CheckBox("DexExplorer", &globals::misc::explorer);
                                        ImAdd::CheckBox("TargetHud", &globals::misc::targethud);
                                        ImAdd::CheckBox("Streamproof", &globals::misc::streamproof);
                                        ImAdd::CheckBox("Crosshair", &globals::misc::colors);
                                    }
                                    ImAdd::EndChild();

                                    ImAdd::BeginChild("asdasdasd", ImVec2(window_width / 2 - 12, 300));
                                    {
                                        if (ImAdd::Button("Eject", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
                                           // exit(0);
                                        }
                                    
                                    }
                                    ImAdd::EndChild();
                                }
                                ImGui::EndGroup();

                                ImGui::SameLine();

                                ImGui::BeginGroup();
                                {
                                    ImVec2 availableSize = ImGui::GetContentRegionAvail();
                                    availableSize.y = availableSize.y - fRadioWidth;

                                  g_config_system.render_config_ui(window_width / 2 - 12, availableSize.y);
                                }
                                ImGui::EndGroup();
                            }
                            ImGui::EndGroup();
                        }


                    }
                    style.FrameBorderSize = 0;
                    ImGui::End();

                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                overlay::visible = false;
            }

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::Begin("esp", NULL,
                ImGuiWindowFlags_NoBackground
                |
                ImGuiWindowFlags_NoResize
                |
                ImGuiWindowFlags_NoCollapse
                |
                ImGuiWindowFlags_NoTitleBar
                |
                ImGuiWindowFlags_NoInputs
                |
                ImGuiWindowFlags_NoMouseInputs
                |
                ImGuiWindowFlags_NoDecoration
                |
                ImGuiWindowFlags_NoMove); {

                globals::instances::draw = ImGui::GetBackgroundDrawList();
                visuals::run();

            }

            ImGui::End();


            if (overlay::visible) {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
            }
            else
            {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

            }

            if (globals::misc::streamproof)
            {
                SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            }
            else
            {
                SetWindowDisplayAffinity(hwnd, WDA_NONE);
            }



            Notifications::Update();
            Notifications::Render();
         




            ImGui::Render();
            const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            if (globals::misc::vsync) {
                g_pSwapChain->Present(1, 0);
            }
            else {
                g_pSwapChain->Present(0, 0);
            }

            static bool test_sent = false;
            if (!test_sent && overlay::visible) {
                Notifications::Success("Overlay Loaded!");
                test_sent = true;
            }
            static float cachedhealth = 0;
            static std::string lastname = "";
       
            if ((globals::combat::aimbot && globals::combat::aimbotkeybind.enabled)
            || (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled)) {
                if (globals::instances::cachedtarget.head.address != 0) {
                    if (lastname == globals::instances::cachedtarget.name) {
                                            if (globals::instances::cachedtarget.humanoid.read_health() < cachedhealth) {
                            std::string sdfsdf = std::to_string(static_cast<int>(globals::instances::cachedtarget.humanoid.read_health()));
                            std::string temph = globals::instances::cachedtarget.name + " HP: " + sdfsdf;
                            Notifications::Success(temph);
                                                      cachedhealth = globals::instances::cachedtarget.humanoid.read_health();
                        }
                    }
                    else {
                        cachedhealth = globals::instances::cachedtarget.humanoid.read_health();
                        lastname = globals::instances::cachedtarget.name;
                    }
                }

            }
        
          /*  static LARGE_INTEGER frequency;
            static LARGE_INTEGER lastTime;
            static bool timeInitialized = false;

            if (!timeInitialized) {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&lastTime);
                timeBeginPeriod(1);
                timeInitialized = true;
            }

            const double targetFrameTime = 1.0 / 165;

            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;

            if (elapsedSeconds < targetFrameTime) {
                DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
                if (sleepMilliseconds > 0) {
                    Sleep(sleepMilliseconds);
                }
            }

            do {
                QueryPerformanceCounter(&currentTime);
                elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            } while (elapsedSeconds < targetFrameTime);

            lastTime = currentTime;*/

    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_avatar_system();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

bool fullsc(HWND windowHandle)
{
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
    {
        RECT rect;
        if (GetWindowRect(windowHandle, &rect))
        {
            return rect.left == monitorInfo.rcMonitor.left
                && rect.right == monitorInfo.rcMonitor.right
                && rect.top == monitorInfo.rcMonitor.top
                && rect.bottom == monitorInfo.rcMonitor.bottom;
        }
    }
}

void movewindow(HWND hw) {
    HWND target = FindWindowA(0, "Roblox");
	HWND foregroundWindow = GetForegroundWindow();

	if (target != foregroundWindow && hw != foregroundWindow)
	{
		MoveWindow(hw, 0, 0, 0, 0, true);
	}
	else
	{
		RECT rect;
		GetWindowRect(target, &rect);

		int rsize_x = rect.right - rect.left;
		int rsize_y = rect.bottom - rect.top;

		if (fullsc(target))
		{
			rsize_x += 16;
            rect.right -= 16;
		}
		else
		{
			rsize_y -= 39;
			rect.left += 8;
			rect.top += 31;
            rect.right -= 16;
		}
        rsize_x -= 16;
		MoveWindow(hw, rect.left, rect.top, rsize_x, rsize_y, TRUE);
	}
}
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}