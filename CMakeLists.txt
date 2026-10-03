cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(GAME_STR "MM")
project(2s2h VERSION 5.0.1 LANGUAGES C CXX)
include(CMake/2ship-cvars.cmake)
include(CMake/lus-cvars.cmake)
set(SPDLOG_LEVEL_TRACE 0)
set(SPDLOG_LEVEL_OFF 6)
set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "cutoff at trace")
option(DIPTYCH_SHARED_ENGINE "Build libultraship, ImGui, SDL2 and spdlog as DLLs with the dynamic CRT" OFF)
option(DIPTYCH_GAME_MODULE "Build the game as a DLL for a multi-game host (requires DIPTYCH_SHARED_ENGINE)" OFF)
set(DIPTYCH_ROOT "" CACHE PATH "Diptych source root (explicit opt-in)")
if(DIPTYCH_GAME_MODULE AND NOT DIPTYCH_ROOT)
    message(FATAL_ERROR "DIPTYCH_GAME_MODULE requires an explicit DIPTYCH_ROOT")
endif()
if(DIPTYCH_ROOT AND NOT EXISTS "${DIPTYCH_ROOT}/src/2s2h/sources.cmake")
    message(FATAL_ERROR "DIPTYCH_ROOT must contain src/2s2h/sources.cmake")
endif()
if(DIPTYCH_GAME_MODULE AND NOT DIPTYCH_SHARED_ENGINE)
    message(FATAL_ERROR "DIPTYCH_GAME_MODULE requires DIPTYCH_SHARED_ENGINE")
endif()
# Static libraries linked into the shared engine and the game modules must be position independent on ELF.
if(DIPTYCH_SHARED_ENGINE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# GCC gives function-local statics of inline functions one process-wide instance (STB_GNU_UNIQUE); each game module
# keeps its own, as each Windows DLL does.
if(DIPTYCH_GAME_MODULE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>)
endif()
set(PROJECT_BUILD_NAME "Battler Bravo" CACHE STRING "" FORCE)
set(PROJECT_TEAM "github.com/2ship2harkinian" CACHE STRING "" FORCE)

execute_process(
    COMMAND git branch --show-current
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Github Actions checks out a detached HEAD, so check environment for the branch name instead
if(NOT GIT_BRANCH)
    if(DEFINED ENV{GITHUB_HEAD_REF} AND NOT "$ENV{GITHUB_HEAD_REF}" STREQUAL "")
        set(GIT_BRANCH "$ENV{GITHUB_HEAD_REF}")
    elseif(DEFINED ENV{GITHUB_REF_NAME} AND NOT "$ENV{GITHUB_REF_NAME}" STREQUAL "")
        set(GIT_BRANCH "$ENV{GITHUB_REF_NAME}")
    else()
        set(GIT_BRANCH "detached")
    endif()
endif()

set(CMAKE_PROJECT_GIT_BRANCH "${GIT_BRANCH}" CACHE STRING "Git branch" FORCE)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# CI builds a PR from an ephemeral merge commit, so report the head commit GitHub shows instead
if(DEFINED ENV{GITHUB_EVENT_PATH} AND EXISTS "$ENV{GITHUB_EVENT_PATH}")
    file(READ "$ENV{GITHUB_EVENT_PATH}" GITHUB_EVENT_JSON)
    string(JSON PR_HEAD_SHA ERROR_VARIABLE PR_HEAD_SHA_ERROR GET "${GITHUB_EVENT_JSON}" "pull_request" "head" "sha")
    if(PR_HEAD_SHA AND PR_HEAD_SHA_ERROR STREQUAL "NOTFOUND")
        set(GIT_COMMIT_HASH "${PR_HEAD_SHA}")
    endif()
endif()

# Get only the first 7 characters of the hash
string(SUBSTRING "${GIT_COMMIT_HASH}" 0 7 SHORT_COMMIT_HASH)

set(CMAKE_PROJECT_GIT_COMMIT_HASH "${SHORT_COMMIT_HASH}" CACHE STRING "Git commit hash" FORCE)

execute_process(
    COMMAND git describe --tags --abbrev=0 --exact-match HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GIT_COMMIT_TAG)
    set(GIT_COMMIT_TAG "" CACHE STRING "Git commit tag" FORCE)
endif()

set(CMAKE_PROJECT_GIT_COMMIT_TAG "${GIT_COMMIT_TAG}" CACHE STRING "Git commit tag" FORCE)

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT 2ship)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_C_COMPILER_LAUNCHER MATCHES "ccache|sccache")
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    endif()
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows|Linux")
    if(NOT DEFINED BUILD_CROWD_CONTROL)
        set(BUILD_CROWD_CONTROL OFF)
    endif()
endif()

