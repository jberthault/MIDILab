# ================
# Find FluidSynth
# ================
#
# The following variables may help finding include directories and library :
#     FluidSynth_INCLUDE_DIR_HINTS
#         A list of directories which may contain:
#         ${HINT}/fluidsynth.h
#         ${HINT}/fluidsynth/version.h
#     FluidSynth_LIBRARY_HINTS
#         A list of directories which may contain:
#         ${HINT}/libfluidsynth.a OR
#         ${HINT}/libfluidsynth.dll OR
#         ${HINT}/libfluidsynth.dll.a OR
#         ${HINT}/libfluidsynth.so
#

include(FindPackageHandleStandardArgs)

find_library(FluidSynth_LIBRARY "fluidsynth" PATHS ${FluidSynth_LIBRARY_HINTS})
find_path(FluidSynth_INCLUDE_DIR "fluidsynth.h" PATHS ${FluidSynth_INCLUDE_DIR_HINTS})
find_path(FluidSynth_VERSION_DIR "fluidsynth/version.h" PATHS ${FluidSynth_INCLUDE_DIR_HINTS})

# looking for fluidsynth version
if (EXISTS "${FluidSynth_VERSION_DIR}/fluidsynth/version.h")
    file(READ "${FluidSynth_VERSION_DIR}/fluidsynth/version.h" FluidSynth_VERSION_FILE)
    string(REGEX REPLACE "(.*)#define FLUIDSYNTH_VERSION[ \t]+\"(.*)\"(.*)" "\\2" FluidSynth_VERSION ${FluidSynth_VERSION_FILE})
    unset(FluidSynth_VERSION_FILE)
endif()

find_package_handle_standard_args(FluidSynth
    REQUIRED_VARS FluidSynth_LIBRARY FluidSynth_INCLUDE_DIR
    VERSION_VAR FluidSynth_VERSION
)

set(FluidSynth_LIBRARIES ${FluidSynth_LIBRARY})
set(FluidSynth_INCLUDE_DIRS ${FluidSynth_INCLUDE_DIR} ${FluidSynth_VERSION_DIR})
list(REMOVE_DUPLICATES FluidSynth_INCLUDE_DIRS)

add_library(FluidSynth::FluidSynth UNKNOWN IMPORTED)
set_target_properties(FluidSynth::FluidSynth PROPERTIES
    IMPORTED_LOCATION "${FluidSynth_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FluidSynth_INCLUDE_DIRS}"
    VERSION "${FluidSynth_VERSION}"
)
