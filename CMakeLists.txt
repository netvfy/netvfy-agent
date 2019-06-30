cmake_minimum_required(VERSION 2.6)
project(netvfy)

SET(CPACK_PACKAGE_VERSION "1.0.0")

enable_testing()

# Using Hardening Options
# https://wiki.debian.org/Hardening
set(HARDENING OPTIONS_COMMON
    -W
    -Wall
    -fstack-protector-all
    --param ssp-buffer-size=4
    -Wformat
    -Wformat-security
    -Werror=format-security
    -D_FORTIFY_SOURCE=2
)

if (WITH_GUI)
	set(CPACK_PACKAGE_NAME "netvfy-agent-gui")
	set(USE_THREAD "-DUSE_THREAD")
else()
	set(CPACK_PACKAGE_NAME "netvfy-agent-cli")
endif()

if (WIN32)
    set(compiler_options "${HARDENING_OPTIONS_COMMON} -fPIE -O1 ${USE_THREAD}")
elseif(UNIX)
    set(compiler_options "${HARDENING_OPTIONS_COMMON} -O1 ${USE_THREAD}")
endif()

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if(APPLE)
	set(apple_package netvfy-agent.app)
	set(library_dest_dir ${apple_package}/Contents/MacOS)
	set(qtconf_dest_dir ${apple_package}/Contents/Resources)
else()
	set(library_dest_dir lib)
endif()

option (WITH_GUI "WITH_GUI" OFF)

add_subdirectory(libnv)
include_directories("./libnv/src/")

add_subdirectory(src)

if (APPLE)
	set(CPACK_GENERATOR "DragNDrop")
else()
	set(CPACK_GENERATOR "DEB")
	if (WITH_GUI)
		install(FILES ./src/gui/rc/nvagent.svg DESTINATION /usr/share/icons/hicolor/scalable/apps/)
		install(FILES ./xdesktop/nvagent.desktop DESTINATION /usr/share/applications/)
	endif()
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Netvfy Agent - Connect to your secure virtual network")
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcap2-bin")
	set(CPACK_DEBIAN_PACKAGE_SECTION "net")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Netvfy core team <help@netvfy.com>")
	set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_CURRENT_SOURCE_DIR}/postinst; ${CMAKE_CURRENT_SOURCE_DIR}/postrm)
endif()

include(CPack)
