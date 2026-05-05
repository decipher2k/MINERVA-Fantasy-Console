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

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbgamepad.h>
#include <circle/input/mouse.h>
#include <SDCard/emmc.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/gpiopin.h>
#include <circle/sound/hdmisoundbasedevice.h>

#include "fantasy_vm.h"
#include "renderer.h"
#include "audio_mixer.h"

enum TShutdownMode {
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

// Input state layout in VM RAM (see INPUT_STATE_ADDR)
struct InputState {
    uint32_t keyboard_keys[8];      // 256 key bits
    uint32_t gamepad_buttons[2];    // 2 players, 32 bits each
    int16_t  gamepad_axis[2][4];    // 2 players, 4 axes each (LX, LY, RX, RY)
    uint32_t mouse_x;
    uint32_t mouse_y;
    uint32_t mouse_buttons;
};

class CKernel {
public:
    CKernel(void);
    ~CKernel(void);
    boolean Initialize(void);
    TShutdownMode Run(void);

    // Trap callback entry point (static wrapper calls instance method)
    static bool VMTrapHandler(uint32_t vector, uint32_t* regs, void* user_data);
    bool HandleVMTrap(uint32_t vector, uint32_t* regs);

private:
    static void KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]);
    static void GamepadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);
    static void GamepadRemovedHandler(CDevice *pDevice, void *pContext);
    void RegisterGamepads(void);
    void UpdateInputState(void);
    bool LoadCartridge(const char* filename);
    bool LoadEmbeddedCartridge(void);
    void ReleaseCartridgeData(void);
    void ClearTileMapStorage(void);
    void LoadAssetsFromCartridge(const uint8_t* data, size_t size);
    void LoadImageLikeAsset(uint32_t id, uint32_t type, uint32_t aux0, uint32_t aux1,
                            const uint8_t* asset, uint32_t asset_size);
    void LoadTileMapAsset(uint32_t id, uint32_t tileset_id,
                          const uint8_t* asset, uint32_t asset_size);
    void CopyFrameBufferToScreen(void);
    void InitInputState(void);

private:
    CActLED m_ActLED;
    CKernelOptions m_Options;
    CDeviceNameService m_DeviceNameService;
    CScreenDevice m_Screen;
    CSerialDevice m_Serial;
    CExceptionHandler m_ExceptionHandler;
    CInterruptSystem m_Interrupt;
    CTimer m_Timer;
    CLogger m_Logger;
    CUSBHCIDevice m_USBHCI;
    CEMMCDevice m_EMMC;
    CFATFileSystem m_FileSystem;

    CSoundBaseDevice* m_pSoundDevice;

    fantasy::FantasyVM m_VM;
    fantasy::Renderer m_Renderer;
    fantasy::AudioMixer m_AudioMixer;

    // Input state mirrors
    InputState m_InputState;
    CUSBKeyboardDevice* m_pKeyboard;
    CUSBGamePadDevice* volatile m_pGamePad[2];
    TGamePadState m_GamePadState;

    uint8_t* m_pCartridgeData;
    size_t m_nCartridgeSize;
    boolean m_bOwnCartridgeData;
    uint8_t* m_pTileMapData[16];
    unsigned m_nTileMapDataCount;

    static CKernel *s_pThis;
};
