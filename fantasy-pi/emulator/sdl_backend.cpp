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

#include "sdl_backend.h"
#include <cstring>

// SDL2 includes
#include <SDL2/SDL.h>

namespace fantasy {

SDLBackend::SDLBackend()
    : width_(640), height_(480), quit_(false),
      window_(nullptr), renderer_(nullptr), texture_(nullptr),
      controller_(nullptr), key_state_(nullptr)
{
    memset(gamepad_buttons_, 0, sizeof(gamepad_buttons_));
    memset(gamepad_axis_, 0, sizeof(gamepad_axis_));
}

SDLBackend::~SDLBackend() {
    Shutdown();
}

bool SDLBackend::Init(int width, int height, const char* title) {
    if (window_ || renderer_ || texture_) {
        Shutdown();
    }

    width_ = width;
    height_ = height;
    quit_ = false;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        return false;
    }

    window_ = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width_, height_, SDL_WINDOW_SHOWN);
    if (!window_) {
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, width_, height_);
    if (!texture_) {
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    key_state_ = new uint8_t[SDL_NUM_SCANCODES];
    memset(key_state_, 0, SDL_NUM_SCANCODES);
    memset(gamepad_buttons_, 0, sizeof(gamepad_buttons_));
    memset(gamepad_axis_, 0, sizeof(gamepad_axis_));
    OpenFirstController();
    return true;
}

void SDLBackend::Shutdown() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    delete[] key_state_;
    key_state_ = nullptr;
    SDL_Quit();
}

void SDLBackend::Update(const Surface* framebuffer) {
    if (!framebuffer || !framebuffer->pixels) return;

    SDL_UpdateTexture(texture_, nullptr, framebuffer->pixels, framebuffer->pitch);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool SDLBackend::ProcessEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                quit_ = true;
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.scancode < SDL_NUM_SCANCODES) {
                    key_state_[e.key.keysym.scancode] = 1;
                }
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    quit_ = true;
                }
                break;
            case SDL_KEYUP:
                if (e.key.keysym.scancode < SDL_NUM_SCANCODES) {
                    key_state_[e.key.keysym.scancode] = 0;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!controller_) {
                    OpenFirstController();
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (controller_) {
                    SDL_GameControllerClose(controller_);
                    controller_ = nullptr;
                }
                OpenFirstController();
                break;
        }
    }
    UpdateControllerState();
    return !quit_;
}

bool SDLBackend::IsKeyDown(int scancode) const {
    if (!key_state_ || scancode < 0 || scancode >= SDL_NUM_SCANCODES) return false;
    return key_state_[scancode] != 0;
}

uint32_t SDLBackend::GetGamepadButtons(int index) const {
    if (index < 0 || index >= 2) return 0;
    return gamepad_buttons_[index];
}

int16_t SDLBackend::GetGamepadAxis(int index, int axis) const {
    if (index < 0 || index >= 2 || axis < 0 || axis >= 4) return 0;
    return gamepad_axis_[index][axis];
}

void SDLBackend::OpenFirstController() {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) return;
        }
    }
}

void SDLBackend::UpdateControllerState() {
    gamepad_buttons_[0] = 0;
    memset(gamepad_axis_[0], 0, sizeof(gamepad_axis_[0]));

    if (!controller_) return;

    const int deadzone = 8000;
    int16_t lx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t ly = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY);
    int16_t rx = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t ry = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY);

    gamepad_axis_[0][0] = (lx > -deadzone && lx < deadzone) ? 0 : lx;
    gamepad_axis_[0][1] = (ly > -deadzone && ly < deadzone) ? 0 : ly;
    gamepad_axis_[0][2] = (rx > -deadzone && rx < deadzone) ? 0 : rx;
    gamepad_axis_[0][3] = (ry > -deadzone && ry < deadzone) ? 0 : ry;

    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_UP) || gamepad_axis_[0][1] < -deadzone) gamepad_buttons_[0] |= 1U << 0;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || gamepad_axis_[0][1] > deadzone) gamepad_buttons_[0] |= 1U << 1;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || gamepad_axis_[0][0] < -deadzone) gamepad_buttons_[0] |= 1U << 2;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || gamepad_axis_[0][0] > deadzone) gamepad_buttons_[0] |= 1U << 3;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A)) gamepad_buttons_[0] |= 1U << 4;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_B)) gamepad_buttons_[0] |= 1U << 5;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_START)) gamepad_buttons_[0] |= 1U << 6;
    if (SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_BACK)) gamepad_buttons_[0] |= 1U << 7;
}

} // namespace fantasy
