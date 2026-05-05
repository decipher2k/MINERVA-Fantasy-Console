#include <fantasy_engine.h>
#include "asset_ids.h"

static uint32_t sky_color = 0xFF87CEEB;
static int32_t player_x = 320;
static int32_t player_y = 220;
static int32_t player_vx = 0;
static int32_t player_vy = 0;
static int32_t player_ground = 400;
static uint32_t input_state = 0;
static uint32_t frame_count = 0;

void read_input()
{
    fe::Engine_PollInput();
    input_state = fe::Input_GamepadButtons(0);
}

void update_player()
{
    player_vx = player_vx / 2;

    if (input_state & fe::BTN_LEFT) {
        player_vx = player_vx - 2;
    }

    if (input_state & fe::BTN_RIGHT) {
        player_vx = player_vx + 2;
    }

    if (player_vx > 8) {
        player_vx = 8;
    }

    if (player_vx < -8) {
        player_vx = -8;
    }

    player_vy = player_vy + 1;
    if (player_vy > 8) {
        player_vy = 8;
    }

    player_x = player_x + player_vx;
    player_y = player_y + player_vy;

    if (player_y >= player_ground) {
        player_y = player_ground;
        player_vy = 0;
        if (input_state & fe::BTN_A) {
            player_vy = -12;
            fe::Audio_Play(ASSET_SFX_JUMP, 0, 128);
        }
    }
}

void draw_frame()
{
    fe::Gfx_Clear(sky_color);
    fe::Image_Draw(ASSET_BG_MAIN, 0, 0, fe::BLEND_COPY);
    fe::Tilemap_DrawLayer(ASSET_MAP_LEVEL1, 0, 0);
    fe::Sprite_DrawEx(ASSET_SHADOW, player_x - 8, player_y + 8, fe::BLEND_ALPHA);
    fe::Sprite_DrawEx(ASSET_PLAYER, player_x, player_y, fe::BLEND_ALPHA);
    fe::Gfx_Present();
}

int main()
{
    fe::Engine_Init();

    for (;;) {
        read_input();
        update_player();
        draw_frame();
        frame_count = frame_count + 1;
    }

    return 0;
}
