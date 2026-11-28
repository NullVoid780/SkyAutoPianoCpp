add_library(sky_core STATIC
    core/base64.cpp
    core/file_system.cpp
    core/logger.cpp
    core/precise_sleep.cpp
    core/random.cpp
    core/string_utils.cpp
    core/text_file.cpp
    core/time_utils.cpp
    core/translator.cpp
)
target_include_directories(sky_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sky_core
    PUBLIC Qt6::Core nlohmann_json::nlohmann_json
    PRIVATE winmm
)

add_library(sky_model INTERFACE)
target_link_libraries(sky_model INTERFACE sky_core)

add_library(sky_codec STATIC
    codec/abc_sheet_parser.cpp
    codec/encrypted_notes.cpp
    codec/json_sheet_parser.cpp
    codec/key_map_builder.cpp
    codec/midi_file.cpp
    codec/midi_sheet_parser.cpp
    codec/note_normalizer.cpp
    codec/playback_note_builder.cpp
    codec/relaxed_json.cpp
    codec/sheet_codec.cpp
    codec/sheet_exporter.cpp
    codec/text_decoder.cpp
)
target_link_libraries(sky_codec PUBLIC sky_model sky_core)

add_library(sky_net STATIC
    net/downloader.cpp
    net/http_client.cpp
    net/url.cpp
    net/zip_extract.cpp
)
target_link_libraries(sky_net PUBLIC sky_core Qt6::Network PRIVATE miniz::miniz)

add_library(sky_services STATIC
    services/audio/note_synth.cpp
    services/audio/sample_bank.cpp
    services/audio/sheet_audio.cpp
    services/audio/wave_out.cpp
    services/library/chromium_local_storage.cpp
    services/library/favorites_service.cpp
    services/library/legacy_import.cpp
    services/library/legacy_removal.cpp
    services/library/library_service.cpp
    services/library/playlist_service.cpp
    services/library/recent_service.cpp
    services/notifications/toast_service.cpp
    services/playback/lite_player.cpp
    services/playback/playback_service.cpp
    services/playback/player_base.cpp
    services/playback/release_scheduler.cpp
    services/playback/song_clock.cpp
    services/playback/strict_player.cpp
    services/playback/timeline_builder.cpp
    services/settings/config_service.cpp
    services/settings/shortcut_parser.cpp
    services/settings/shortcut_service.cpp
    services/settings/theme_library_service.cpp
    services/report/report_writer.cpp
    services/settings/ui_state_service.cpp
    services/store/store_mapping.cpp
    services/store/store_service.cpp
    services/update/release_notes.cpp
    services/update/github_release.cpp
    services/update/update_service.cpp
    services/update/version.cpp
)
target_link_libraries(sky_services PUBLIC sky_codec sky_net sky_model sky_core skyinput PRIVATE miniz::miniz winmm)
find_path(DRLIBS_INCLUDE_DIRS "dr_mp3.h" REQUIRED)
target_include_directories(sky_services SYSTEM PRIVATE ${DRLIBS_INCLUDE_DIRS})
target_compile_definitions(sky_services PRIVATE SKY_UPDATE_REPOSITORY="${SKY_UPDATE_REPOSITORY}")

add_library(sky_crash STATIC
    crash/breadcrumbs.cpp
    crash/crash_archive.cpp
    crash/crash_channel.cpp
    crash/crash_dialog.cpp
    crash/crash_handler.cpp
    crash/crash_report.cpp
    crash/minidump_writer.cpp
    crash/pending_reports.cpp
    crash/report_collector.cpp
    crash/stack_walker.cpp
)
target_include_directories(sky_crash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sky_crash
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE dbghelp psapi version bcrypt comctl32 shell32 ole32 advapi32 miniz::miniz
)

add_executable(crashreport WIN32 crash/reporter_main.cpp)
set_target_properties(crashreport PROPERTIES OUTPUT_NAME CrashReport)
target_link_libraries(crashreport PRIVATE sky_crash)

configure_file(app/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/app/version.hpp @ONLY)

