#include "../visuals.h"
#include <Windows.h>
#include "../../wallcheck/wallcheck.h"
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <algorithm>
#include <vector>
#define min
#undef min
#define max
#undef max

ESPData visuals::espData;
std::thread visuals::updateThread;
bool visuals::threadRunning = false;

void draw_text_with_shadow_enhanced(float font_size, const ImVec2& position, ImColor color, const char* text);

inline float distance_sq(const Vector3& v1, const Vector3& v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    float dz = v1.z - v2.z;
    return dx * dx + dy * dy + dz * dz;
}

inline float distance_sq(const Vector2& v1, const Vector2& v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return dx * dx + dy * dy;
}

float vector_length(const Vector3& vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

void render_box_enhanced(const Vector2& top_left, const Vector2& bottom_right, ImColor color) {
    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    float width = bottom_right.x - top_left.x;
    float height = bottom_right.y - top_left.y;

    if (globals::visuals::boxfill) {
        globals::instances::draw->AddRectFilled(
            ImVec2(top_left.x, top_left.y),
            ImVec2(bottom_right.x, bottom_right.y),
            ImGui::ColorConvert(globals::visuals::boxfillcolor), 0
        );
    }

    if (globals::visuals::boxtype == 0) {
        if ((*globals::visuals::box_overlay_flags)[1]) {
            globals::instances::draw->AddShadowRect(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                ImGui::ColorConvert(globals::visuals::glowcolor), 15.f, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 0
            );
        }

        if ((*globals::visuals::box_overlay_flags)[0]) {
            globals::instances::draw->AddRect(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                ImColor(0, 0, 0, 255), 0.0f, 0, 2.5f
            );
        }

        globals::instances::draw->AddRect(
            ImVec2(top_left.x, top_left.y),
            ImVec2(bottom_right.x, bottom_right.y),
            color, 0.0f, 0, 0.25f
        );

        if ((*globals::visuals::box_overlay_flags)[2]) {
            ImColor boxfill = ImColor(globals::visuals::boxfillcolor[0], globals::visuals::boxfillcolor[1], globals::visuals::boxfillcolor[2], globals::visuals::boxfillcolor[3]);
            globals::instances::draw->AddRectFilled(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                boxfill, 0.0f, 0
            );
        }
    }
    else if (globals::visuals::boxtype == 1) {
        float corner_size = std::min(width * 1.5f, height) * 0.25f;

        if ((*globals::visuals::box_overlay_flags)[1]) {
            globals::instances::draw->AddShadowRect(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                ImGui::ColorConvert(globals::visuals::glowcolor), 15.f, ImVec2(0, 0),
                ImDrawFlags_ShadowCutOutShapeBackground, 0
            );
        }

        if ((*globals::visuals::box_overlay_flags)[2]) {
            ImColor boxfill = ImColor(
                globals::visuals::boxfillcolor[0],
                globals::visuals::boxfillcolor[1],
                globals::visuals::boxfillcolor[2],
                globals::visuals::boxfillcolor[3]
            );
            globals::instances::draw->AddRectFilled(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                boxfill, 0.0f, 0
            );
        }

        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x + corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x - corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x + corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x, bottom_right.y - corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x - corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x, bottom_right.y - corner_size), color, 1.0f);

        if ((*globals::visuals::box_overlay_flags)[0]) {
            ImColor outline_color = ImColor(0, 0, 0, 180);
            float outline_thickness = 2.0f;

            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, top_left.y - 1), ImVec2(top_left.x + corner_size + 1, top_left.y - 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, top_left.y - 1), ImVec2(top_left.x - 1, top_left.y + corner_size + 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, top_left.y - 1), ImVec2(bottom_right.x - corner_size - 1, top_left.y - 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, top_left.y - 1), ImVec2(bottom_right.x + 1, top_left.y + corner_size + 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, bottom_right.y + 1), ImVec2(top_left.x + corner_size + 1, bottom_right.y + 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, bottom_right.y + 1), ImVec2(top_left.x - 1, bottom_right.y - corner_size - 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, bottom_right.y + 1), ImVec2(bottom_right.x - corner_size - 1, bottom_right.y + 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, bottom_right.y + 1), ImVec2(bottom_right.x + 1, bottom_right.y - corner_size - 1), outline_color, outline_thickness);
        }

        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x + corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x - corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x + corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x, bottom_right.y - corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x - corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x, bottom_right.y - corner_size), color, 1.0f);
    }
}

