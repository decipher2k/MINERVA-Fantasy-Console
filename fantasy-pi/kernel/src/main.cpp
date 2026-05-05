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

#include "kernel.h"
#include <circle/startup.h>

extern "C" int printf(const char*, ...) {
    return 0;
}

int main(void) {
    CKernel kernel;
    if (!kernel.Initialize()) {
        halt();
        return EXIT_HALT;
    }

    TShutdownMode shutdown_mode = kernel.Run();
    switch (shutdown_mode) {
    case ShutdownReboot:
        reboot();
        return EXIT_REBOOT;
    case ShutdownHalt:
    default:
        halt();
        return EXIT_HALT;
    }
}
