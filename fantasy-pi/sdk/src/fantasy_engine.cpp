#include <fantasy_engine.h>

namespace fe {

void Engine_Init() {}
void Engine_LoadScene(uint32_t) {}
void Engine_PollInput() {}
void Engine_Update() {}
void Engine_Draw() {}
void Engine_PlaySFX(uint32_t sampleId, uint8_t channel) { Audio_Play(sampleId, channel, 255); }

void Gfx_Clear(uint32_t) {}
void Gfx_Present() {}
void Image_Draw(uint32_t, int32_t, int32_t, BlendMode) {}

void Sprite_Draw(uint32_t, int32_t, int32_t, uint8_t) {}
void Sprite_DrawEx(uint32_t, int32_t, int32_t, BlendMode, uint8_t) {}
void Tilemap_DrawLayer(uint32_t, int32_t, int32_t) {}
void Palette_SetColor(uint8_t, uint8_t, uint32_t) {}

bool Input_IsPressed(uint32_t) { return false; }
bool Input_IsHeld(uint32_t) { return false; }
bool Input_IsReleased(uint32_t) { return false; }
uint32_t Input_GamepadButtons(uint8_t) { return 0; }
int16_t Input_GamepadAxis(uint8_t, uint8_t) { return 0; }

void Audio_Init() {}
void Audio_Play(uint32_t, uint8_t, uint8_t) {}
void Audio_Stop(uint8_t) {}
void Audio_SetVolume(uint8_t, uint8_t) {}

int32_t fp_mul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
int32_t fp_div(int32_t a, int32_t b) { return b == 0 ? 0 : (int32_t)(((int64_t)a << 16) / b); }
int32_t fp_from_int(int32_t i) { return i << 16; }
int32_t fp_to_int(int32_t f) { return f >> 16; }
uint32_t Math_Rand(uint32_t seed) { return seed; }
int32_t Math_Sin(uint32_t) { return 0; }
int32_t Math_Cos(uint32_t) { return 0; }

void Debug_Log(uint32_t) {}
void Debug_LogString(const char*) {}

} // namespace fe
