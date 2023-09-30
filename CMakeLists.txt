cmake_minimum_required( VERSION 3.10.0 )

project(l4d2_resolve_collision_fix)

if(MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Arch must be Win32")
endif()

include("cmake/sourcemod.cmake")

set(SDK_PATH $ENV{HL2SDKL4D2})
set(MM_PATH $ENV{MMSOURCE})
set(SM_PATH $ENV{SOURCEMOD})

add_compile_definitions(SE_LEFT4DEAD2=16 SE_LEFT4DEAD=8 SOURCE_ENGINE=16 SE_L4D2=16 SE_ORANGEBOX=3 SE_ALIENSWARM=17)

add_extension(l4d2_resolve_collision_fix.ext.2.l4d2 9)