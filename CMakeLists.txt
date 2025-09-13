cmake_minimum_required(VERSION 3.20)
project(libretro_opengl_lua_c)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#find_package(OpenGL REQUIRED)

# Enable FetchContent
include(FetchContent)

#================================================
# libretro-common
#================================================

# Fetch libretro-common for headers
FetchContent_Declare(
    libretro-common
    GIT_REPOSITORY https://github.com/libretro/libretro-common.git
    GIT_TAG master
)
FetchContent_MakeAvailable(libretro-common)

# ================================================
# Lua
# ================================================
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lua)

# Lua doesn't have a CMake build, so we manually compile it
# List Lua source files (core and libraries, excluding lua.c and luac.c)
set(LUA_SRC
    ${lua_SOURCE_DIR}/lapi.c
    ${lua_SOURCE_DIR}/lauxlib.c
    ${lua_SOURCE_DIR}/lbaselib.c
    ${lua_SOURCE_DIR}/lcode.c
    ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lctype.c
    ${lua_SOURCE_DIR}/ldblib.c
    ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c
    ${lua_SOURCE_DIR}/ldump.c
    ${lua_SOURCE_DIR}/lfunc.c
    ${lua_SOURCE_DIR}/lgc.c
    ${lua_SOURCE_DIR}/linit.c
    ${lua_SOURCE_DIR}/liolib.c
    ${lua_SOURCE_DIR}/llex.c
    ${lua_SOURCE_DIR}/lmathlib.c
    ${lua_SOURCE_DIR}/lmem.c
    ${lua_SOURCE_DIR}/loadlib.c
    ${lua_SOURCE_DIR}/lobject.c
    ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/loslib.c
    ${lua_SOURCE_DIR}/lparser.c
    ${lua_SOURCE_DIR}/lstate.c
    ${lua_SOURCE_DIR}/lstring.c
    ${lua_SOURCE_DIR}/lstrlib.c
    ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltablib.c
    ${lua_SOURCE_DIR}/ltm.c
    ${lua_SOURCE_DIR}/lundump.c
    ${lua_SOURCE_DIR}/lvm.c
    ${lua_SOURCE_DIR}/lzio.c
    ${lua_SOURCE_DIR}/lutf8lib.c
)

# Create a static Lua library
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})

#================================================
# LIB
#================================================

set(LIB_NAME libretro_opengl_lua_c)

# Define the shared library
add_library(${LIB_NAME} SHARED 
    ${libretro-common_SOURCE_DIR}/glsym/rglgen.c
    ${libretro-common_SOURCE_DIR}/glsym/glsym_gl.c
    src/module_gl.c
    src/lib.c
)

# Set include directories
target_include_directories(${LIB_NAME}  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include             #current project folder
    ${libretro-common_SOURCE_DIR}/include
    ${lua_SOURCE_DIR}                               # lua
)

# Link lib
target_link_libraries(${LIB_NAME} PRIVATE 
    lua                                             # lua
    opengl32                                        # opengl
)

# Ensure the output is a .dll with no prefix
set_target_properties(${LIB_NAME}  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${LIB_NAME}"
    SUFFIX ".dll"
)

# Set C standard
set_property(TARGET ${LIB_NAME}  PROPERTY C_STANDARD 99)

# Windows-specific settings
if(WIN32)
    # target_compile_definitions(libretro_gl_sample PRIVATE -D__WIN32_LIBRETRO__)
    # ws2_32 # network
    # target_link_libraries(${LIB_NAME} PRIVATE ws2_32 gdi32 user32 shell32)
    # target_link_options(${APP_NAME} PRIVATE -static-libgcc)

    target_link_options(${LIB_NAME} PRIVATE
        -static-libgcc
        # -static-libstdc++  # Uncomment if C++ code is used
        # -static           # Avoid full static linking to prevent issues with system libraries
    )
endif()