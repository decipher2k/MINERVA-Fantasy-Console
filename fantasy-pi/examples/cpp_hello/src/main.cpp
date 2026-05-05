#include <fantasy_engine.h>

uint32_t color = 0xFF204060;

int main() {
    fe::Engine_Init();

    while (true) {
        fe::Gfx_Clear(color);
        fe::Gfx_Present();
    }

    return 0;
}

