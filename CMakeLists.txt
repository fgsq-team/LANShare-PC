cmake_minimum_required(VERSION 3.22.1)
if (APPLE)
    #    设置支持的最低MacOS版本
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "Minimum OS X deployment version")
endif ()

project(LANShare)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_BUILD_TYPE MATCHES "Release")
    #    发行版不输出debug信息
    add_compile_definitions(QT_NO_DEBUG_OUTPUT)
    add_definitions(-DRELEASE)
else ()
    add_definitions(-DDEBUG)
endif ()

# 设置你的静态库路径
#set(LIBS_DIR ${CMAKE_SOURCE_DIR}/ffmpeg/lib)
#
## 添加你的静态库
#add_library(libavcodec STATIC IMPORTED)
#set_target_properties(libavcodec PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libavcodec.dll.a)
#
#add_library(libavformat STATIC IMPORTED)
#set_target_properties(libavformat PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libavformat.dll.a)
#
#add_library(libavutil STATIC IMPORTED)
#set_target_properties(libavutil PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libavutil.dll.a)
#
#add_library(libswscale STATIC IMPORTED)
#set_target_properties(libswscale PROPERTIES IMPORTED_LOCATION ${LIBS_DIR}/libswscale.dll.a)

if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    set(CMAKE_PREFIX_PATH D:/AndroidToolchain/Qt/6.8.0/mingw_64)
    if (${CMAKE_BUILD_TYPE} MATCHES "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -mwindows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -mwindows ")
    endif ()
elseif (APPLE)
    set(CMAKE_PREFIX_PATH /Users/user/Qt6.12.4/5.12.4/clang_64)
elseif (UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    set(CMAKE_PREFIX_PATH /home/user/Qt/6.8.3/gcc_64)
else ()
    message(STATUS "Unknown platform")
endif ()

include_directories(
        "include"
        "src/utils/include"
        "src/include"
        "tools/include"
        "httpserver/include"
        "zip/"
        #        "ffmpeg/include"
)

file(GLOB_RECURSE FILE_SOURCES tools/*.c*)
file(GLOB_RECURSE HTTP_SERVER_FILE_SOURCES httpserver/*.*)
file(GLOB_RECURSE SRC_FILE_SOURCES src/*.*)
file(GLOB_RECURSE ZIP_FILE_SOURCES zip/*.*)
find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql REQUIRED)
add_executable(
        ${PROJECT_NAME}
        ${FILE_SOURCES}
        ${HTTP_SERVER_FILE_SOURCES}
        ${SRC_FILE_SOURCES}
        ${ZIP_FILE_SOURCES}
        res.qrc
)
# 资源打包到二进制文件工具
add_executable(res2cpp build-tools/res2cpp/resource.cpp build-tools/res2cpp/res2cpp.cpp)

if (WIN32)
    set(SOURCE_FILES icons/win/LANShare.rc)
    target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
    target_link_libraries(
            ${PROJECT_NAME}
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Sql
            wsock32
            iphlpapi
            Ws2_32
            ole32
            wlanapi
            #            libavcodec
            #            libavformat
            #            libavutil
            #            libswscale
    )
else ()
    target_link_libraries(
            ${PROJECT_NAME}
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Sql
            pthread
    )
endif ()
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets Sql Network)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
if (APPLE AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # 生成LANShare.app
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.fgsqw.lanshare"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/build/macos/Info.plist"
            MACOSX_BUNDLE_ICON_FILE "build/macos/AppIcon.icns"
    )
    # 复制 AppIcon.icns 到 .app 包中
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/build/macos/AppIcon.icns ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Resources/AppIcon.icns
            COMMENT "Copying AppIcon.icns to the bundle"
    )
    # 打包项目为dmg
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_PREFIX_PATH}/bin/macdeployqt
            ARGS ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PROJECT_NAME}.app -dmg
            COMMENT "Running macdeployqt"
    )
endif ()
