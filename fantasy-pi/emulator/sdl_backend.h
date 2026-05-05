/*
Copyright 2026 Dennis Michael Heine

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <cstdint>
#include "renderer.h"

// SDL2 backend for emulator
// Requires SDL2 development libraries on host system

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

namespace fantasy {

class SDLBackend {
public:
    SDLBackend();
    ~SDLBackend();

    bool Init(int width, int height, const char* title = "Fantasy Pi Emulator");
    void Shutdown();
    void Update(const Surface* framebuffer);
    bool ProcessEvents();
    bool ShouldQuit() const { return quit_; }

    // Keyboard state query for emulator input bridge
    bool IsKeyDown(int scancode) const;
    uint32_t GetGamepadButtons(int index) const;
    int16_t GetGamepadAxis(int index, int axis) const;

private:
    void OpenFirstController();
    void UpdateControllerState();

    int width_;
    int height_;
    bool quit_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    SDL_GameController* controller_;
    uint8_t* key_state_;
    uint32_t gamepad_buttons_[2];
    int16_t gamepad_axis_[2][4];
};

} // namespace fantasy
