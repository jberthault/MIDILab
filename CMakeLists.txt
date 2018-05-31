project(MIDILab)

cmake_minimum_required(VERSION 3.1.3)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# -------
# options
# -------

option(ENABLE_PACKAGING "collects DLLs after a successul build in order to make a standalone installer (MinGW only)")
option(ENABLE_LOG_DEBUG "allows 'debug' messages to appear on the standard output")
option(ENABLE_LOG_INFO "allows 'info' messages to appear on the standard output" ON)
option(ENABLE_LOG_WARNING "allows 'warning' messages to appear on the standard output" ON)
option(ENABLE_LOG_ERROR "allows 'error' messages to appear on the standard output" ON)
option(ENABLE_LOG_TIMING "embed time measurements for debugging purposes")

# -------------------
# target dependencies
# -------------------

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5XmlPatterns REQUIRED)
find_package(Boost REQUIRED)
find_package(FluidSynth)
if (UNIX)
    find_package(ALSA REQUIRED)
endif()

# -----------------
# target definition
# -----------------

file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")
set(QRC_FILES "src/data.qrc")
set(TXT_FILES "README.md" "ROADMAP.md" "LICENSE")

add_executable(${PROJECT_NAME} ${SRC_FILES} ${QRC_FILES} ${TXT_FILES})

# --------------------
# target configuration
# --------------------

foreach(LEVEL IN ITEMS DEBUG INFO WARNING ERROR TIMING)
    if (ENABLE_LOG_${LEVEL})
        message(STATUS "${LEVEL} messages enabled")
        target_compile_definitions(${PROJECT_NAME} PUBLIC "-DMIDILAB_ENABLE_${LEVEL}")
    else()
        message(STATUS "${LEVEL} messages disabled")
    endif()
endforeach()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_relaxed_constexpr) # triggers >= c++14
target_include_directories(${PROJECT_NAME} PUBLIC "src")
target_link_libraries(${PROJECT_NAME} Boost::boost Qt5::Widgets Qt5::Xml Qt5::XmlPatterns)

if (FluidSynth_FOUND)
    target_link_libraries(${PROJECT_NAME} FluidSynth::FluidSynth)
    target_compile_definitions(${PROJECT_NAME} PUBLIC "-DMIDILAB_FLUIDSYNTH_VERSION=${FluidSynth_VERSION}")
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME} ${ALSA_LIBRARIES} pthread)
    target_include_directories(${PROJECT_NAME} PUBLIC ${ALSA_INCLUDE_DIRS})
elseif(WIN32)
    target_sources(${PROJECT_NAME} PUBLIC "src/config.rc")
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON) # discard terminal
    target_link_libraries(${PROJECT_NAME} WinMM)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --always
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE MIDILAB_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC "-DMIDILAB_VERSION=${MIDILAB_VERSION}")
        set_target_properties(${PROJECT_NAME} PROPERTIES VERSION "${MIDILAB_VERSION}")
    endif()
endif()

# ------------
# installation
# ------------

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
install(FILES "LICENSE" DESTINATION ".")

if (MINGW AND ENABLE_PACKAGING)
    # look for windeployqt to gather qt libs
    find_program(WINDEPLOYQT_EXECUTABLE "windeployqt")
    # we assume dll will be found in the same directory as windeployqt
    get_filename_component(DEPENDENCY_BINARY_DIR "${WINDEPLOYQT_EXECUTABLE}" DIRECTORY)
    # the build type determines which dlls must be copied
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(WINDEPLOYQT_OPTIONS "--release")
    endif()
    # copy every file required to prepare a standalone installer
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/package"
        # qt & gcc runtime
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${WINDEPLOYQT_OPTIONS} --compiler-runtime -dir "${CMAKE_CURRENT_BINARY_DIR}/package" "$<TARGET_FILE:${PROJECT_NAME}>"
        # qt dependencies
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libbz2-1.dll        "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libfreetype-6.dll   "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libharfbuzz-0.dll   "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libglib-2.0-0.dll   "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libgraphite2.dll    "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libiconv-2.dll      "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libicudt61.dll      "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libicuin61.dll      "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libicuuc61.dll      "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libintl-8.dll       "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libjpeg-8.dll       "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libpcre-1.dll       "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libpcre2-16-0.dll   "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libpng16-16.dll     "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/zlib1.dll           "${CMAKE_CURRENT_BINARY_DIR}/package"
        # fluidsynth dependencies
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libfluidsynth-1.dll "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libportaudio-2.dll  "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libreadline7.dll    "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libsndfile-1.dll    "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libFLAC-8.dll       "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libogg-0.dll        "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libspeex-1.dll      "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libtermcap-0.dll    "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libvorbis-0.dll     "${CMAKE_CURRENT_BINARY_DIR}/package"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${DEPENDENCY_BINARY_DIR}/libvorbisenc-2.dll  "${CMAKE_CURRENT_BINARY_DIR}/package"
    )
    # give all the files previously gathered to the installer
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package/" DESTINATION ".")
endif()

set(CPACK_PACKAGE_VENDOR "MIDILab")
set(CPACK_PACKAGE_VERSION "${MIDILAB_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/src/data/logo.ico")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/jberthault/MIDILab")
set(CPACK_NSIS_HELP_LINK "https://github.com/jberthault/MIDILab")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MIDILab.lnk' '$INSTDIR\\\\MIDILab.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\MIDILab.lnk'")
include(CPack)