void render_health_bar_enhanced(const Vector2& top_left, const Vector2& bottom_right, float health_percent, ImColor color) {
    if (!globals::visuals::healthbar) return;

    float box_width = bottom_right.x - top_left.x;
    float box_height = bottom_right.y - top_left.y;

    float bar_width = 1.f;
    float bar_height = box_height;
    float bar_x = top_left.x - bar_width - 4.0f;

    if ((*globals::visuals::healthbar_overlay_flags)[0]) {
        ImColor outline_color = ImColor(0, 0, 0, 255);
        globals::instances::draw->AddRectFilled(
            ImVec2(bar_x - 1, top_left.y - 1),
            ImVec2(bar_x + bar_width + 1, bottom_right.y + 1),
            outline_color
        );
    }

    if ((*globals::visuals::healthbar_overlay_flags)[2]) {
        globals::instances::draw->AddShadowRect(
            ImVec2(bar_x, top_left.y),
            ImVec2(bar_x + bar_width, bottom_right.y),
            ImGui::ColorConvert(globals::visuals::healthbarcolor), 5.f, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 0
        );
    }

    ImColor background_color = ImColor(35, 35, 35, 200);
    globals::instances::draw->AddRectFilled(
        ImVec2(bar_x, top_left.y),
        ImVec2(bar_x + bar_width, bottom_right.y),
        background_color
    );

    color = ImGui::ColorConvert(globals::visuals::healthbarcolor);
    auto color1 = ImGui::ColorConvert(globals::visuals::healthbarcolor1);
    float fill_height = bar_height * health_percent;

    if ((*globals::visuals::healthbar_overlay_flags)[1]) {
        globals::instances::draw->AddRectFilledMultiColor(
            ImVec2(bar_x, bottom_right.y - fill_height),
            ImVec2(bar_x + bar_width, bottom_right.y),
            color, color, color1, color1
        );
    }
    else {
        globals::instances::draw->AddRectFilled(
            ImVec2(bar_x, bottom_right.y - fill_height),
            ImVec2(bar_x + bar_width, bottom_right.y),
            color
        );
    }
}

void render_name_enhanced(const std::string& name, const Vector2& top_left, const Vector2& bottom_right) {
    if (!globals::visuals::name || name.empty()) return;

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    ImVec2 text_size = ImGui::CalcTextSize(name.c_str());
    float y_pos = top_left.y - 15.0f;

    draw_text_with_shadow_enhanced(13.0f, ImVec2(center_x - text_size.x * 0.5f, y_pos), ImGui::ColorConvert(globals::visuals::namecolor), name.c_str());
}

void render_distance_enhanced(float distance, const Vector2& top_left, const Vector2& bottom_right, bool has_tool) {
    if (!globals::visuals::distance) return;

    char distance_str[16];
    sprintf_s(distance_str, "[%.0fm]", distance);

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    float offset = bottom_right.y + (globals::visuals::toolesp ? 14.0f : 2.0f);
    ImVec2 text_size = ImGui::CalcTextSize(distance_str);
    text_size.x *= (9.0f / ImGui::GetFontSize());

    draw_text_with_shadow_enhanced(9.0f, ImVec2(center_x - text_size.x * 0.5f, offset), ImGui::ColorConvert(globals::visuals::distancecolor), distance_str);
}

void render_tool_name_enhanced(const std::string& tool_name, const Vector2& top_left, const Vector2& bottom_right) {
    if (!globals::visuals::toolesp || tool_name.empty()) return;

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    float y_pos = bottom_right.y + 2.0f;
    ImVec2 text_size = ImGui::CalcTextSize(tool_name.c_str());
    text_size.x *= (11.0f / ImGui::GetFontSize());
    draw_text_with_shadow_enhanced(11.0f, ImVec2(center_x - text_size.x * 0.5f, y_pos), ImGui::ColorConvert(globals::visuals::toolespcolor), tool_name.c_str());
}

void render_snapline_enhanced(const Vector2& screen_pos) {
    if (!globals::visuals::snapline) return;

    ImVec2 screen_size = ImGui::GetIO().DisplaySize;

    globals::instances::draw->AddLine(
        ImVec2(screen_size.x * 0.5f, screen_size.y),
        ImVec2(screen_pos.x, screen_pos.y),
        ImGui::ColorConvert(globals::visuals::snaplinecolor),
        1.0f
    );
}

static std::vector<Vector2> cached_skeleton_points;
static std::chrono::steady_clock::time_point last_skeleton_update;

