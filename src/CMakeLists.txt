set(CMAKE_C_FLAGS "-I/usr/local/include -L/usr/local/lib -g -W -Wall -DNVAGENT_VERSION='\"${CPACK_PACKAGE_VERSION}\"' ${compiler_options}")
set(CMAKE_CXX_FLAGS "-I/usr/local/include -L/usr/local/lib -g -W -Wall -DNVAGENT_VERSION='\"${CPACK_PACKAGE_VERSION}\"' ${compiler_options}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(tests)

if (APPLE)
	if (NOT OPENSSL_ROOT_DIR)
		set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
	endif()
	include_directories("${OPENSSL_ROOT_DIR}/include")
	link_directories("${OPENSSL_ROOT_DIR}/lib")
endif()

if (NOT TAPCFG_ROOT_DIR)
	set(TAPCFG_ROOT_DIR ${CMAKE_SOURCE_DIR}/tapcfg)
endif()

include_directories("${TAPCFG_ROOT_DIR}/src/include/")

if (WIN32)
	include_directories("${LIBCURL_ROOT_DIR}/include/"
		"${LIBRESSL_ROOT_DIR}/include/"
		"${LIBEVENT_ROOT_DIR}/include/"
		"${LIBJANSSON_ROOT_DIR}/src/"
		"${CMAKE_SOURCE_DIR}/libnv/src/")
	set (win_rc_icon gui/rc/icon.rc)
else()
	set (win_rc_icon "")
endif()

set(nvagent_sources
	agent.c
	control.c
	ndb.c
	switch.c
	sysname.c)

message(STATUS "CMAKE_SOURCE_DIR" ${CMAKE_SOURCE_DIR})

link_directories("${TAPCFG_ROOT_DIR}/build")

if (WITH_GUI)

	set(gui_sources gui/main.cpp)

	include_directories("${CMAKE_SOURCE_DIR}/wx/include/")
	add_executable(netvfy-agent ${nvagent_soruces} ${gui_sources})
	set_target_properties(netvfy-agent PROPERTIES LINK_FLAGS "-mwindows")

	if (WIN32)
		set(GUI_LIBS
			"${CMAKE_SOURCE_DIR}/wx/lib/gcc810_dll/libwxbase31u.a"
			"${CMAKE_SOURCE_DIR}/wx/lib/gcc810_dll/libwxmsw31u_core.a")
	elseif (APPLE)

		# TODO use output to set flags
		# ${CMAKE_SOURCE_DIR}/wx/build-cocoa-debug/wx-config --cxxflags
		# ${CMAKE_SOURCE_DIR}/wx/build-cocoa-debug/wx-config --libs core

		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
			-I${CMAKE_SOURCE_DIR}/wx/build-cocoa-debug/lib/wx/include/osx_cocoa-unicode-3.1 \
			-I${CMAKE_SOURCE_DIR}/wx/include \
			-D_FILE_OFFSET_BITS=64 -DWXUSINGDLL -D__WXMAC__ -D__WXOSX__ -D__WXOSX_COCOA__")

		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
			-L${CMAKE_SOURCE_DIR}/wx/build-cocoa-debug/lib \
			-framework IOKit -framework Carbon -framework Cocoa \
			-framework AudioToolbox -framework System -framework OpenGL -lwx_osx_cocoau_core-3.1 -lwx_baseu-3.1")
	endif()
else()
	set(cli_sources cli/main.c)
	add_executable(netvfy-agent MACOSX_BUNDLE ${nvagent_sources} ${cli_sources})
endif()

if (APPLE)
	target_link_libraries(netvfy-agent
		${TAPCFG_ROOT_DIR}/build/libtapcfg.a
		curl
		nv
		ssl
		crypto
		jansson
		event_core
		event_extra
		event_openssl)
elseif (WIN32)
	file(GLOB LIBS_FOR_WIN32
		${LIBCURL_ROOT_DIR}/lib/.libs/libcurl*dll
		${LIBRESSL_ROOT_DIR}/ssl/.libs/libssl*dll
		${LIBRESSL_ROOT_DIR}/crypto/.libs/libcrypto*dll
		${LIBJANSSON_ROOT_DIR}/src/.libs/libjansson*dll
		${LIBEVENT_ROOT_DIR}/.libs/libevent*dll)

	target_link_libraries(netvfy-agent
		ws2_32
		nv
		${LIBS_FOR_WIN32}
		${OPENSSL_LIBRARIES}
		${TAPCFG_ROOT_DIR}/build/tapcfg.dll
		${GUI_LIBS})
else()
	target_link_libraries(netvfy-agent
		curl
		${TAPCFG_ROOT_DIR}/build/libtapcfg.a
		nv
		ssl
		crypto
		event_core
		event_extra
		event_openssl
		jansson)
endif()

install(TARGETS netvfy-agent
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime)

if (APPLE)
    set(APPS "\${CMAKE_INSTALL_PREFIX}/${apple_package}")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/osx/tuntap_20111101.pkg DESTINATION . USE_SOURCE_PERMISSIONS COMPONENT Runtime)
    if (WITH_GUI)
        install(CODE "
            file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"\")
            " COMPONENT Runtime)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/osx/NetVirt-Agent-launcher.app DESTINATION . USE_SOURCE_PERMISSIONS COMPONENT Runtime)
        set(DIRS ${QT5_LIBRARY_DIRS})
        install(CODE "
            file(GLOB_RECURSE QTPLUGINS
              \"\${CMAKE_INSTALL_PREFIX}/${library_dest_dir}/plugins/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
            set(BU_CHMOD_BUNDLE_ITEMS 1)
            include(BundleUtilities)
            fixup_bundle(\"${APPS}\" \"\${QTPLUGINS}\" \"${DIRS}\")
            " COMPONENT Runtime)
    endif()
endif()