# Enable the Gfx debugger in LUS to use libgfxd from ZAPDTR
set(GFX_DEBUG_DISASSEMBLER ON)

# Tell LUS we're using F3DEX_GBI_2 (in a way that doesn't break libgfxd)
set(GBI_UCODE F3DEX_GBI_2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Enable MPQ and OTR support
set(INCLUDE_MPQ_SUPPORT ON)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

    if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
        OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
        message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
    endif()
endif()

################################################################################
# VCPKG dependencies for Windows
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
option(SKIP_AUTOMATE_VCPKG "Use caller-provided dependencies instead of automatic vcpkg setup" OFF)
if(NOT SKIP_AUTOMATE_VCPKG)
    include(CMake/automate-vcpkg.cmake)
endif()

if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
	set(VCPKG_TRIPLET x86-windows-static)
	set(VCPKG_TARGET_TRIPLET x86-windows-static)
elseif ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
	set(VCPKG_TRIPLET x64-windows-static)
	set(VCPKG_TARGET_TRIPLET x64-windows-static)
endif()

if(DIPTYCH_SHARED_ENGINE)
	set(VCPKG_OVERLAY_TRIPLETS ${CMAKE_CURRENT_SOURCE_DIR}/CMake/triplets)
	set(ENV{VCPKG_OVERLAY_TRIPLETS} ${VCPKG_OVERLAY_TRIPLETS})
	set(VCPKG_TRIPLET x64-windows-diptych)
	set(VCPKG_TARGET_TRIPLET x64-windows-diptych)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	add_compile_definitions(LUS_DYNAMIC_CRT)
endif()

if(NOT SKIP_AUTOMATE_VCPKG)
    vcpkg_bootstrap()
    vcpkg_install_packages(zlib bzip2 libzip libpng sdl2 sdl2-net glew glfw3 nlohmann-json tinyxml2 spdlog libogg libvorbis opus opusfile)
endif()
endif()

################################################################################
# Global configuration types
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(CMAKE_C_FLAGS_DEBUG "-g -ffast-math -DDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -ffast-math -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG")
elseif(MSVC)
    # We need to remove RTC1 as it is not compatible with O2, which we set for some files in DEBUG
    string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_OBJCXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

if(NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    get_linux_lsb_release_information()
    message(STATUS "Linux ${LSB_RELEASE_ID_SHORT} ${LSB_RELEASE_VERSION_SHORT} ${LSB_RELEASE_CODENAME_SHORT}")
else()
    message(STATUS ${CMAKE_SYSTEM_NAME})
endif()

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Set CONTROLLERBUTTONS_T
################################################################################
add_compile_definitions(CONTROLLERBUTTONS_T=uint32_t)

################################################################################
# Sub-projects
################################################################################
if(DIPTYCH_SHARED_ENGINE)
    set(LUS_BUILD_SHARED ON)
endif()
add_subdirectory(libultraship ${CMAKE_BINARY_DIR}/libultraship)
target_compile_definitions(libultraship PUBLIC INCLUDE_MPQ_SUPPORT)

# LUS ships no PCH of its own and these headers dominate its build. Done here rather than in the
# submodule, and PRIVATE, so nothing about its public interface changes.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU|MSVC")
    target_precompile_headers(libultraship PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/Context.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/resource/ResourceManager.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/window/Window.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<spdlog/spdlog.h$<ANGLE-R>>"
    )
    # Clang replays a PCH's pending template instantiations in every consuming TU unless told to do
    # the work up front. MSVC's PCH already behaves this way.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        target_compile_options(libultraship PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fpch-instantiate-templates>
        )
    endif()
endif()

add_subdirectory(ZAPDTR/ZAPD ${CMAKE_BINARY_DIR}/ZAPD)
add_subdirectory(OTRExporter)
add_subdirectory(mm)

set_property(TARGET 2ship PROPERTY APPIMAGE_DESKTOP_FILE_TERMINAL YES)
set_property(TARGET 2ship PROPERTY APPIMAGE_DESKTOP_FILE "${CMAKE_SOURCE_DIR}/mm/linux/2s2h.desktop")
set_property(TARGET 2ship PROPERTY APPIMAGE_ICON_FILE "${CMAKE_BINARY_DIR}/2s2hIcon.png")

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
install(FILES "${CMAKE_BINARY_DIR}/mm/2ship.o2r" DESTINATION . COMPONENT ship)
install(TARGETS ZAPD DESTINATION ./assets/extractor COMPONENT extractor)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/ COMPONENT extractor)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml COMPONENT extractor)
endif()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/ COMPONENT 2s2h)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml COMPONENT 2s2h)
endif()

find_package(Python3 COMPONENTS Interpreter)

