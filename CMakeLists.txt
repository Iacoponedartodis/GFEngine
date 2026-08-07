cmake_minimum_required(VERSION 3.25)
project(GFEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ── SDL2 ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
set(SDL_STATIC          ON  CACHE BOOL "" FORCE)
set(SDL_SHARED          OFF CACHE BOOL "" FORCE)
set(SDL_TEST            OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# ── stb ───────────────────────────────────────────────────────────────────────
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    # Pin al commit già in uso (KI #27: senza tag ogni configure da zero
    # prendeva il master corrente — build non riproducibile)
    GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4
)
FetchContent_MakeAvailable(stb)

# ── tinyobjloader ─────────────────────────────────────────────────────────────
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyobjloader)

# ── tinygltf ──────────────────────────────────────────────────────────────────
FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_SHALLOW    TRUE
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL              OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY          ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── spdlog (telemetria, ADR-013) ──────────────────────────────────────────────
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# ── cpptrace (stack trace nei crash report, ADR-013) ──────────────────────────
FetchContent_Declare(cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v0.7.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cpptrace)

# ── Tracy profiler (Fase 1 ottimizzazione — opt-in, ADR-015) ──────────────────
# Profiling real-time a precisione nanosecondo. OFF di default: le build normali
# (Debug/Release) restano identiche e a costo zero — con TRACY_ENABLE non definita
# le macro ZoneScoped/FrameMark diventano no-op e TracyClient compila come stub.
# Per profilare: configura una build RelWithDebInfo con -DUSE_TRACY_PROFILER=ON e
# collega il Tracy profiler GUI a 127.0.0.1.
# Linkato SOLO a GFEngine (ADR-002: l'editor non deve dipendere dal runtime).
# Tag pinnato (KI #27: niente tag mobili → build riproducibili).
option(USE_TRACY_PROFILER "Abilita il profiler Tracy in GFEngine" OFF)
FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG        v0.11.1
    GIT_SHALLOW    TRUE
)
# TracyClient legge l'opzione TRACY_ENABLE: ON compila il client e definisce la
# macro TRACY_ENABLE (propagata via INTERFACE a chi linka); OFF = stub a costo nullo.
# Gli header restano sempre includibili in entrambi gli stati.
set(TRACY_ENABLE    ${USE_TRACY_PROFILER} CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND ON                      CACHE BOOL "" FORCE)  # profila solo con GUI connessa
FetchContent_MakeAvailable(tracy)

# ── Recast Navigation (ADR-017, Fase navigazione) ─────────────────────────────
# NavMesh + pathfinding + crowd. Solo le librerie core (Recast/Detour/DetourCrowd),
# NIENTE demo/tests/examples (tirano dentro SDL/GL). Linkate SOLO a GFEngine
# (ADR-002). Tag pinnato (KI #27). Phase A usa Recast+Detour; DetourCrowd in Phase B.
FetchContent_Declare(recastnavigation
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
    GIT_TAG        v1.6.0
    GIT_SHALLOW    TRUE
)
set(RECASTNAVIGATION_DEMO     OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS    OFF CACHE BOOL "" FORCE)
# v1.6.0 usa cmake_minimum_required < 3.5, rifiutato da CMake 4.0. Questo dice a
# CMake di trattare il progetto come min 3.5 (workaround ufficiale, tag invariato).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(recastnavigation)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# ── Simboli anche in Release ─────────────────────────────────────────────────
# Il crash reporter (cpptrace) sa già stampare uno stack, ma senza PDB stampa
# indirizzi: "#9 0x00007ff75ff533a0 in ??". Un crash che capita SOLO in Release
# — e capitano, l'ottimizzazione fa emergere difetti che il Debug maschera — era
# quindi illeggibile proprio nella configurazione in cui si presenta.
# /Zi produce il PDB, /DEBUG lo collega, /OPT:REF /OPT:ICF rimettono le
# ottimizzazioni di link che /DEBUG disattiva: il binario resta quello di prima,
# i simboli vivono in un file a parte. Costo a runtime: zero (ADR-050).
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>
                     $<$<CONFIG:Release>:/OPT:REF>
                     $<$<CONFIG:Release>:/OPT:ICF>)
endif()

