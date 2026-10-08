cmake_minimum_required(VERSION 3.22.1)
if (APPLE)
    #    设置支持的最低MacOS版本
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "Minimum OS X deployment version")
endif ()

project(LANShare)

# 添加 Qt 版本选择开关
option(USE_QT6 "Use Qt6 instead of Qt5" ON)

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

if (WIN32)
    #    MinGW 默认 _WIN32_WINNT 过低，会导致 ws2tcpip.h 不声明 inet_ntop 等 Vista+ API
    add_compile_definitions(_WIN32_WINNT=0x0600)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    if(USE_QT6)
        set(CMAKE_PREFIX_PATH C:/Qt/6.12.0/mingw_64)
    else()
        set(CMAKE_PREFIX_PATH C:/Qt5/Qt5.14.2/5.14.2/mingw73_64)
    endif()
    if (${CMAKE_BUILD_TYPE} MATCHES "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -mwindows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -mwindows ")
    endif ()
elseif (APPLE)
    if(USE_QT6)
        set(CMAKE_PREFIX_PATH /Users/user/Qt6.12.4/5.12.4/clang_64)
    else()
        set(CMAKE_PREFIX_PATH /Users/user/Qt5.15.2/5.15.2/clang_64)
    endif()
elseif (UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    if(USE_QT6)
        set(CMAKE_PREFIX_PATH /home/user/Downloads/qt-everywhere-src-5.12.4/Qt6.12.4_static)
    else()
        set(CMAKE_PREFIX_PATH /home/user/Qt/6.8.3/gcc_64)
    endif()
else ()
    message(STATUS "Unknown platform")
endif ()

include_directories(
        "include"
        "src/version/four/include"
        "src/utils/include"
        "src/include"
        "src/manager/include"
        "tools/include"
        "httpserver/include"
        "zip/"
        #        "ffmpeg/include"
)

file(GLOB_RECURSE FILE_SOURCES tools/*.c*)
file(GLOB_RECURSE HTTP_SERVER_FILE_SOURCES httpserver/*.*)
file(GLOB_RECURSE SRC_FILE_SOURCES src/*.*)
file(GLOB_RECURSE ZIP_FILE_SOURCES zip/*.*)

# 根据开关选择 Qt 版本
if(USE_QT6)
    find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql Svg LinguistTools REQUIRED)
else()
    find_package(Qt5 COMPONENTS Core Gui Widgets Network Sql Svg LinguistTools REQUIRED)
endif()

# 生成翻译文件
set(TS_FILES 
    ${CMAKE_SOURCE_DIR}/translations/lanshare_en.ts
)

if(USE_QT6)
    qt6_add_translation(QM_FILES ${TS_FILES})
else()
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

add_executable(
        ${PROJECT_NAME}
        ${FILE_SOURCES}
        ${HTTP_SERVER_FILE_SOURCES}
        ${SRC_FILE_SOURCES}
        ${ZIP_FILE_SOURCES}
        ${QM_FILES}
        res.qrc
        httpserver/CaseInsensitiveMap.cpp
        httpserver/include/CaseInsensitiveMap.h
        src/version/four/FileSend.cpp
        src/version/four/FileServer.cpp
        src/version/four/include/FileServer.h
        src/version/four/FileSend.cpp
        src/version/four/include/FileSend.h
        src/version/four/CustomDataStream.cpp
        src/version/four/include/CustomDataStream.h
        src/version/four/FileTransfer.cpp
        src/version/four/include/FileTransfer.h
        src/version/four/include/ProgressCallback.h
        src/TranslationManager.cpp
        src/include/TranslationManager.h
        src/utils/include/FileUtils.h
)

# 资源打包到二进制文件工具
add_executable(res2cpp build-tools/res2cpp/res2cpp.cpp)

if (WIN32)
    set(SOURCE_FILES icons/win/LANShare.rc)
    target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})
    if(USE_QT6)
        target_link_libraries(
                ${PROJECT_NAME}
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
                Qt6::Network
                Qt6::Sql
                Qt6::Svg
                wsock32
                iphlpapi
                Ws2_32
                ole32
                wlanapi
        )
    else()
        target_link_libraries(
                ${PROJECT_NAME}
                Qt5::Core
                Qt5::Gui
                Qt5::Widgets
                Qt5::Network
                Qt5::Sql
                Qt5::Svg
                wsock32
                iphlpapi
                Ws2_32
                ole32
                wlanapi
        )
    endif()
else ()
    if(USE_QT6)
        target_link_libraries(
                ${PROJECT_NAME}
                Qt6::Core
                Qt6::Gui
                Qt6::Widgets
                Qt6::Network
                Qt6::Sql
                Qt6::Svg
                pthread
        )
    else()
        target_link_libraries(
                ${PROJECT_NAME}
                Qt5::Core
                Qt5::Gui
                Qt5::Widgets
                Qt5::Network
                Qt5::Sql
                Qt5::Svg
                pthread
        )
    endif()
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

    if(USE_QT6)
        if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying: ${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/"
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        endif ()
        foreach (QT_LIB Core Gui Widgets Sql Network Svg)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying: ${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/"
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endforeach (QT_LIB)
    else()
        if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying: ${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/"
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        endif ()
        foreach (QT_LIB Core Gui Widgets Sql Network Svg)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying: ${QT_INSTALL_PATH}/bin/Qt5${QT_LIB}${DEBUG_SUFFIX}.dll -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/"
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/bin/Qt5${QT_LIB}${DEBUG_SUFFIX}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endforeach (QT_LIB)
    endif()
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
    if(USE_QT6)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_PREFIX_PATH}/bin/macdeployqt
                ARGS ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PROJECT_NAME}.app -dmg
                COMMENT "Running macdeployqt"
        )
    else()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_PREFIX_PATH}/bin/macdeployqt
                ARGS ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PROJECT_NAME}.app -dmg
                COMMENT "Running macdeployqt"
        )
    endif()
endif ()