void render_skeleton_enhanced(const CachedPlayerData& playerData, ImColor color) {
    if (!globals::visuals::skeletons || !playerData.valid || playerData.distance > 500.0f) return;

    roblox::player player = playerData.player;

    if (!is_valid_address(player.head.address) ||
        (!is_valid_address(player.lefthand.address) && !is_valid_address(player.leftupperarm.address)) ||
        (!is_valid_address(player.righthand.address) && !is_valid_address(player.rightupperarm.address)) ||
        (!is_valid_address(player.leftfoot.address) && !is_valid_address(player.leftupperleg.address)) ||
        (!is_valid_address(player.rightfoot.address) && !is_valid_address(player.rightupperleg.address)))
        return;

    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    auto on_screen = [&dimensions](const Vector2& pos) {
        return pos.x > 0 && pos.x < dimensions.x && pos.y > 0 && pos.y < dimensions.y;
        };

    if (player.rigtype == 0) {
        Vector2 head = roblox::worldtoscreen(player.head.get_pos());
        Vector2 torso = roblox::worldtoscreen(player.uppertorso.get_pos());
        Vector2 left_arm = roblox::worldtoscreen(player.lefthand.get_pos());
        Vector2 right_arm = roblox::worldtoscreen(player.righthand.get_pos());
        Vector2 left_leg = roblox::worldtoscreen(player.leftfoot.get_pos());
        Vector2 right_leg = roblox::worldtoscreen(player.rightfoot.get_pos());

        if (on_screen(head) && on_screen(torso)) {
            globals::instances::draw->AddLine(ImVec2(head.x, head.y), ImVec2(torso.x, torso.y), color, 1.0f);
        }
        if (on_screen(torso) && on_screen(left_arm)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(left_arm.x, left_arm.y), color, 1.0f);
        }
        if (on_screen(torso) && on_screen(right_arm)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(right_arm.x, right_arm.y), color, 1.0f);
        }
        if (on_screen(torso) && on_screen(left_leg)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(left_leg.x, left_leg.y), color, 1.0f);
        }
        if (on_screen(torso) && on_screen(right_leg)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(right_leg.x, right_leg.y), color, 1.0f);
        }
    }
    else {
        if (!is_valid_address(player.leftupperarm.address) ||
            !is_valid_address(player.rightupperarm.address) ||
            !is_valid_address(player.leftlowerarm.address) ||
            !is_valid_address(player.rightlowerarm.address) ||
            !is_valid_address(player.leftupperleg.address) ||
            !is_valid_address(player.rightupperleg.address) ||
            !is_valid_address(player.leftlowerleg.address) ||
            !is_valid_address(player.rightlowerleg.address))
            return;

        Vector2 head = roblox::worldtoscreen(player.head.get_pos());
        Vector2 upper_torso = roblox::worldtoscreen(player.uppertorso.get_pos());
        Vector2 lower_torso = roblox::worldtoscreen(player.lowertorso.get_pos());
        Vector2 left_upper_arm = roblox::worldtoscreen(player.leftupperarm.get_pos());
        Vector2 right_upper_arm = roblox::worldtoscreen(player.rightupperarm.get_pos());
        Vector2 left_lower_arm = roblox::worldtoscreen(player.leftlowerarm.get_pos());
        Vector2 right_lower_arm = roblox::worldtoscreen(player.rightlowerarm.get_pos());
        Vector2 left_hand = roblox::worldtoscreen(player.lefthand.get_pos());
        Vector2 right_hand = roblox::worldtoscreen(player.righthand.get_pos());
        Vector2 left_upper_leg = roblox::worldtoscreen(player.leftupperleg.get_pos());
        Vector2 right_upper_leg = roblox::worldtoscreen(player.rightupperleg.get_pos());
        Vector2 left_lower_leg = roblox::worldtoscreen(player.leftlowerleg.get_pos());
        Vector2 right_lower_leg = roblox::worldtoscreen(player.rightlowerleg.get_pos());
        Vector2 left_foot = roblox::worldtoscreen(player.leftfoot.get_pos());
        Vector2 right_foot = roblox::worldtoscreen(player.rightfoot.get_pos());

        if (on_screen(head) && on_screen(upper_torso)) {
            head.y -= 1.5f;
            globals::instances::draw->AddLine(ImVec2(head.x, head.y), ImVec2(upper_torso.x, upper_torso.y), color, 1.0f);
        }
        if (on_screen(upper_torso) && on_screen(lower_torso)) {
            lower_torso.y += 1.5f;
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(lower_torso.x, lower_torso.y), color, 1.0f);
        }
        if (on_screen(upper_torso) && on_screen(left_upper_arm)) {
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(left_upper_arm.x, left_upper_arm.y), color, 1.0f);
        }
        if (on_screen(upper_torso) && on_screen(right_upper_arm)) {
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(right_upper_arm.x, right_upper_arm.y), color, 1.0f);
        }
        if (on_screen(left_upper_arm) && on_screen(left_lower_arm)) {
            globals::instances::draw->AddLine(ImVec2(left_upper_arm.x, left_upper_arm.y), ImVec2(left_lower_arm.x, left_lower_arm.y), color, 1.0f);
        }
        if (on_screen(right_upper_arm) && on_screen(right_lower_arm)) {
            globals::instances::draw->AddLine(ImVec2(right_upper_arm.x, right_upper_arm.y), ImVec2(right_lower_arm.x, right_lower_arm.y), color, 1.0f);
        }
        if (on_screen(left_lower_arm) && on_screen(left_hand)) {
            globals::instances::draw->AddLine(ImVec2(left_lower_arm.x, left_lower_arm.y), ImVec2(left_hand.x, left_hand.y), color, 1.0f);
        }
        if (on_screen(right_lower_arm) && on_screen(right_hand)) {
            globals::instances::draw->AddLine(ImVec2(right_lower_arm.x, right_lower_arm.y), ImVec2(right_hand.x, right_hand.y), color, 1.0f);
        }
        if (on_screen(lower_torso) && on_screen(left_upper_leg)) {
            globals::instances::draw->AddLine(ImVec2(lower_torso.x, lower_torso.y), ImVec2(left_upper_leg.x, left_upper_leg.y), color, 1.0f);
        }
        if (on_screen(lower_torso) && on_screen(right_upper_leg)) {
            globals::instances::draw->AddLine(ImVec2(lower_torso.x, lower_torso.y), ImVec2(right_upper_leg.x, right_upper_leg.y), color, 1.0f);
        }
        if (on_screen(left_upper_leg) && on_screen(left_lower_leg)) {
            globals::instances::draw->AddLine(ImVec2(left_upper_leg.x, left_upper_leg.y), ImVec2(left_lower_leg.x, left_lower_leg.y), color, 1.0f);
        }
        if (on_screen(right_upper_leg) && on_screen(right_lower_leg)) {
            globals::instances::draw->AddLine(ImVec2(right_upper_leg.x, right_upper_leg.y), ImVec2(right_lower_leg.x, right_lower_leg.y), color, 1.0f);
        }
        if (on_screen(left_lower_leg) && on_screen(left_foot)) {
            globals::instances::draw->AddLine(ImVec2(left_lower_leg.x, left_lower_leg.y), ImVec2(left_foot.x, left_foot.y), color, 1.0f);
        }
        if (on_screen(right_lower_leg) && on_screen(right_foot)) {
            globals::instances::draw->AddLine(ImVec2(right_lower_leg.x, right_lower_leg.y), ImVec2(right_foot.x, right_foot.y), color, 1.0f);
        }
    }
}

