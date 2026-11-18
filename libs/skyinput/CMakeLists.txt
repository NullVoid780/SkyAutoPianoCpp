add_library(skyinput STATIC
    src/error.cpp
    src/global_hotkey.cpp
    src/hardware_sender.cpp
    src/hook_thread.cpp
    src/hotkey_thread.cpp
    src/image.cpp
    src/input_blocker.cpp
    src/key_code.cpp
    src/keyboard.cpp
    src/low_level_hook.cpp
    src/mouse.cpp
    src/mouse_curve.cpp
    src/panic_release.cpp
    src/scan_code.cpp
    src/screen.cpp
    src/virtual_sender.cpp
    src/window_info.cpp
    src/work_window.cpp
)
target_include_directories(skyinput
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(skyinput PRIVATE user32 gdi32)
