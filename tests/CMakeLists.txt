find_package(Catch2 3 CONFIG REQUIRED)

add_executable(skyautopiano_tests
    codec/test_encrypted_notes.cpp
    codec/test_key_map_builder.cpp
    codec/test_sheet_codec_abc.cpp
    codec/test_sheet_codec_json.cpp
    codec/test_sheet_codec_midi.cpp
    codec/test_sheet_exporter.cpp
    codec/test_text_decoder.cpp
    core/test_core_utils.cpp
    core/test_crash_report.cpp
    core/test_runtime_setup.cpp
    services/test_chromium_local_storage.cpp
    services/test_config_service.cpp
    services/test_legacy_import.cpp
    services/test_library_service.cpp
    services/test_playback_service.cpp
    services/test_playlist_service.cpp
    services/test_report_writer.cpp
    services/test_shortcut_parser.cpp
    services/test_song_clock.cpp
    services/test_store_mapping.cpp
    services/test_theme_library_service.cpp
    services/test_timeline_builder.cpp
    services/test_ui_state_service.cpp
    services/test_update_service.cpp
    skyinput/test_skyinput_key_code.cpp
    ui/test_editor_document.cpp
    ui/test_frame_diff.cpp
    ui/test_key_capture.cpp
    ui/test_theme_catalog.cpp
    ui/test_theme_tokens.cpp
)
target_include_directories(skyautopiano_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_compile_definitions(skyautopiano_tests PRIVATE SKY_QSS_PATH="${CMAKE_SOURCE_DIR}/src/ui/styles/app.qss")
target_link_libraries(skyautopiano_tests PRIVATE sky_ui sky_services sky_codec sky_core sky_crash skyinput miniz::miniz Catch2::Catch2WithMain)

add_custom_command(TARGET skyautopiano_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:skyautopiano_tests> $<TARGET_FILE_DIR:skyautopiano_tests>
    COMMAND_EXPAND_LISTS
)

include(Catch)
catch_discover_tests(skyautopiano_tests DISCOVERY_MODE PRE_TEST)