add_library(sky_ui STATIC
    app/paths.cpp
    app/player_controller.cpp
    app/update_controller.cpp
    app/single_instance.cpp
    platform/file_dialog.cpp
    platform/shell_open.cpp
    platform/tray_notification.cpp
    platform/window_attributes.cpp
    ui/core/easing.cpp
    ui/core/elevation_layer.cpp
    ui/core/hover_tracker.cpp
    ui/core/overlay.cpp
    ui/core/smooth_scroll.cpp
    ui/main_window/add_sheet_button.cpp
    ui/main_window/bulk_toolbar.cpp
    ui/main_window/nav_drawer.cpp
    ui/main_window/nav_tab.cpp
    ui/main_window/nav_toggle.cpp
    ui/main_window/playlist_card_delegate.cpp
    ui/main_window/playlist_detail_bar.cpp
    ui/main_window/playlist_picker.cpp
    ui/main_window/quick_start.cpp
    ui/main_window/playlists_view.cpp
    ui/main_window/store_card_delegate.cpp
    ui/main_window/store_pagination.cpp
    ui/main_window/store_view.cpp
    ui/widgets/confirm_popup.cpp
    ui/widgets/card_frame.cpp
    ui/widgets/markdown_view.cpp
    ui/widgets/progress_bar.cpp
    ui/main_window/changelog_panel.cpp
    ui/settings_window/update_overview_card.cpp
    ui/settings_window/update_prompt.cpp
    ui/editor_window/editable_field.cpp
    ui/editor_window/editor_document.cpp
    ui/editor_window/editor_setup.cpp
    ui/editor_window/editor_window.cpp
    ui/editor_window/key_pad.cpp
    ui/editor_window/key_pad_cell.cpp
    ui/editor_window/meta_panel.cpp
    ui/editor_window/step_grid_view.cpp
    ui/editor_window/timeline_view.cpp
    ui/editor_window/tool_button.cpp
    ui/widgets/flow_layout.cpp
    ui/widgets/step_dots.cpp
    ui/widgets/toggle_switch.cpp
    ui/settings_window/about_tab.cpp
    ui/settings_window/action_button.cpp
    ui/settings_window/app_info_card.cpp
    ui/settings_window/confirm_dialog.cpp
    ui/settings_window/general_tab.cpp
    ui/settings_window/key_cap.cpp
    ui/settings_window/key_capture.cpp
    ui/settings_window/keyboard_tab.cpp
    ui/settings_window/save_button.cpp
    ui/settings_window/setting_card.cpp
    ui/settings_window/settings_panel.cpp
    ui/settings_window/settings_row.cpp
    ui/settings_window/settings_sidebar.cpp
    ui/settings_window/reports_tab.cpp
    ui/settings_window/settings_window.cpp
    ui/settings_window/shortcut_edit.cpp
    ui/settings_window/shortcut_row.cpp
    ui/settings_window/shortcuts_tab.cpp
    ui/settings_window/sidebar_item.cpp
    ui/settings_window/tab_header.cpp
    ui/settings_window/updates_tab.cpp
    ui/settings_window/color_picker.cpp
    ui/settings_window/theme_builder.cpp
    ui/settings_window/theme_card.cpp
    ui/settings_window/theme_preview.cpp
    ui/settings_window/themes_tab.cpp
    app/theme_controller.cpp
    app/issue_recorder.cpp
    app/sky_window_watcher.cpp
    ui/issue/issue_report_window.cpp
    ui/issue/recording_bar.cpp
    ui/theme_catalog.cpp
    ui/main_window/library_model.cpp
    ui/main_window/library_view.cpp
    ui/main_window/loop_control.cpp
    ui/main_window/main_window.cpp
    ui/main_window/player_footer.cpp
    ui/main_window/sky_status_chip.cpp
    ui/main_window/scroll_top_button.cpp
    ui/main_window/sheet_actions_menu.cpp
    ui/main_window/sheet_card_delegate.cpp
    ui/main_window/speed_control.cpp
    ui/main_window/theme_toggle.cpp
    ui/render/backdrop_blur.cpp
    ui/render/box_blur.cpp
    ui/render/font_loader.cpp
    ui/render/frame_diff.cpp
    ui/render/icon_cache.cpp
    ui/render/paint_utils.cpp
    ui/render/shadow_painter.cpp
    ui/theme.cpp
    ui/theme_manager.cpp
    ui/widgets/badge.cpp
    ui/widgets/icon_button.cpp
    ui/widgets/inline_switch.cpp
    ui/widgets/search_field.cpp
    ui/widgets/seek_slider.cpp
    ui/widgets/select_box.cpp
    ui/widgets/state_card.cpp
    ui/widgets/toast_host.cpp
    ui/widgets/toast_item.cpp
    ui/widgets/transport_button.cpp
)
target_include_directories(sky_ui PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(sky_ui PUBLIC sky_services Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Network PRIVATE dwmapi)

add_executable(skyautopiano WIN32
    main.cpp
    app/application.cpp
    ui/resources.qrc
)
set_target_properties(skyautopiano PROPERTIES OUTPUT_NAME SkyAutoPiano)
target_link_libraries(skyautopiano PRIVATE sky_ui sky_crash)
add_dependencies(skyautopiano crashreport)

set(SKY_ICON_PATH ${CMAKE_SOURCE_DIR}/assets/icon/Icon9.ico)
set(SKY_FILE_DESCRIPTION "Sky Auto Piano")
set(SKY_INTERNAL_NAME "SkyAutoPiano")
configure_file(${CMAKE_SOURCE_DIR}/packaging/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/generated/skyautopiano.rc @ONLY)
set(SKY_FILE_DESCRIPTION "Sky Auto Piano crash reporter")
set(SKY_INTERNAL_NAME "CrashReport")
configure_file(${CMAKE_SOURCE_DIR}/packaging/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/generated/crashreport.rc @ONLY)
target_sources(skyautopiano PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/skyautopiano.rc ${CMAKE_SOURCE_DIR}/packaging/app.manifest)
target_sources(crashreport PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/crashreport.rc ${CMAKE_SOURCE_DIR}/packaging/app.manifest)

file(GLOB SKY_ICON_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/icons/*.svg)
qt_add_resources(skyautopiano sky_icons PREFIX "/icons" BASE ${CMAKE_SOURCE_DIR}/assets/icons FILES ${SKY_ICON_FILES})
file(GLOB_RECURSE SKY_SOUNDPACK_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/soundpack/*.mp3
     ${CMAKE_SOURCE_DIR}/assets/soundpack/*.wav ${CMAKE_SOURCE_DIR}/assets/soundpack/*.flac)
qt_add_resources(skyautopiano sky_soundpack PREFIX "/soundpack" BASE ${CMAKE_SOURCE_DIR}/assets/soundpack
                 FILES ${SKY_SOUNDPACK_FILES})

file(GLOB SKY_I18N_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/i18n/*.json)
qt_add_resources(skyautopiano sky_i18n PREFIX "/i18n" BASE ${CMAKE_SOURCE_DIR}/assets/i18n FILES ${SKY_I18N_FILES})

get_target_property(SKY_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(SKY_QT_BIN ${SKY_QMAKE} DIRECTORY)
add_custom_command(TARGET skyautopiano POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:skyautopiano> $<TARGET_FILE_DIR:skyautopiano>
    COMMAND_EXPAND_LISTS
)
add_custom_command(TARGET skyautopiano POST_BUILD
    COMMAND ${SKY_QT_BIN}/windeployqt.exe --no-translations --no-compiler-runtime --no-system-d3d-compiler
            --no-opengl-sw --verbose 0 $<TARGET_FILE:skyautopiano>
    COMMENT "Deploying Qt runtime next to SkyAutoPiano.exe"
)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_SKIP TRUE)
include(InstallRequiredSystemLibraries)
set(SKY_DEPLOY_ROOT ${CMAKE_BINARY_DIR}/deploy)
set(SKY_STAGING ${SKY_DEPLOY_ROOT}/SkyAutoPiano)
add_custom_target(deploy
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SKY_STAGING}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SKY_STAGING}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:skyautopiano> $<TARGET_FILE:crashreport> ${SKY_STAGING}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${SKY_STAGING}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:skyautopiano> ${SKY_STAGING}
    COMMAND ${SKY_QT_BIN}/windeployqt.exe --no-translations --no-compiler-runtime --no-system-d3d-compiler
            --no-system-dxc-compiler --no-opengl-sw --verbose 0 ${SKY_STAGING}/SkyAutoPiano.exe
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/packaging/licenses ${SKY_STAGING}/licenses
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${SKY_STAGING}/licenses/LICENSE
    DEPENDS skyautopiano crashreport
    COMMENT "Staging a deployable copy in ${SKY_STAGING}"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

find_program(SKY_ISCC ISCC PATHS "C:/Program Files (x86)/Inno Setup 6" "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6")
set(SKY_ZIP ${SKY_DEPLOY_ROOT}/SkyAutoPiano-${PROJECT_VERSION}-portable.zip)
if(SKY_ISCC)
    set(SKY_INSTALLER_COMMAND ${SKY_ISCC} /Q /DAppVersion=${PROJECT_VERSION} /DStagingDir=${SKY_STAGING}
        /DIconFile=${SKY_ICON_PATH} /DOutputDir=${SKY_DEPLOY_ROOT} ${CMAKE_SOURCE_DIR}/packaging/installer.iss)
else()
    set(SKY_INSTALLER_COMMAND ${CMAKE_COMMAND} -E echo "Inno Setup 6 not found: skipped the installer, built the zip only")
endif()
add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E rm -f ${SKY_ZIP}
    COMMAND ${CMAKE_COMMAND} -E chdir ${SKY_STAGING} ${CMAKE_COMMAND} -E tar cf ${SKY_ZIP} --format=zip .
    COMMAND ${SKY_INSTALLER_COMMAND}
    DEPENDS deploy
    COMMENT "Packaging Sky Auto Piano ${PROJECT_VERSION}"
    VERBATIM
)