cmake_minimum_required(VERSION 3.20)
project(fantasy-pi VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(FANTASY_PI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# Host build options (for emulator and tools)
option(BUILD_HOST_EMULATOR "Build host emulator" ON)
option(BUILD_TOOLS "Build assembler and asset compiler" ON)
option(BUILD_TESTS "Build unit tests" ON)

# Find packages for host builds
if(BUILD_HOST_EMULATOR OR BUILD_TOOLS)
    find_package(Threads REQUIRED)
endif()

if(BUILD_HOST_EMULATOR)
    # Try standard CMake find first
    find_package(SDL2 QUIET)

    # Fallback: manual search on Windows / common paths
    if(NOT SDL2_FOUND)
        if(WIN32)
            # Check SDL2_DIR environment variable
            if(DEFINED ENV{SDL2_DIR})
                set(_SDL2_DIR "$ENV{SDL2_DIR}")
                find_package(SDL2 PATHS "${_SDL2_DIR}" NO_DEFAULT_PATH QUIET)
            endif()

            # Search common installation paths manually
            if(NOT SDL2_FOUND)
                foreach(_SDL2_ROOT
                    "C:/SDL2"
                    "C:/vcpkg/installed/x64-windows"
                    "$ENV{LOCALAPPDATA}/vcpkg/installed/x64-windows"
                    "$ENV{USERPROFILE}/vcpkg/installed/x64-windows"
                )
                    if(EXISTS "${_SDL2_ROOT}/include/SDL2/SDL.h")
                        set(SDL2_INCLUDE_DIRS "${_SDL2_ROOT}/include")
                        find_library(SDL2_LIBRARY SDL2 PATHS
                            "${_SDL2_ROOT}/lib"
                            "${_SDL2_ROOT}/lib/x64"
                            "${_SDL2_ROOT}/lib/x86"
                            NO_DEFAULT_PATH
                        )
                        if(SDL2_LIBRARY)
                            set(SDL2_FOUND TRUE)
                            set(SDL2_LIBRARIES ${SDL2_LIBRARY})
                            message(STATUS "Found SDL2 manually: ${_SDL2_ROOT}")
                            break()
                        endif()
                    endif()
                endforeach()
            endif()
        endif()
    endif()

    if(SDL2_FOUND)
        message(STATUS "SDL2 found - emulator will have GUI support")
    else()
        message(WARNING "SDL2 not found. Building headless emulator. Set SDL2_DIR env var or install SDL2.")
    endif()
endif()

# Common include directories
include_directories(
    ${FANTASY_PI_ROOT}/vm/include
    ${FANTASY_PI_ROOT}/renderer/include
    ${FANTASY_PI_ROOT}/emulator
)

# VM library (host build)
add_library(fantasy_vm_host STATIC
    ${FANTASY_PI_ROOT}/vm/src/fantasy_vm.cpp
)
target_include_directories(fantasy_vm_host PUBLIC
    ${FANTASY_PI_ROOT}/vm/include
)

# Renderer library (host build)
add_library(fantasy_renderer_host STATIC
    ${FANTASY_PI_ROOT}/renderer/src/renderer.cpp
    ${FANTASY_PI_ROOT}/renderer/src/audio_mixer.cpp
)
target_include_directories(fantasy_renderer_host PUBLIC
    ${FANTASY_PI_ROOT}/renderer/include
)

# Host emulator
if(BUILD_HOST_EMULATOR)
    set(EMULATOR_SRCS ${FANTASY_PI_ROOT}/emulator/main.cpp)
    if(SDL2_FOUND)
        list(APPEND EMULATOR_SRCS ${FANTASY_PI_ROOT}/emulator/sdl_backend.cpp)
    endif()
    add_executable(fantasy_emulator ${EMULATOR_SRCS})
    target_link_libraries(fantasy_emulator
        fantasy_vm_host
        fantasy_renderer_host
        Threads::Threads
    )
    if(SDL2_FOUND)
        if(TARGET SDL2::SDL2)
            target_link_libraries(fantasy_emulator SDL2::SDL2)
        else()
            target_include_directories(fantasy_emulator PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(fantasy_emulator ${SDL2_LIBRARIES})
        endif()
        target_compile_definitions(fantasy_emulator PRIVATE HAS_SDL2=1 SDL_MAIN_HANDLED)
    else()
        message(WARNING "SDL2 not found. Building headless emulator.")
    endif()

    if(WIN32)
        set(EMULATOR_RUNTIME_DLLS)

        if(SDL2_FOUND)
            set(_SDL2_DLL)
            if(TARGET SDL2::SDL2)
                get_target_property(_SDL2_DLL SDL2::SDL2 IMPORTED_LOCATION)
                if(NOT _SDL2_DLL)
                    get_target_property(_SDL2_DLL SDL2::SDL2 IMPORTED_LOCATION_RELEASE)
                endif()
            endif()
            if(NOT _SDL2_DLL AND SDL2_BINDIR AND EXISTS "${SDL2_BINDIR}/SDL2.dll")
                set(_SDL2_DLL "${SDL2_BINDIR}/SDL2.dll")
            endif()
            if(_SDL2_DLL AND EXISTS "${_SDL2_DLL}")
                list(APPEND EMULATOR_RUNTIME_DLLS "${_SDL2_DLL}")
            endif()
        endif()

        if(MINGW)
            get_filename_component(_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(_MINGW_DLL
                libgcc_s_seh-1.dll
                libstdc++-6.dll
                libwinpthread-1.dll
                libmcfgthread-2.dll
            )
                if(EXISTS "${_MINGW_BIN_DIR}/${_MINGW_DLL}")
                    list(APPEND EMULATOR_RUNTIME_DLLS "${_MINGW_BIN_DIR}/${_MINGW_DLL}")
                endif()
            endforeach()
        endif()

        if(EMULATOR_RUNTIME_DLLS)
            list(REMOVE_DUPLICATES EMULATOR_RUNTIME_DLLS)
            add_custom_command(TARGET fantasy_emulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${EMULATOR_RUNTIME_DLLS}
                    $<TARGET_FILE_DIR:fantasy_emulator>
                COMMAND_EXPAND_LISTS
                VERBATIM
                COMMENT "Copying emulator runtime DLLs"
            )
        endif()

        set(EMULATOR_RUN_DIR "${FANTASY_PI_ROOT}/build")
        add_custom_command(TARGET fantasy_emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${EMULATOR_RUN_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:fantasy_emulator>
                "${EMULATOR_RUN_DIR}"
            VERBATIM
            COMMENT "Copying emulator to ROM build directory"
        )
        if(EMULATOR_RUNTIME_DLLS)
            add_custom_command(TARGET fantasy_emulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${EMULATOR_RUNTIME_DLLS}
                    "${EMULATOR_RUN_DIR}"
                COMMAND_EXPAND_LISTS
                VERBATIM
                COMMENT "Copying emulator runtime DLLs to ROM build directory"
            )
        endif()
    endif()
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_vm
        ${FANTASY_PI_ROOT}/tests/vm/test_vm.cpp
    )
    target_link_libraries(test_vm fantasy_vm_host)
    add_test(NAME VMTest COMMAND test_vm)

    add_executable(test_renderer
        ${FANTASY_PI_ROOT}/tests/renderer/test_renderer.cpp
    )
    target_link_libraries(test_renderer fantasy_renderer_host)
    add_test(NAME RendererTest COMMAND test_renderer)

    add_executable(test_blit
        ${FANTASY_PI_ROOT}/tests/renderer/test_blit.cpp
    )
    target_link_libraries(test_blit fantasy_renderer_host)
    add_test(NAME BlitTest COMMAND test_blit)

    add_executable(test_assembler
        ${FANTASY_PI_ROOT}/tests/assembler/test_assembler.cpp
    )
    add_test(NAME AssemblerTest COMMAND test_assembler)
endif()

# Bare-metal kernel build is handled by build/make/Makefile
# This CMake is for host development only.