# ── Sanitizers (opt-in: rallentano molto, non per il playtest) ────────────────
# MSVC supporta solo ASan (/fsanitize=address); UBSan NON esiste su MSVC.
option(GF_ENABLE_ASAN "Abilita AddressSanitizer (e UBSan dove supportato)" OFF)
if(GF_ENABLE_ASAN AND MSVC)
    # MSVC aggiunge /RTC1 di default nella config Debug, ma /RTC è INCOMPATIBILE con
    # /fsanitize=address (errore di configurazione/compilazione). Lo si toglie dai
    # flag Debug quando ASan è attivo. NB: serve anche il componente "C++
    # AddressSanitizer" installato dal Visual Studio Installer (fornisce clang_rt.asan).
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
endif()
function(gf_apply_sanitizers target)
    if(NOT GF_ENABLE_ASAN)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        # Le librerie di terze parti (vcpkg: cpptrace, ...) sono compilate SENZA le
        # annotazioni-contenitori ASan della STL; senza questi define si ha
        # LNK2038 'annotate_vector'/'annotate_string' mismatch. Disattivarle sui
        # NOSTRI target li fa combaciare (si perde solo il container-overflow ASan,
        # non il resto). È il workaround MSVC standard per ASan + lib prebuilt.
        target_compile_definitions(${target} PRIVATE
            _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        # /INCREMENTAL è incompatibile con ASan
        target_link_options(${target} PRIVATE /INCREMENTAL:NO)
    else()
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# ── OpenGL — DEVE stare prima di imgui ────────────────────────────────────────
if(WIN32)
    set(PLATFORM_GL_LIBS opengl32 gdi32)
else()
    find_package(OpenGL REQUIRED)
    set(PLATFORM_GL_LIBS OpenGL::GL)
endif()

# ── Dear ImGui (docking branch) ───────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    # Pin al commit del branch docking già in uso (KI #27: "docking" è un
    # branch mobile — un configure da zero poteva prendere una API diversa)
    GIT_TAG        6029ee3789a2b7898f6423ec0c88cc4e5425f5a9
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    # imgui_demo.cpp non è "la demo": è dove vivono ShowStyleEditor (l'editor di
    # aspetto integrato) e ShowMetricsWindow, che contiene l'**ID Stack Tool** —
    # lo strumento che diagnostica esattamente i conflitti di identificatore che ci
    # sono già costati un modale invisibile (KI del 2026-08-05, changelog 164).
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(imgui PUBLIC
    SDL2::SDL2-static
    ${PLATFORM_GL_LIBS}
)

# ── GFEngine ──────────────────────────────────────────────────────────────────
add_executable(GFEngine
    src/main.cpp
    src/core/Application.cpp
    src/core/StateDump.cpp
    src/core/Audio.cpp
    src/core/Clock.cpp
    src/core/Window.cpp
    src/core/Renderer.cpp
    src/platform/OpenGL.cpp
    src/physics/Collision.cpp
    src/core/InputManager.cpp
    src/render/Shader.cpp
    src/render/Camera.cpp
    src/render/Mesh.cpp
    src/render/Texture.cpp
    src/render/Model.cpp
    src/render/HUD.cpp
    src/render/Ui2D.cpp
    src/vendor/stb_image_impl.cpp
    src/vendor/tinyobj_impl.cpp
    src/ecs/World.cpp
    src/ecs/systems/MovementSystem.cpp
    src/ecs/systems/CombatSystem.cpp
    src/ecs/systems/SquadSystem.cpp
    src/ecs/systems/ObjectiveSystem.cpp
    src/game/data/ContentValidation.cpp
    src/ecs/systems/AiSystem.cpp
    src/ecs/systems/AiCommandLayer.cpp
    src/ecs/systems/AiTrace.cpp
    src/ecs/systems/CrowdSystem.cpp
    src/game/game_modes/ConquestMode.cpp
    src/game/game_modes/SandboxMode.cpp
    src/game/game_modes/GameModeFactory.cpp
    src/game/game_modes/ObjectiveModes.cpp
    src/game/CommandPosts.cpp
    src/game/nav/NavManager.cpp
    src/game/ai/WorldIntel.cpp
    src/core/Telemetry.cpp
    src/core/Profiler.cpp
    src/game/PlayerController.cpp
    src/game/data/DefinitionRegistry.cpp
    src/render/OptionsMenu.cpp
    src/render/PreMatchMenu.cpp
    src/render/LauncherScreen.cpp
    src/render/MainMenuScreen.cpp
    src/render/SandboxMenu.cpp
    src/vendor/tinygltf_impl.cpp
)
target_include_directories(GFEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/glm
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
    ${tinygltf_SOURCE_DIR}
)
target_compile_features(GFEngine PRIVATE cxx_std_20)
target_compile_definitions(GFEngine PRIVATE SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS)
target_link_libraries(GFEngine PRIVATE
    SDL2::SDL2-static
    ${PLATFORM_GL_LIBS}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cpptrace::cpptrace
    TracyClient   # stub a costo nullo se USE_TRACY_PROFILER=OFF (ADR-015)
    Recast Detour DetourCrowd   # navmesh + pathfinding + crowd (ADR-017)
)
gf_apply_sanitizers(GFEngine)
if(MSVC)
    target_compile_options(GFEngine PRIVATE
        /W4 /permissive- /EHsc
        /external:anglebrackets /external:W0
    )
else()
    target_compile_options(GFEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── GFEditor ──────────────────────────────────────────────────────────────────
add_executable(GFEditor
    editor/src/main.cpp
    editor/src/EditorApp.cpp
    editor/src/ui/HomeScreen.cpp
    editor/src/ui/HelpBrowser.cpp
    editor/src/viewport/FreeCameraViewport.cpp
    editor/src/util/FileDialog.cpp
    editor/src/util/RigReader.cpp
    editor/src/util/DefinitionRename.cpp
    editor/src/util/DataPath.cpp          # R8: unica risoluzione di data/
    editor/src/modules/BalanceEditor.cpp
    editor/src/modules/EntityEditor.cpp
    editor/src/modules/MapEditor.cpp
    editor/src/modules/WeaponEditor.cpp
    editor/src/modules/VehicleEditor.cpp
    editor/src/modules/MissionEditor.cpp
    editor/src/modules/ClassEditor.cpp
    src/core/Telemetry.cpp
    src/core/Profiler.cpp
    src/game/data/DefinitionRegistry.cpp
    src/game/data/ContentValidation.cpp   # ADR-018: STESSO gate del runtime, mai una copia
    src/game/ai/WorldIntel.cpp            # ADR-032: il loader costruisce il grafo dei link
    src/game/nav/NavManager.cpp           # STESSA costruzione di navmesh del runtime.
                                          # L'editor deve poter dire "qui non ci si cammina"
                                          # PRIMA di salvare, e l'unico modo che non menta e
                                          # costruire il navmesh vero: un secondo calcolo
                                          # "solo per l'editor" darebbe un verdetto diverso da
                                          # quello del gioco — il difetto che ci e costato di
                                          # piu (changelog 77). Stesso principio di ADR-018.
    src/platform/OpenGL.cpp
    src/render/Shader.cpp
    src/render/Camera.cpp
    src/render/Mesh.cpp
    src/render/Texture.cpp
    src/render/Model.cpp
    src/vendor/stb_image_impl.cpp
    src/vendor/tinyobj_impl.cpp
    src/vendor/tinygltf_impl.cpp
)
target_include_directories(GFEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/editor/include
    ${CMAKE_SOURCE_DIR}/editor/include/modules
    ${CMAKE_SOURCE_DIR}/external/glm
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
    ${tinygltf_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(GFEditor PRIVATE cxx_std_20)
target_compile_definitions(GFEditor PRIVATE SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS)
target_link_libraries(GFEditor PRIVATE
    SDL2::SDL2-static
    ${PLATFORM_GL_LIBS}
    imgui
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cpptrace::cpptrace
    Recast Detour DetourCrowd   # navmesh nell'editor: vedi la nota su NavManager.cpp
)
gf_apply_sanitizers(GFEditor)
if(WIN32)
    target_link_libraries(GFEditor PRIVATE comdlg32)
endif()
if(MSVC)
    target_compile_options(GFEditor PRIVATE
        /W4 /permissive- /EHsc
        /external:anglebrackets /external:W0
    )
else()
    target_compile_options(GFEditor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Copia cartella data nella directory di output ─────────────────────────────
add_custom_command(TARGET GFEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:GFEngine>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:GFEngine>/data"
    COMMAND ${CMAKE_COMMAND} -DTARGET_DATA_DIR=$<TARGET_FILE_DIR:GFEngine>/data
            -P "${CMAKE_SOURCE_DIR}/cmake/strip_backups.cmake"
    COMMENT "Copia data/ -> GFEngine output dir (pulizia preventiva, senza .bak)"
)
add_custom_command(TARGET GFEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:GFEditor>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:GFEditor>/data"
    COMMAND ${CMAKE_COMMAND} -DTARGET_DATA_DIR=$<TARGET_FILE_DIR:GFEditor>/data
            -P "${CMAKE_SOURCE_DIR}/cmake/strip_backups.cmake"
    COMMENT "Copia data/ -> GFEditor output dir (pulizia preventiva, senza .bak)"
)

# ── Copia cartella assets nella directory di output ───────────────────────────
# Il gioco carica i modelli (.glb/.obj) con percorsi relativi "assets/..."
# risolti rispetto alla cartella dell'eseguibile.
add_custom_command(TARGET GFEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:GFEngine>/assets"
    COMMENT "Copia assets/ -> GFEngine output dir"
)