# Target to generate OTRs
add_custom_target(
    ExtractAssets
    # CMake versions prior to 3.17 do not have the rm command, use remove instead for older versions
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<VERSION_LESS:${CMAKE_VERSION},3.17>,remove,rm> -f mm.zip 2ship.o2r
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --non-interactive --xml-root ../mm/assets/xml --custom-otr-file 2ship.o2r "--custom-assets-path" ${CMAKE_CURRENT_SOURCE_DIR}/mm/assets/custom --port-ver "${CMAKE_PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -DSYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DTARGET_DIR="$<TARGET_FILE_DIR:ZAPD>" -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/copy-existing-otrs.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Running asset extraction..."
    DEPENDS ZAPD
    BYPRODUCTS mm.o2r ${CMAKE_SOURCE_DIR}/mm.o2r ${CMAKE_SOURCE_DIR}/2ship.o2r
)

# Target to generate headers
add_custom_target(
    ExtractAssetHeaders
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --non-interactive --xml-root ../mm/assets/xml --gen-headers
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Generating asset headers..."
    DEPENDS ZAPD
)

# Target to generate only 2ship.o2r
add_custom_target(
    Generate2ShipOtr
    # CMake versions prior to 3.17 do not have the rm command, use remove instead for older versions
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<VERSION_LESS:${CMAKE_VERSION},3.17>,remove,rm> -f 2ship.o2r
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --norom --custom-otr-file 2ship.o2r "--custom-assets-path" ${CMAKE_CURRENT_SOURCE_DIR}/mm/assets/custom --port-ver "${CMAKE_PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -DSYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DTARGET_DIR="$<TARGET_FILE_DIR:ZAPD>" -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -DONLY2SHIPOTR=On -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/copy-existing-otrs.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Generating 2ship.o2r..."
    DEPENDS ZAPD
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
file(COPY ${CMAKE_SOURCE_DIR}/mm/linux/2s2hIcon.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
add_custom_target(CreateOSXIcons
   COMMAND mkdir -p ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset
   COMMAND sips -z 16 16     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_16x16.png
   COMMAND sips -z 32 32     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_16x16@2x.png
   COMMAND sips -z 32 32     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_32x32.png
   COMMAND sips -z 64 64     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_32x32@2x.png
   COMMAND sips -z 128 128   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_128x128.png
   COMMAND sips -z 256 256   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_128x128@2x.png
   COMMAND sips -z 256 256   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_256x256.png
   COMMAND sips -z 512 512   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_256x256@2x.png
   COMMAND sips -z 512 512   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_512x512.png
   COMMAND cp                mm/macosx/2s2hIcon.png ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_512x512@2x.png
   COMMAND iconutil -c icns -o ${CMAKE_BINARY_DIR}/macosx/2s2h.icns ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset
   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   COMMENT "Creating OSX icons ..."
   )
add_dependencies(2ship CreateOSXIcons)

install(TARGETS ZAPD DESTINATION ${CMAKE_BINARY_DIR}/assets/extractor)

set(PROGRAM_PERMISSIONS_EXECUTE OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml)

# Rename the installed soh binary to drop the macos suffix
INSTALL(CODE "FILE(RENAME \${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h-macos \${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h)")
install(CODE "
   include(BundleUtilities)
  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h\" \"\" \"${dirs}\")
   ")

endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows|NintendoSwitch|CafeOS")
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION . COMPONENT 2s2h RENAME readme.txt )
install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/mods\")" COMPONENT 2s2h)
install(CODE "file(TOUCH \"\${CMAKE_INSTALL_PREFIX}/mods/custom_mod_files_go_here.txt\")" COMPONENT 2s2h)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR "External")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows|NintendoSwitch|CafeOS")
    set(CPACK_GENERATOR "ZIP")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPACK_GENERATOR "Bundle")
endif()

set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_SOURCE_DIR}/CMake/Packaging-2.cmake)
include(CMake/Packaging.cmake)

# Dependencies force static CRT; shared DLLs need dynamic CRT throughout.
if(MSVC AND DIPTYCH_SHARED_ENGINE)
    function(diptych_force_dynamic_crt dir)
        get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE|OBJECT_LIBRARY)$")
                set_target_properties(${target} PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
                # CMP0091 OLD targets ignore MSVC_RUNTIME_LIBRARY.
                set_property(TARGET ${target} APPEND PROPERTY COMPILE_OPTIONS
                    "$<$<COMPILE_LANGUAGE:C,CXX>:$<IF:$<CONFIG:Debug>,/MDd,/MD>>")
            endif()
        endforeach()
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        foreach(subdir IN LISTS subdirs)
            diptych_force_dynamic_crt(${subdir})
        endforeach()
    endfunction()
    diptych_force_dynamic_crt(${CMAKE_SOURCE_DIR})
endif()
