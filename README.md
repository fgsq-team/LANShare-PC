# LANShare-PC

LANShare 是一款跨平台的局域网文件传输与即时通讯工具，支持在 Windows、macOS 和 Linux 之间快速共享文件、文件夹和文本消息，无需连接互联网。

## 功能特性

- **文件/文件夹传输** — 支持发送文件与文件夹，带实时进度显示，可拖拽发送
- **文本消息** — 局域网内设备间即时发送文本消息，支持剪切板共享
- **设备自动发现** — 基于 UDP 广播自动发现同一局域网内的在线设备
- **Web 端支持** — 内置 HTTP 服务器与 WebSocket，支持通过浏览器访问和收发文件
- **AES 加密** — 通讯数据支持 AES-256 加密传输，保障数据安全
- **媒体控制** — 支持远程控制媒体播放（暂停、上一曲、下一曲、静音等）
- **主题切换** — 支持亮色/暗色主题，可跟随系统
- **系统托盘** — 支持最小化到系统托盘，后台运行
- **消息持久化** — 聊天记录可保存到本地数据库
- **多设备类型** — 支持识别 Windows、macOS、Linux、Android、iOS、Web 等设备类型
- **开机自启** — 支持开机自动启动

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI 框架 | Qt 6 (Widgets, Network, Sql) |
| 构建系统 | CMake ≥ 3.22.1 |
| HTTP 服务器 | 自研 HTTP/WebSocket 服务器 |
| 压缩 | miniz / zip |
| 加密 | AES-256 (QAES encryption) |
| 数据库 | Qt SQL (SQLite) |

## 项目结构

```
LANShare-PC/
├── src/              # 主程序源码（UI、业务逻辑、设备管理）
├── tools/            # 网络工具库（TCP/UDP 客户端与服务端、编解码等）
├── httpserver/       # 内置 HTTP/WebSocket 服务器
├── web/              # Web 端前端页面（HTML/CSS/JS）
├── zip/              # 压缩/解压库（miniz）
├── icons/            # 应用图标资源
├── img/              # UI 图片资源
├── style/            # QSS 主题样式文件
├── ffmpeg/           # FFmpeg 头文件与库（可选，媒体处理）
├── build-tools/      # 构建辅助工具（资源打包等）
└── translations/     # 翻译文件
```

## 构建指南

### 环境要求

- **CMake** ≥ 3.22.1
- **Qt 6**（需包含 Core、Gui、Widgets、Network、Sql 模块）
- **C++17** 兼容编译器（GCC / Clang / MinGW）

### Windows（MinGW）

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j$(nproc)
```

> Windows 构建会自动将 Qt 运行时 DLL 复制到输出目录。

### macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

> Release 模式下会自动生成 `.app` 包并使用 `macdeployqt` 打包为 `.dmg`。

### Linux

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Qt 路径配置

如果 CMake 找不到 Qt，请修改 `CMakeLists.txt` 中对应平台的 `CMAKE_PREFIX_PATH`：

```cmake
# Windows
set(CMAKE_PREFIX_PATH C:/Qt/6.8.0/mingw_64)
# macOS
set(CMAKE_PREFIX_PATH /path/to/Qt/clang_64)
# Linux
set(CMAKE_PREFIX_PATH /path/to/Qt/gcc_64)
```

## 使用说明

1. 确保所有设备处于同一局域网内
2. 启动 LANShare 后，程序会自动通过 UDP 广播发现其他设备
3. 在设备列表中选择目标设备，即可发送文件、文件夹或文本消息
4. 接收文件时可选择保存路径（默认为 `~/LANShare/`）
5. 可通过 Web 服务在浏览器中访问设备列表并传输文件

### 默认端口

| 协议 | 端口 |
|------|------|
| UDP（设备发现） | 4573 |
| TCP（文件传输） | 5856 |

## 许可证

本项目基于 [Apache License 2.0](LICENSE) 开源。

## 相关链接

- 官方网站：[lanshares.com](http://lanshares.com)
- 项目仓库：[GitHub - fgsq-team/LANShare-PC](https://github.com/fgsq-team/LANShare-PC)
