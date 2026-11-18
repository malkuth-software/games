cmake_minimum_required(VERSION 3.25)
project(GamesPortableAudioTests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(GAMES_AUDIO_SOURCE_DIR "" CACHE PATH "Development GUI.Forms source root; leave empty for installed Audio SDK")
if(GAMES_AUDIO_SOURCE_DIR)
    # GUI.Forms itself builds GUIForms::Audio (and GUIForms::Threading) with audio
    # on: its Audio.cmake needs the stx package's Vorbis decoder, which only
    # GUI.Forms' own configure builds and finds (cmake/Stx.cmake), so the audio
    # build is GUI.Forms' with every host, the renderer and the Gallery off.
    set(GUI_FORMS_ENABLE_SKIA OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_ENABLE_MACOS_HOST OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_ENABLE_WINDOWS_HOST OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_ENABLE_LINUX_HOST OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_BUILD_PREPARED_TEXT OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GUI_FORMS_BUILD_AUDIO ON CACHE BOOL "" FORCE)
    set(GUI_FORMS_AUDIO_VORBIS_BACKEND stx CACHE STRING "" FORCE)
    set(GUI_FORMS_BUILD_AUDIO_LOOP_TRANSPORT ON CACHE BOOL "Development transport" FORCE)
    add_subdirectory("${GAMES_AUDIO_SOURCE_DIR}" toolkit EXCLUDE_FROM_ALL)
else()
    find_package(GUIForms 0.6 CONFIG REQUIRED COMPONENTS Audio)
endif()
get_filename_component(games_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GAMES_AUDIO_ASSET_DIR "" CACHE PATH "Prepared Games runtime assets containing audio/")
set(GAMES_AUDIO_FORMAT "ogg" CACHE STRING "Prepared audio format: ogg or wav")
if(NOT IS_DIRECTORY "${GAMES_AUDIO_ASSET_DIR}/audio")
    message(FATAL_ERROR "Set GAMES_AUDIO_ASSET_DIR to a complete prepared runtime asset directory")
endif()
if(NOT GAMES_AUDIO_FORMAT STREQUAL "ogg" AND NOT GAMES_AUDIO_FORMAT STREQUAL "wav")
    message(FATAL_ERROR "GAMES_AUDIO_FORMAT must be ogg or wav")
endif()
add_library(game_audio_adapters STATIC
    "${games_root}/src/audio_loader.cpp"
    "${games_root}/src/pcm_player.cpp"
    "${games_root}/src/scene_audio.cpp"
    "${games_root}/src/audio.cpp"
    "${games_root}/src/runtime_paths.cpp"
    "${games_root}/games/eggy/src/audio.cpp"
    "${games_root}/games/switchbox/src/platform/audio.cpp"
    "${games_root}/games/atomprobe/src/platform/audio.cpp")
target_compile_features(game_audio_adapters PUBLIC cxx_std_20)
target_include_directories(game_audio_adapters PUBLIC "${games_root}/src")
target_link_libraries(game_audio_adapters PUBLIC GUIForms::Audio)
if(GAMES_AUDIO_SOURCE_DIR)
    target_sources(game_audio_adapters PRIVATE
        "${games_root}/src/bar_music.cpp"
        "${games_root}/games/fourpegs/src/platform/audio.cpp")
    add_executable(bar_music "${games_root}/tests/bar_music_tests.cpp")
    target_link_libraries(bar_music PRIVATE game_audio_adapters)
    add_executable(audio_device_smoke "${games_root}/tools/audio_device_smoke.cpp")
    target_link_libraries(audio_device_smoke PRIVATE game_audio_adapters)
endif()
add_executable(audio_assets "${games_root}/tests/audio_tests.cpp")
target_link_libraries(audio_assets PRIVATE GUIForms::Audio)
add_executable(audio_policy "${games_root}/tests/audio_policy_tests.cpp")
target_link_libraries(audio_policy PRIVATE game_audio_adapters)
enable_testing()
add_test(NAME prepared_audio_assets COMMAND audio_assets "${GAMES_AUDIO_ASSET_DIR}" "${GAMES_AUDIO_FORMAT}")
add_test(NAME game_audio_policy COMMAND audio_policy)
if(TARGET bar_music)
    add_test(NAME bar_music COMMAND bar_music "${GAMES_AUDIO_ASSET_DIR}")
    set_tests_properties(bar_music PROPERTIES TIMEOUT 30)
endif()
