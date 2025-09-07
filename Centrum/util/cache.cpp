#include "../util/classes/classes.h"
#include "../features/hook.h"
#include "globals.h"
#include "../drawing/overlay/overlay.h"

namespace freecam {
    Vector3 camera_position;
    Matrix3 camera_rotation;
    float movement_speed = 50.0f;
    float mouse_sensitivity = 0.002f;
    bool last_mouse_state = false;
    POINT screen_center;
    float pitch = 0.0f;
    float yaw = 0.0f;
    bool center_initialized = false;
}

static Vector3 lookvec(const Matrix3& rotationMatrix) {
    return rotationMatrix.getColumn(2);
}

static Vector3 rightvec(const Matrix3& rotationMatrix) {
    return rotationMatrix.getColumn(0);
}

bool is_key_pressed(int vkey) {
    return (GetAsyncKeyState(vkey) & 0x8000) != 0;
}

Matrix3 create_rotation_matrix(float pitch, float yaw) {
    Matrix3 result;

    float cos_pitch = cos(pitch);
    float sin_pitch = sin(pitch);
    float cos_yaw = cos(yaw);
    float sin_yaw = sin(yaw);

    result.data[0] = cos_yaw;
    result.data[1] = sin_yaw * sin_pitch;
    result.data[2] = sin_yaw * cos_pitch;
    result.data[3] = 0;
    result.data[4] = cos_pitch;
    result.data[5] = -sin_pitch;
    result.data[6] = -sin_yaw;
    result.data[7] = cos_yaw * sin_pitch;
    result.data[8] = cos_yaw * cos_pitch;

    return result;
}

void update_mouse_look() {
    if (!freecam::center_initialized) {
        RECT screen_rect;
        GetWindowRect(GetDesktopWindow(), &screen_rect);
        freecam::screen_center.x = (screen_rect.right - screen_rect.left) / 2;
        freecam::screen_center.y = (screen_rect.bottom - screen_rect.top) / 2;
        freecam::center_initialized = true;
    }

    POINT current_mouse;
    GetCursorPos(&current_mouse);

    if (!freecam::last_mouse_state) {
        SetCursorPos(freecam::screen_center.x, freecam::screen_center.y);
        ShowCursor(FALSE);
        freecam::last_mouse_state = true;
        return;
    }

    int delta_x = current_mouse.x - freecam::screen_center.x;
    int delta_y = current_mouse.y - freecam::screen_center.y;

    if (delta_x != 0 || delta_y != 0) {
        freecam::yaw -= delta_x * freecam::mouse_sensitivity;
        freecam::pitch -= delta_y * freecam::mouse_sensitivity;

        if (freecam::pitch > 1.5f) freecam::pitch = 1.5f;
        if (freecam::pitch < -1.5f) freecam::pitch = -1.5f;

        freecam::camera_rotation = create_rotation_matrix(freecam::pitch, freecam::yaw);

        SetCursorPos(freecam::screen_center.x, freecam::screen_center.y);
    }
}

void update_movement() {
    Vector3 look_vector = lookvec(freecam::camera_rotation);
    Vector3 right_vector = rightvec(freecam::camera_rotation);
    Vector3 up_vector = { 0.0f, 1.0f, 0.0f };
    Vector3 direction = { 0.0f, 0.0f, 0.0f };

    if (is_key_pressed('W')) {
        direction = direction - look_vector;
    }
    if (is_key_pressed('S')) {
        direction = direction + look_vector;
    }
    if (is_key_pressed('A')) {
        direction = direction - right_vector;
    }
    if (is_key_pressed('D')) {
        direction = direction + right_vector;
    }
    if (is_key_pressed(VK_SPACE)) {
        direction = direction + up_vector;
    }
    if (is_key_pressed(VK_LSHIFT)) {
        direction = direction - up_vector;
    }

    if (direction.magnitude() > 0) {
        direction = direction.normalize();
    }

    float speed_multiplier = is_key_pressed(VK_LCONTROL) ? 2.0f : 1.0f;
    float move_speed = freecam::movement_speed * speed_multiplier;

    freecam::camera_position = freecam::camera_position + (direction * (move_speed / 100.0f));
}

void hooks::cache() {
    while (true) {
        globals::instances::mouseservice = globals::instances::datamodel.findfirstchild("MouseService").address;
        globals::misc::spectatebind.update();

        static bool freecammed;
        if (globals::misc::spectatebind.enabled && globals::misc::spectate) {
            if (!freecammed) {
                globals::instances::lp.hrp.setAnchored(true);
              
                globals::instances::camera.setType(6);
                freecam::camera_position = globals::instances::camera.getPos();
                freecam::camera_rotation = globals::instances::camera.getRot();
                freecam::last_mouse_state = false;

                freecammed = true;
            }
            if (globals::focused) {
                if (!overlay::visible) {
                    update_mouse_look();
                    update_movement();
                }
           
            }
            globals::instances::localplayer.spectate(0x0);
            for (int i = 0; i < 100; i++) {
                globals::instances::camera.writePos(freecam::camera_position);
                globals::instances::camera.writeRot(freecam::camera_rotation);
            }
      
            
        }
        else {
            if (freecammed) {
                globals::instances::lp.hrp.setAnchored(false);
                globals::instances::localplayer.unspectate();
                freecam::last_mouse_state = false;
                ShowCursor(TRUE);
                freecammed = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}