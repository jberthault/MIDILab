project(MIDILab)

cmake_minimum_required(VERSION 3.1.3)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

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
qt5_add_resources(QRC_FILES "src/data.qrc")

add_executable(${PROJECT_NAME} ${SRC_FILES} ${QRC_FILES} "README.md" "LICENSE")

# --------------------
# target configuration
# --------------------

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

if (MINGW)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DEPENDENCY_ARCHIVE "dependencies-MinGW64_Release")
    else()
        set(DEPENDENCY_ARCHIVE "dependencies-MinGW32_Release")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${PROJECT_SOURCE_DIR}/packages/${DEPENDENCY_ARCHIVE}.tar.gz"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DEPENDENCY_ARCHIVE}/" DESTINATION ".")
endif()

set(CPACK_PACKAGE_VENDOR "MIDILab")
SET(CPACK_PACKAGE_VERSION "${MIDILAB_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/src/data/logo.ico")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/jberthault/MIDILab")
set(CPACK_NSIS_HELP_LINK "https://github.com/jberthault/MIDILab")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MIDILab.lnk' '$INSTDIR\\\\MIDILab.exe'")
include(CPack)
