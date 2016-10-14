project(WiseTagger)
cmake_minimum_required(VERSION 3.4)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(STATUS "Source Directory: " ${CMAKE_SOURCE_DIR})
	message(STATUS "Build Directory:  " ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

if (NOT CMAKE_BUILD_TYPE)
	message(STATUS "Build type not set, using \"Release\"")
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(APP_VERSION "0.5.2")
set(TARGET_PRODUCT "WiseTagger")
set(TARGET_COMPANY "catgirl")

set(wt_srcs
	src/main.cpp
	src/file_queue.cpp
	src/input.cpp
	src/multicompleter.cpp
	src/picture.cpp
	src/reverse_search.cpp
	src/settings_dialog.cpp
	src/statistics.cpp
	src/tagger.cpp
	src/window.cpp
	util/misc.cpp
	util/open_graphical_shell.cpp
	resources/resources.qrc
)
if(WIN32)
	set(SUBSYSTEM WIN32)
	set(WINEXTRAS Qt5::WinExtras)
	set(WINEXTRAS_PKG WinExtras)
endif()
find_package(Qt5 REQUIRED COMPONENTS Core Gui Network Widgets ${WINEXTRAS_PKG})

qt5_wrap_ui(wisetagger_ui_hdrs
	ui/settings.ui
)
add_executable(wisetagger ${SUBSYSTEM} ${wt_srcs} ${wisetagger_ui_hdrs} ${wisetagger_rc_srcs})
target_link_libraries(wisetagger Qt5::Core Qt5::Gui Qt5::Network Qt5::Widgets ${WINEXTRAS})

find_package(Git)
if(GIT_FOUND)
	execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE TARGET_REVISION  OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE TARGET_DEVBUILD)
	target_compile_definitions(wisetagger PRIVATE USE_REVISION_INFO)
	string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M")
	set(TGT_REV_LINK "<a href=\\\"https://bitbucket.org/catgirl/wisetagger/commits/${TARGET_REVISION}\\\">${TARGET_REVISION}</a>")
	if(TARGET_DEVBUILD)
		set(DEVBUILD " (WIP build)")
	endif()
	set(BUILD_FROM "on ${BUILD_DATE} from commit ${TGT_REV_LINK}${DEVBUILD}")
endif()
configure_file("util/project_info.h.in" "util/project_info.h")

if(WIN32)
	target_compile_definitions(wisetagger PRIVATE UNICODE)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(wisetagger PRIVATE -Wall -Wextra)
endif()

set_target_properties(wisetagger PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

install(TARGETS wisetagger 
	ARCHIVE DESTINATION lib 
	LIBRARY DESTINATION lib 
	RUNTIME DESTINATION bin
)
