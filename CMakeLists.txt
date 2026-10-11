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

# ===== 调试增强（仅 Debug 生效，用于定位堆损坏/越界/UB）=====
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (Debug)" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer (Debug, MinGW 支持有限)" OFF)
if (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MSVC)
    # 更详细的调试信息 + 关闭优化 + 保留帧指针，让崩溃栈回溯更准确
    add_compile_options(-g3 -O0 -fno-omit-frame-pointer -fasynchronous-unwind-tables)
    # -fstack-protector-all：仅对 Qt6(GCC13，SSP 符号已并入 libc/libgcc，无外部 DLL 依赖)启用。
    # Qt5(GCC 7.3)若启用会引入外部 libssp-0.dll，脱离 IDE 的 PATH 直接运行 exe 会报“找不到 libssp-0.dll”，
    # 且它只是调试辅助、非必需，故对 Qt5 不加。
    if (USE_QT6)
        add_compile_options(-fstack-protector-all)
        add_link_options(-fstack-protector-all)
    endif ()
    # libstdc++ 轻量边界检查（不改变 ABI，安全；切勿用 _GLIBCXX_DEBUG，会与 Qt release 版 libstdc++ 的 ABI 冲突导致堆损坏）
    add_compile_definitions(_GLIBCXX_ASSERTIONS)
    if (ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
    endif ()
    if (ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
        message(WARNING "MinGW 的 ASan 支持有限，若链接失败或二次崩溃，请改用 Windows Page Heap(gflags /p /enable LANShare.exe /full)")
    endif ()
endif ()

# ===== MSVC 调试增强（若切换到 MSVC kit，可直接用官方 ASan）=====
if (CMAKE_BUILD_TYPE MATCHES "Debug" AND MSVC)
    add_compile_options(/Zi /Od /fsanitize=address)
    add_link_options(/DEBUG)
endif ()

if (WIN32)
    #    MinGW 默认 _WIN32_WINNT 过低，会导致 ws2tcpip.h 不声明 inet_ntop 等 Vista+ API
    add_compile_definitions(_WIN32_WINNT=0x0600)
    # 运行库链接策略：
    # - Sanitizer 需要动态运行时，静态链接会与其 runtime 冲突，开启 sanitizer 时一律去掉静态运行时；
    # - Qt5(MinGW GCC 7.3)：Qt5*.dll 动态依赖 libstdc++-6.dll / libgcc_s_seh-1.dll，运行时必然把共享
    #   版 C++ 运行库拉进进程。老 MinGW 不允许「exe 静态内联 + Qt 动态」两份 libstdc++ 混用，混用会造成
    #   new/delete 与堆管理器错配 -> 表现为任意 delete 处的延迟 SIGTRAP 堆损坏。故这里让 exe 复用 Qt5
    #   已加载的共享运行时（不加 -static），进程内只保留一份 C++ 运行库；
    # - Qt6(GCC 13)容忍静态+动态混用，维持原静态链接策略不变。
    if (ENABLE_UBSAN OR ENABLE_ASAN)
        set(CMAKE_EXE_LINKER_FLAGS "")
    elseif (NOT USE_QT6)
        set(CMAKE_EXE_LINKER_FLAGS "")
    else ()
        set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    endif ()
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

    # 复制运行所需插件到 ./plugins/<类型>/（App 通过 QApplication::addLibraryPath("./plugins") 查找）：
    # sqlite 数据库驱动、svg 图标/图像引擎、windows 原生样式（QSS 主题基座）。
    # 仅 style 插件名随 Qt 版本不同，其余同名；用 EXISTS 守护，缺失则跳过并告警。
    if (USE_QT6)
        set(NEEDED_PLUGINS
                "sqldrivers/qsqlite.dll"
                "styles/qmodernwindowsstyle.dll"
                "iconengines/qsvgicon.dll"
                "imageformats/qsvg.dll"
                "imageformats/qjpeg.dll"
                "imageformats/qico.dll"
                "imageformats/qgif.dll")
    else ()
        set(NEEDED_PLUGINS
                "sqldrivers/qsqlite.dll"
                "styles/qwindowsvistastyle.dll"
                "iconengines/qsvgicon.dll"
                "imageformats/qsvg.dll"
                "imageformats/qjpeg.dll"
                "imageformats/qico.dll"
                "imageformats/qgif.dll")
    endif ()
    foreach (_pl ${NEEDED_PLUGINS})
        if (EXISTS "${QT_INSTALL_PATH}/plugins/${_pl}")
            get_filename_component(_pldir "${_pl}" DIRECTORY)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/${_pldir}/")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying plugin: ${_pl} -> plugins/${_pldir}/"
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/plugins/${_pl}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/${_pldir}/")
        else ()
            message(WARNING "Qt 插件未找到，跳过拷贝: ${QT_INSTALL_PATH}/plugins/${_pl}")
        endif ()
    endforeach ()

    # Qt5 构建：exe 已改为动态链接 C++ 运行时（与 Qt5Core 共用同一份），必须随 exe 放一份
    # 与编译器版本严格匹配的 MinGW 运行库 DLL。必须从“编译 exe 的那个 MinGW 的 bin”取，
    # 切勿从别的 MinGW 拷（版本不一致会导致 _emutls_v.* 入口点错误或堆损坏延迟 SIGTRAP）。
    if (NOT USE_QT6)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach (_rt libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            if (EXISTS "${MINGW_BIN_DIR}/${_rt}")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy
                        "${MINGW_BIN_DIR}/${_rt}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
            endif ()
        endforeach ()
    endif ()

    # 拷贝 sanitizer 运行时（位于 MinGW 编译器 bin 目录）
    if (ENABLE_UBSAN OR ENABLE_ASAN)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(_SAN_DLLS "")
        if (ENABLE_UBSAN)
            file(GLOB _UBSAN_DLLS "${MINGW_BIN_DIR}/libubsan*.dll")
            list(APPEND _SAN_DLLS ${_UBSAN_DLLS})
        endif ()
        if (ENABLE_ASAN)
            file(GLOB _ASAN_DLLS "${MINGW_BIN_DIR}/libasan*.dll")
            list(APPEND _SAN_DLLS ${_ASAN_DLLS})
        endif ()
        foreach (_dll ${_SAN_DLLS})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying sanitizer runtime: ${_dll}"
                    COMMAND ${CMAKE_COMMAND} -E copy "${_dll}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        endforeach ()
    endif ()
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
