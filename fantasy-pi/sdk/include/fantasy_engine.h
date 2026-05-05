#pragma once

#include <stdint.h>

namespace fe {

struct Vec2 {
    int32_t x;
    int32_t y;
};

struct Rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

struct GameObject {
    uint16_t type_id;
    uint16_t instance_id;
    Vec2 pos;
    Vec2 vel;
    Rect collider;
    uint32_t sprite_id;
    uint32_t anim_id;
    uint32_t state;
    uint32_t flags;
    void* user_data;
};

using GO_InitFunc = void (*)(GameObject* self);
using GO_UpdateFunc = void (*)(GameObject* self);
using GO_DrawFunc = void (*)(GameObject* self);
using GO_CollisionFunc = void (*)(GameObject* self, GameObject* other);
using GO_InteractFunc = void (*)(GameObject* self, GameObject* other);

enum BlendMode : uint32_t {
    BLEND_COPY = 0,
    BLEND_ALPHA = 1,
    BLEND_ADDITIVE = 2,
    BLEND_MULTIPLY = 3,
    BLEND_COLORKEY = 4,
    BLEND_MASK = 5
};

enum GamepadButton : uint32_t {
    BTN_UP = 1u << 0,
    BTN_DOWN = 1u << 1,
    BTN_LEFT = 1u << 2,
    BTN_RIGHT = 1u << 3,
    BTN_A = 1u << 4,
    BTN_B = 1u << 5,
    BTN_START = 1u << 6,
    BTN_SELECT = 1u << 7
};

enum GamepadAxis : uint8_t {
    AXIS_LX = 0,
    AXIS_LY = 1,
    AXIS_RX = 2,
    AXIS_RY = 3
};

void Engine_Init();
void Engine_LoadScene(uint32_t mapId);
void Engine_PollInput();
void Engine_Update();
void Engine_Draw();
void Engine_PlaySFX(uint32_t sampleId, uint8_t channel);

void Gfx_Clear(uint32_t argb);
void Gfx_Present();
void Image_Draw(uint32_t id, int32_t x, int32_t y, BlendMode mode = BLEND_COPY);

void Sprite_Draw(uint32_t id, int32_t x, int32_t y, uint8_t flip = 0);
void Sprite_DrawEx(uint32_t id, int32_t x, int32_t y, BlendMode mode, uint8_t alpha = 255);
void Tilemap_DrawLayer(uint32_t layerId, int32_t scrollX, int32_t scrollY);
void Palette_SetColor(uint8_t palette, uint8_t index, uint32_t argb);

bool Input_IsPressed(uint32_t button);
bool Input_IsHeld(uint32_t button);
bool Input_IsReleased(uint32_t button);
uint32_t Input_GamepadButtons(uint8_t player);
int16_t Input_GamepadAxis(uint8_t player, uint8_t axis);

void Audio_Init();
void Audio_Play(uint32_t sampleId, uint8_t channel, uint8_t volume = 255);
void Audio_Stop(uint8_t channel);
void Audio_SetVolume(uint8_t channel, uint8_t volume);

int32_t fp_mul(int32_t a, int32_t b);
int32_t fp_div(int32_t a, int32_t b);
int32_t fp_from_int(int32_t i);
int32_t fp_to_int(int32_t f);
uint32_t Math_Rand(uint32_t seed = 0);
int32_t Math_Sin(uint32_t angle);
int32_t Math_Cos(uint32_t angle);

void Debug_Log(uint32_t value);
void Debug_LogString(const char* text);

} // namespace fe

using fe::GameObject;
using fe::Vec2;
using fe::Rect;
using fe::GO_InitFunc;
using fe::GO_UpdateFunc;
using fe::GO_DrawFunc;
using fe::GO_CollisionFunc;
using fe::GO_InteractFunc;