void draw_text_with_shadow_enhanced(float font_size, const ImVec2& position, ImColor color, const char* text) {
    ImFont* font = ImGui::GetIO().Fonts->Fonts[0];
    if (font) {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                globals::instances::draw->AddText(font, font_size, ImVec2(position.x + i, position.y + j), ImColor(0, 0, 0, 255), text);
            }
        }
        globals::instances::draw->AddText(font, font_size, position, color, text);
    }
    else {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                globals::instances::draw->AddText(0, font_size, ImVec2(position.x + i, position.y + j), ImColor(0, 0, 0, 255), text);
            }
        }
        globals::instances::draw->AddText(0, font_size, position, color, text);
    }
}

void render_offscreen_arrow_enhanced(const Vector3& player_pos, const Vector3& local_pos, ImColor color, const std::string& player_name = "") {
    if (!globals::visuals::oofarrows) return;

    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    ImVec2 screen_center(dimensions.x / 2.f, dimensions.y / 2.f);
    auto view_matrix = globals::instances::visualengine.GetViewMatrix();
    auto camerarotation = globals::instances::camera.getRot();

    Vector3 camera_forward = camerarotation.getColumn(2);
    camera_forward = camera_forward * -1.0f;

    Vector3 dir_to_player = player_pos - local_pos;
    float distance = vector_length(dir_to_player * 0.2);
    if (distance < 1.0f) return;

    dir_to_player.x /= distance;
    dir_to_player.y /= distance;
    dir_to_player.z /= distance;

    Vector3 camera_right = camerarotation.getColumn(0);
    Vector3 camera_up = camerarotation.getColumn(1);

    float forward_dot = dir_to_player.dot(camera_forward);
    if (forward_dot > 0.98f) return;

    float right_dot = dir_to_player.dot(camera_right);
    float up_dot = dir_to_player.dot(camera_up);

    ImVec2 screen_dir(right_dot, -up_dot);
    float screen_dir_len = sqrtf(screen_dir.x * screen_dir.x + screen_dir.y * screen_dir.y);
    if (screen_dir_len < 0.001f) return;

    screen_dir.x /= screen_dir_len;
    screen_dir.y /= screen_dir_len;

    const float radius = 150.0f;
    ImVec2 pos(
        screen_center.x + screen_dir.x * radius,
        screen_center.y + screen_dir.y * radius
    );

    float angle = atan2f(screen_dir.y, screen_dir.x);
    const float arrow_size = 10.0f;
    const float arrow_angle = 0.6f;

    ImVec2 point1(
        pos.x - arrow_size * cosf(angle - arrow_angle),
        pos.y - arrow_size * sinf(angle - arrow_angle)
    );
    ImVec2 point2(
        pos.x - arrow_size * cosf(angle + arrow_angle),
        pos.y - arrow_size * sinf(angle + arrow_angle)
    );

    ImColor arrow_color = ImGui::ColorConvert(globals::visuals::oofcolor);

    if ((*globals::visuals::oof_overlay_flags)[1]) {
        for (int i = -3; i <= 3; i++) {
            for (int j = -3; j <= 3; j++) {
                if (i == 0 && j == 0) continue;
                ImColor glow_color = arrow_color;
                glow_color.Value.w *= 0.3f;
                ImVec2 glow_pos(pos.x + i, pos.y + j);
                ImVec2 glow_point1(point1.x + i, point1.y + j);
                ImVec2 glow_point2(point2.x + i, point2.y + j);
                globals::instances::draw->AddTriangleFilled(glow_pos, glow_point1, glow_point2, glow_color);
            }
        }
    }

    globals::instances::draw->AddTriangleFilled(pos, point1, point2, arrow_color);
    globals::instances::draw->AddTriangle(pos, point1, point2, ImColor(0, 0, 0, 255), 0.5f);

    if (!player_name.empty()) {
        char distance_text[32];
        sprintf_s(distance_text, "%.0fm", distance);

        ImVec2 text_pos(pos.x - 15, pos.y + 15);
        draw_text_with_shadow_enhanced(10.0f, text_pos, arrow_color, distance_text);
    }
}

