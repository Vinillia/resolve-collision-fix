cmake_minimum_required( VERSION 3.10.0 )

function(add_extension ext_name engine)
	add_library(${ext_name} SHARED ${CMAKE_CURRENT_SOURCE_DIR}/source/extension.cpp)
	target_compile_definitions(${ext_name} PRIVATE SOURCE_ENGINE=${engine})
	
	target_sources(${ext_name} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/source/extension.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/debugoverlay.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/resolve_collision_tools.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/takedamageinfohack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/source/util_shared.cpp
	${SM_PATH}/public/asm/asm.c
	${SM_PATH}/public/CDetour/detours.cpp
	${SM_PATH}/public/libudis86/decode.c
	${SM_PATH}/public/libudis86/itab.c
	${SM_PATH}/public/libudis86/syn.c
	${SM_PATH}/public/libudis86/syn-att.c
	${SM_PATH}/public/libudis86/syn-intel.c
	${SM_PATH}/public/libudis86/udis86.c
	${SM_PATH}/public/smsdk_ext.cpp)

	target_include_directories(${ext_name} PRIVATE 
	${SDK_PATH}/common
	${SDK_PATH}/public
	${SDK_PATH}/public/tier0
	${SDK_PATH}/public/tier1
	${SDK_PATH}/game/shared
	${SDK_PATH}/game/server
	${SDK_PATH}/public
	${SDK_PATH}/public/engine
	${SDK_PATH}/public/game/server
	${SDK_PATH}/public/tier0
	${SDK_PATH}/public/tier1
	${SDK_PATH}/mathlib/
	${SDK_PATH}/public/mathlib/
	${SM_PATH}/core
	${SM_PATH}/public
	${SM_PATH}/public/amtl
	${SM_PATH}/public/amtl/amtl
	${SM_PATH}/public/asm
	${SM_PATH}/public/jit
	${SM_PATH}/public/jit/x86
	${SM_PATH}/sourcepawn/include/
	${MM_PATH}/core
	${MM_PATH}/core/sourcehook
	source
	source/sdk)

    if (UNIX)
		set_target_properties(${ext_name} PROPERTIES COMPILE_OPTIONS "-m32" LINK_FLAGS "-m32")

		target_compile_options(${ext_name} PRIVATE
		-Wall
		-Wno-delete-non-virtual-dtor
		-Wno-invalid-offsetof
		-Wno-overloaded-virtual
		-Wno-reorder
		-Wno-sign-compare
		-Wno-unknown-pragmas
		-Wno-unused
		-Wregister
		-fno-strict-aliasing
		-fpermissive
		-Wregister
		-fvisibility-inlines-hidden
		-fvisibility=hidden
		-m32
		-march=pentium3
		-mmmx
		-msse)

		add_compile_definitions(
			_LINUX
			stricmp=strcmp
			_vsnprintf=vsnprintf
			_GLIBCXX_USE_CXX11_ABI=0)
		
		target_link_libraries(${ext_name} PRIVATE 
		${SDK_PATH}/lib/linux/mathlib_i486.a
		${SDK_PATH}/lib/linux/tier1_i486.a
		${SDK_PATH}/lib/linux/tier2_i486.a
		${SDK_PATH}/lib/linux/tier3_i486.a
		${SDK_PATH}/lib/linux/libtier0_srv.so
		${SDK_PATH}/lib/linux/libvstdlib_srv.so
		${SDK_PATH}/lib/linux/mathlib_i486.so)

		#target_compile_options(${ext_name} PUBLIC -static-libstdc++ -stdlib=libstdc++)
        target_link_options(${ext_name} PRIVATE -static-libstdc++ -static-libgcc)
	
	else()
		add_compile_definitions(
			_ITERATOR_DEBUG_LEVEL=0
			_CRT_SECURE_NO_DEPRECATE
			_CRT_SECURE_NO_WARNINGS
			_CRT_NONSTDC_NO_DEPRECATE
			WIN32)

		target_compile_options(${ext_name} PRIVATE
			"/W3"
			"/EHsc"
			"/GR-"
			"/TP")

		target_link_libraries(${ext_name} PRIVATE 
		${SDK_PATH}/lib/public/tier0.lib
		${SDK_PATH}/lib/public/tier1.lib
		${SDK_PATH}/lib/public/vstdlib.lib
		${SDK_PATH}/lib/public/mathlib.lib
		legacy_stdio_definitions.lib)
    endif()

	set_target_properties(${ext_name} PROPERTIES POSITION_INDEPENDENT_CODE True)

	set_target_properties(${ext_name} PROPERTIES CXX_STANDARD 17)
    set_target_properties(${ext_name} PROPERTIES CXX_STANDARD_REQUIRED ON)

    set_target_properties(${ext_name} PROPERTIES PREFIX "")
endfunction()
