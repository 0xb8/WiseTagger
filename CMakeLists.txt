project(WiseTagger)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_VERSION "0.6.3")
set(TARGET_PRODUCT "WiseTagger")
set(TARGET_COMPANY "catgirl")


# ----- WiseTagger -----
set(wt_srcs
	src/main.cpp
	src/global_enums.cpp
	src/global_enums.h
	src/file_queue.cpp
	src/file_queue.h
	src/filter_dialog.cpp
	src/input.cpp
	src/input.h
	src/multicompleter.cpp
	src/multicompleter.h
	src/picture.cpp
	src/picture.h
	src/reverse_search.cpp
	src/reverse_search.h
	src/settings_dialog.cpp
	src/settings_dialog.h
	src/statistics.cpp
	src/statistics.h
	src/tagger.cpp
	src/tagger.h
	src/tag_parser.cpp
	src/tag_parser.h
	src/window.cpp
	src/window.h
	util/imagecache.cpp
	util/imagecache.h
	util/misc.cpp
	util/misc.h
	util/strings.cpp
	util/strings.h
	util/tag_fetcher.cpp
	util/tag_fetcher.h
	util/tag_file.cpp
	util/tag_file.h
	util/open_graphical_shell.cpp
	util/open_graphical_shell.h
	resources/resources.qrc
)
if(WIN32)
	set(SUBSYSTEM WIN32)
	set(WINEXTRAS Qt5::WinExtras)
	set(WINEXTRAS_PKG WinExtras)
endif()
find_package(Qt5 REQUIRED COMPONENTS Core Gui Network Widgets Multimedia MultimediaWidgets ${WINEXTRAS_PKG})

qt5_wrap_ui(wisetagger_ui_hdrs
	ui/settings.ui
)
add_executable(WiseTagger ${SUBSYSTEM} ${wt_srcs} ${wisetagger_ui_hdrs})
target_link_libraries(WiseTagger Qt5::Core Qt5::Gui Qt5::Network Qt5::Widgets Qt5::Multimedia Qt5::MultimediaWidgets ${WINEXTRAS})
target_compile_definitions(WiseTagger PRIVATE QT_DEPRECATED_WARNINGS)

if(CMAKE_BUILD_TYPE MATCHES Release)
	target_compile_definitions(WiseTagger PRIVATE QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT)
endif()

configure_file("util/project_info.h.in" "util/project_info.h")

if(WIN32)
	target_compile_definitions(WiseTagger PRIVATE UNICODE)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(WiseTagger PRIVATE -Wall -Wextra)
endif()

install(TARGETS WiseTagger
	RUNTIME DESTINATION bin
)

install(FILES resources/wisetagger.svg
	DESTINATION share/icons/hicolor/scalable/apps/
)

install(FILES dist/wisetagger.desktop
	DESTINATION share/applications/
)


# ----- WiseTagger CLI ----
add_executable(WiseTaggerCli
	src/main_cli.cpp
	src/tagger_commandline.cpp
	src/tagger_commandline.h
	src/tag_parser.cpp
	src/tag_parser.h
	util/strings.cpp
	util/strings.h
	util/tag_file.cpp
	util/tag_file.h
)
target_link_libraries(WiseTaggerCli Qt5::Core)
target_compile_definitions(WiseTaggerCli PRIVATE QT_DEPRECATED_WARNINGS)
if(CMAKE_BUILD_TYPE MATCHES Release)
	target_compile_definitions(WiseTaggerCli PRIVATE QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT)
endif()
if(WIN32)
	target_compile_definitions(WiseTaggerCli PRIVATE UNICODE)
endif()
install(TARGETS WiseTaggerCli
	RUNTIME DESTINATION bin
)