float cross_product_2d(const ImVec2& p, const ImVec2& q, const ImVec2& r) {
    return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
}

std::vector<ImVec2> convexHull(std::vector<ImVec2>& points) {
    int n = points.size();
    if (n <= 3) return points;

    int bottommost_index = 0;
    for (int i = 1; i < n; ++i) {
        if (points[i].y < points[bottommost_index].y || (points[i].y == points[bottommost_index].y && points[i].x < points[bottommost_index].x)) {
            bottommost_index = i;
        }
    }
    std::swap(points[0], points[bottommost_index]);
    ImVec2 p0 = points[0];

    std::sort(points.begin() + 1, points.end(), [&](const ImVec2& a, const ImVec2& b) {
        float cross = cross_product_2d(p0, a, b);
        if (cross == 0) {
            float distSqA = (p0.x - a.x) * (p0.x - a.x) + (p0.y - a.y) * (p0.y - a.y);
            float distSqB = (p0.x - b.x) * (p0.x - b.x) + (p0.y - b.y) * (p0.y - b.y);
            return distSqA < distSqB;
        }
        return cross > 0;
        });

    std::vector<ImVec2> hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);

    for (int i = 2; i < n; ++i) {
        while (hull.size() > 1 && cross_product_2d(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }

    return hull;
}

Vector3 Rotate(const Vector3& vec, const Matrix3& rotation_matrix) {
    return Vector3{
        vec.x * rotation_matrix.data[0] + vec.y * rotation_matrix.data[1] + vec.z * rotation_matrix.data[2],
        vec.x * rotation_matrix.data[3] + vec.y * rotation_matrix.data[4] + vec.z * rotation_matrix.data[5],
        vec.x * rotation_matrix.data[6] + vec.y * rotation_matrix.data[7] + vec.z * rotation_matrix.data[8]
    };
}

std::vector<Vector3> GetCorners(const Vector3& partCF, const Vector3& partSize) {
    std::vector<Vector3> corners;
    corners.reserve(8);

    for (int X : {-1, 1}) {
        for (int Y : {-1, 1}) {
            for (int Z : {-1, 1}) {
                corners.emplace_back(
                    partCF.x + partSize.x * X,
                    partCF.y + partSize.y * Y,
                    partCF.z + partSize.z * Z
                );
            }
        }
    }
    return corners;
}

namespace esp_helpers {
    bool on_screen(const Vector2& pos) {
        auto dimensions = globals::instances::visualengine.get_dimensions();
        if (dimensions.x == -1 || dimensions.y == -1) return false;
        return pos.x > -1 && pos.x < dimensions.x && pos.y > -1 && pos.y - 28 < dimensions.y;
    }

    bool get_player_bounds(roblox::player& player, Vector2& top_left, Vector2& bottom_right, float& margin) {
        if (!is_valid_address(player.head.address) || (!is_valid_address(player.lefthand.address) && !is_valid_address(player.leftupperarm.address)) ||
            (!is_valid_address(player.righthand.address) && !is_valid_address(player.rightupperarm.address)) ||
            (!is_valid_address(player.leftfoot.address) && !is_valid_address(player.leftupperleg.address)) ||
            (!is_valid_address(player.rightfoot.address) && !is_valid_address(player.rightupperleg.address)))
            return false;

        Vector3 head_pos = player.head.get_pos();
        Vector3 left_hand_pos, right_hand_pos, left_foot_pos, right_foot_pos, hrppos, upper_torso_pos;

        if (player.rigtype == 0) {
            left_hand_pos = player.lefthand.get_pos();
            right_hand_pos = player.righthand.get_pos();
            left_foot_pos = player.leftfoot.get_pos();
            right_foot_pos = player.rightfoot.get_pos();
            upper_torso_pos = player.uppertorso.get_pos();
        }
        else {
            left_hand_pos = player.lefthand.get_pos();
            right_hand_pos = player.righthand.get_pos();
            left_foot_pos = player.leftfoot.get_pos();
            right_foot_pos = player.rightfoot.get_pos();
            upper_torso_pos = player.uppertorso.get_pos();
        }

        std::unordered_map<std::string, Vector2> points;
        points["head"] = roblox::worldtoscreen(head_pos);
        points["torso"] = roblox::worldtoscreen(upper_torso_pos);
        points["rfoot"] = roblox::worldtoscreen(right_foot_pos);
        points["lfoot"] = roblox::worldtoscreen(left_foot_pos);
        points["rhand"] = roblox::worldtoscreen(right_hand_pos);
        points["lhand"] = roblox::worldtoscreen(left_hand_pos);

        float min_x = FLT_MAX, min_y = FLT_MAX;
        float max_x = FLT_MIN, max_y = FLT_MIN;

        for (const auto& pair : points) {
            if (on_screen(pair.second)) {
                min_x = std::min(min_x, pair.second.x);
                min_y = std::min(min_y, pair.second.y);
                max_x = std::max(max_x, pair.second.x);
                max_y = std::max(max_y, pair.second.y);
            }
        }

        float box_margin = std::abs((points["head"].y - points["torso"].y)) * 0.65f;
        min_x = min_x - box_margin;
        min_y = min_y - box_margin;
        max_x = max_x + box_margin;
        max_y = max_y + box_margin;

        top_left = { min_x , min_y };
        bottom_right = { max_x , max_y };

        return true;
    }
}

void visuals::updateESP() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    while (visuals::threadRunning) {
        try {
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            if (!globals::visuals::visuals) {
                std::this_thread::sleep_for(std::chrono::microseconds(7500));
                continue;
            }

            if (!is_valid_address(globals::instances::lp.hrp.address)) {
                std::this_thread::sleep_for(std::chrono::microseconds(7500));
                continue;
            }

            std::vector<CachedPlayerData> tempPlayers;
            Vector3 local_pos = globals::instances::camera.getPos();
            visuals::espData.cachedmutex.lock();
            for (auto& player : globals::instances::cachedplayers) {
                if (!is_valid_address(player.main.address) || player.name == globals::instances::lp.name)
                    continue;

                if (!is_valid_address(player.hrp.address))
                    continue;

                try {
                    CachedPlayerData data;
                    data.player = player;
                    data.position = player.hrp.get_pos();
                    data.distance = (local_pos - data.position).magnitude();

                    if (globals::visuals::nametype == 0)
                        data.name = player.name;
                    else
                        data.name = player.displayname;

                    data.tool_name = player.toolname;
                    if (player.health != 0 && player.maxhealth != 0) {
                        data.health = read<float>(player.humanoid.address + offsets::Health) / player.maxhealth;
                    }
                    else {
                        if (player.maxhealth == 0) {
                            player.maxhealth = read<float>(player.humanoid.address + offsets::MaxHealth);
                        }
                    }

                    Vector2 top_left, bottom_right;
                    float margin;
                    data.valid = esp_helpers::get_player_bounds(player, top_left, bottom_right, margin);

                    if (data.valid) {
                        data.top_left = top_left;
                        data.bottom_right = bottom_right;
                        data.margin = margin;
                        tempPlayers.push_back(data);
                    }
                }
                catch (...) {
                    continue;
                }
            }

            if (!tempPlayers.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                std::sort(tempPlayers.begin(), tempPlayers.end(),
                    [](const CachedPlayerData& a, const CachedPlayerData& b) {
                        return a.distance < b.distance;
                    });

                visuals::espData.cachedPlayers = std::move(tempPlayers);
                visuals::espData.localPos = local_pos;
            }
        }
        catch (...) {
        }
        visuals::espData.cachedmutex.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

struct Polygon_t {
    std::vector<ImVec2> vertices;
    float depth;
};
static std::mutex chamsMutex;
static std::vector<Polygon_t> polys;

void utils::CacheChamsPoly() {
    while (true) {
        if (!globals::visuals::visuals || !globals::visuals::chams) {
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            continue;
        }

        std::vector<Polygon_t> temp_polygons;
        auto& playerlist = globals::instances::cachedplayers;
        if (playerlist.empty()) {
            std::this_thread::sleep_for(std::chrono::microseconds(5000));
            continue;
        }
        auto& local_player = playerlist[0];

        for (roblox::player& player : playerlist) {
            if (!local_player.main.address) continue;
            if (player.main.address == local_player.main.address) continue;

            std::vector<roblox::instance> parts_chams;

            switch (player.rigtype) {
            case 0:
                parts_chams = {
                    player.head,
                    player.uppertorso,
                    player.leftfoot,
                    player.rightfoot,
                    player.lefthand,
                    player.righthand
                };
                break;
            case 1:
                parts_chams = {
                    player.head,
                    player.uppertorso,
                    player.lowertorso,
                    player.leftupperarm,
                    player.rightupperarm,
                    player.leftlowerleg,
                    player.rightlowerleg,
                    player.leftfoot,
                    player.rightfoot,
                    player.leftupperleg,
                    player.rightupperleg,
                    player.leftlowerarm,
                    player.rightlowerarm,
                    player.lefthand,
                    player.righthand
                };
                break;
            default:
                continue;
            }

            for (auto& part : parts_chams) {
                try {
                    Vector3 part_size = part.get_part_size();
                    Vector3 part_pos = part.get_pos();
                    Matrix3 part_rotation = part.read_cframe().rotation_matrix;

                    std::vector<Vector3> CubeVertices = GetCorners(
                        { 0, 0, 0 },
                        { part_size.x / 2, part_size.y / 2, part_size.z / 2 }
                    );

                    for (auto& vertex : CubeVertices) {
                        vertex = Rotate(vertex, part_rotation);
                        vertex = { vertex.x + part_pos.x, vertex.y + part_pos.y, vertex.z + part_pos.z };
                    }

                    std::vector<ImVec2> screen_vertices;
                    screen_vertices.reserve(8);
                    float total_depth = 0;
                    bool valid_polygon = true;

                    for (const auto& vertex : CubeVertices) {
                        Vector2 screen_pos = roblox::worldtoscreen(vertex);
                        if (!esp_helpers::on_screen(screen_pos)) {
                            valid_polygon = false;
                            break;
                        }

                        screen_vertices.emplace_back(ImVec2(screen_pos.x, screen_pos.y));
                        total_depth += vertex.z;
                    }

                    if (!valid_polygon || screen_vertices.size() < 3)
                        continue;

                    std::vector<ImVec2> hull = convexHull(screen_vertices);

                    if (hull.size() >= 3) {
                        temp_polygons.emplace_back(Polygon_t{ hull, total_depth / CubeVertices.size() });
                    }
                }
                catch (...) {
                    continue;
                }
            }
        }

        if (!temp_polygons.empty()) {
            std::sort(temp_polygons.begin(), temp_polygons.end(), [](const Polygon_t& a, const Polygon_t& b) {
                return a.depth > b.depth;
                });
            std::lock_guard<std::mutex> lock(chamsMutex);
            polys = std::move(temp_polygons);
        }
        else {
            std::lock_guard<std::mutex> lock(chamsMutex);
            polys.clear();
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

void render_chams_enhanced() {
    if (!globals::visuals::chams || !globals::instances::draw)
        return;

    std::vector<Polygon_t> polygons_to_draw;
    {
        std::lock_guard<std::mutex> lock(chamsMutex);
        polygons_to_draw = polys;
    }

    if (polygons_to_draw.empty())
        return;

    globals::instances::draw->_FringeScale = 0.f;
    ImDrawListFlags oldFlags = globals::instances::draw->Flags;

    ImColor fillColor(
        globals::visuals::chamscolor[0],
        globals::visuals::chamscolor[1],
        globals::visuals::chamscolor[2],
        globals::visuals::chamscolor[3]
    );

    ImColor outlineColor(0.0f, 0.0f, 0.0f, globals::visuals::chamscolor[3]);

    for (const auto& polygon : polygons_to_draw) {
        if (polygon.vertices.size() < 3)
            continue;

        if ((*globals::visuals::chams_overlay_flags)[1]) {
            ImColor glow_color = fillColor;
            glow_color.Value.w *= 0.5f;
            globals::instances::draw->AddShadowConvexPoly(
                polygon.vertices.data(),
                polygon.vertices.size(),
                glow_color,
                10.f,
                ImVec2(0, 0)
            );
        }
        if ((*globals::visuals::chams_overlay_flags)[2]) {
            globals::instances::draw->AddConvexPolyFilled(
                polygon.vertices.data(),
                polygon.vertices.size(),
                fillColor
            );
        }
        if ((*globals::visuals::chams_overlay_flags)[0]) {
            globals::instances::draw->AddPolyline(
                polygon.vertices.data(),
                polygon.vertices.size(),
                outlineColor,
                true,
                1.5f
            );
        }
    }
    globals::instances::draw->Flags = oldFlags;
}

void visuals::stopThread() {
    if (threadRunning) {
        threadRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
}

void visuals::run() {
    if (!threadRunning) {
        updateThread = std::thread(visuals::updateESP);
        updateThread.detach();
        std::thread updatechams(utils::CacheChamsPoly);
        updatechams.detach();
        threadRunning = true;
    }

    if (!globals::visuals::visuals || !globals::instances::draw) {
        return;
    }

    auto screen_dimensions = globals::instances::visualengine.get_dimensions();
    if (screen_dimensions.x == 0 || screen_dimensions.y == 0) {
        return;
    }

    render_chams_enhanced();
    visuals::espData.cachedmutex.lock();

    for (auto& player : visuals::espData.cachedPlayers) {
        if (!player.valid)
            continue;

        Vector2 screen_check = roblox::worldtoscreen(player.position);
        if (screen_check.x <= 0 || screen_check.x >= screen_dimensions.x ||
            screen_check.y <= 0 || screen_check.y >= screen_dimensions.y) {
            if (player.distance > 100.0f) {
                continue;
            }
        }

        Vector2 top_left = player.top_left;
        Vector2 bottom_right = player.bottom_right;

        if (!esp_helpers::on_screen(top_left) || !esp_helpers::on_screen(bottom_right))
            continue;

        ImColor player_color = ImColor(255, 255, 255);

        if (globals::visuals::boxes) {
            player_color = ImGui::ColorConvert(globals::visuals::boxcolors);
            if (globals::instances::cachedtarget.name == player.name)
                player_color = ImColor(255, 0, 0);

            render_box_enhanced(top_left, bottom_right, player_color);
        }

        if (globals::visuals::name) {
            render_name_enhanced(player.name, top_left, bottom_right);
        }

        if (globals::visuals::distance) {
            render_distance_enhanced(player.distance, top_left, bottom_right, !player.tool_name.empty() && globals::visuals::toolesp);
        }

        if (globals::visuals::toolesp && !player.tool_name.empty()) {
            render_tool_name_enhanced(player.tool_name, top_left, bottom_right);
        }

        if (globals::visuals::snapline) {
            render_snapline_enhanced(screen_check);
        }

        if (globals::visuals::healthbar) {
            render_health_bar_enhanced(top_left, bottom_right, player.health, ImColor(255, 0, 0));
        }

        if (globals::visuals::skeletons) {
            render_skeleton_enhanced(player, ImGui::ColorConvert(globals::visuals::skeletonscolor));
        }
    }

    for (auto& player : visuals::espData.cachedPlayers) {
        if (!player.valid) continue;

        Vector2 screen_pos = roblox::worldtoscreen(player.position);
        auto dimensions = globals::instances::visualengine.get_dimensions();

        bool is_offscreen = screen_pos.x <= 0 || screen_pos.x >= dimensions.x ||
            screen_pos.y <= 0 || screen_pos.y >= dimensions.y;

        if (is_offscreen && globals::visuals::oofarrows) {
            render_offscreen_arrow_enhanced(player.position, visuals::espData.localPos,
                ImGui::ColorConvert(globals::visuals::oofcolor), player.name);
        }
    }

    visuals::espData.cachedmutex.unlock();
}