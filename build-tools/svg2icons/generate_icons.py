#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
矢量图标 -> 全平台多分辨率位图图标 生成脚本。

以 icons/lanshare.svg 为唯一图标母版（矢量，无限清晰），一次性生成：
  1. Windows 资源管理器 / 任务栏用的多分辨率 .ico（16~256 全尺寸，小尺寸 BMP + 大尺寸 PNG 编码）
  2. Qt 运行时窗口 / 托盘 / 聊天列表用的 .png
  3. 网页端 favicon 用的 .png
  4. macOS 的 AppIcon.iconset 全套 .png 与 AppIcon.icns

依赖：PySide6（用 Qt 的 Svg 模块做矢量光栅化，与程序运行时渲染结果一致）、Pillow。
用法：python build-tools/svg2icons/generate_icons.py [--svg icons/lanshare.svg] [--out build/svg2icons-preview]
"""

import argparse
import io
import struct
import sys
from pathlib import Path

import os

# Qt 的离屏平台插件：无需显示环境即可光栅化
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PIL import Image  # noqa: E402
from PySide6.QtCore import QBuffer, QRectF  # noqa: E402
from PySide6.QtGui import QColor, QGuiApplication, QImage, QPainter  # noqa: E402
from PySide6.QtSvg import QSvgRenderer  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

# Windows 图标标准尺寸：覆盖任务栏(24/32)、小图标(16)、中/大图标(48/64/96/128)、超大图标(256)，
# 以及 150%/125% 缩放下的 20/40，避免系统自行缩放导致发虚
WIN_SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

# macOS iconset 命名规范（iconutil 直接按文件名识别），@2x 为对应尺寸的两倍像素
ICONSET = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
]

# .icns 内部条目类型 -> 像素尺寸（均为 PNG 压缩，macOS 10.7+ 通用）
ICNS_TYPES = [
    (b"icp4", 16),
    (b"icp5", 32),
    (b"ic07", 128),
    (b"ic08", 256),
    (b"ic09", 512),
    (b"ic10", 1024),
    (b"ic11", 32),   # 16x16@2x
    (b"ic12", 64),   # 32x32@2x
    (b"ic13", 512),  # 256x256@2x
    (b"ic14", 1024),  # 512x512@2x
]


class SvgRasterizer:
    """基于 Qt Svg 的矢量光栅化器：把 SVG 渲染成指定尺寸的 RGBA 图像。"""

    def __init__(self, svg_path: Path):
        self._app = QGuiApplication.instance() or QGuiApplication([])
        self._renderer = QSvgRenderer(str(svg_path))
        if not self._renderer.isValid():
            raise RuntimeError(f"SVG 解析失败: {svg_path}")

    def render(self, size: int) -> Image.Image:
        """按 size x size 渲染，背景透明，开启抗锯齿。

        显式传入目标矩形（而非依赖 viewport 默认值），确保 SVG 的 viewBox 精确铺满画布。
        """
        image = QImage(size, size, QImage.Format_ARGB32)
        image.fill(QColor(0, 0, 0, 0))
        painter = QPainter(image)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, True)
        self._renderer.render(painter, QRectF(0, 0, size, size))
        painter.end()

        buffer = QBuffer()
        buffer.open(QBuffer.WriteOnly)
        image.save(buffer, "PNG", 0)
        result = Image.open(io.BytesIO(bytes(buffer.data()))).convert("RGBA")
        # PySide6 拿不到 render() 的返回值，用“是否画出了不透明像素”判断渲染是否真的发生
        if result.getchannel("A").getextrema()[1] == 0:
            raise RuntimeError(f"SVG 渲染结果为空画布: size={size}")
        return result


def downscale(master: Image.Image, size: int) -> Image.Image:
    """从高分辨率母版逐级折半缩放至目标尺寸。

    直接一次性大幅缩小会因 LANCZOS 支撑窗口不足产生锯齿/走样，逐级折半可保证每个
    尺寸都由“接近 2 倍”的下采样得到，边缘灰度覆盖准确、细节不丢失。
    """
    image = master
    while image.width >= size * 2:
        half = max(size, image.width // 2)
        image = image.resize((half, half), Image.LANCZOS)
    if image.width != size:
        image = image.resize((size, size), Image.LANCZOS)
    return image


def _png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, "PNG", optimize=True)
    return buffer.getvalue()


def _bmp_bytes(image: Image.Image) -> bytes:
    """编码为 ICO 内嵌的 32 位 BMP（BITMAPINFOHEADER + BGRA 自下而上 + 1bpp AND 掩码）。

    小尺寸一律用 BMP：部分 Shell / 老版本 Windows 对 PNG 压缩的 16/32 条目支持不完整。
    """
    width, height = image.size
    pixels = image.convert("RGBA").load()

    xor_data = bytearray()
    for y in range(height - 1, -1, -1):  # BMP 行序自下而上
        for x in range(width):
            r, g, b, a = pixels[x, y]
            xor_data += bytes((b, g, r, a))

    mask_stride = ((width + 31) // 32) * 4
    and_mask = bytearray(mask_stride * height)
    for y in range(height - 1, -1, -1):
        row = (height - 1 - y) * mask_stride
        for x in range(width):
            if pixels[x, y][3] == 0:  # 掩码位为 1 表示该像素透明
                and_mask[row + (x >> 3)] |= 0x80 >> (x & 7)

    header = struct.pack(
        "<IiiHHIIiiII",
        40,                 # biSize
        width,              # biWidth
        height * 2,         # biHeight：XOR + AND 两部分高度之和
        1,                  # biPlanes
        32,                 # biBitCount
        0,                  # biCompression：BI_RGB
        len(xor_data) + len(and_mask),
        0, 0, 0, 0,
    )
    return header + bytes(xor_data) + bytes(and_mask)


def write_ico(path: Path, images: list) -> None:
    """按 ICONDIR/ICONDIRENTRY 规范打包多分辨率 .ico（>=128 用 PNG，其余用 BMP）。"""
    payloads = []
    for image in images:
        size = image.width
        data = _png_bytes(image) if size >= 128 else _bmp_bytes(image)
        payloads.append((size, data))

    count = len(payloads)
    header_size = 6 + count * 16
    offset = header_size

    out = io.BytesIO()
    out.write(struct.pack("<HHH", 0, 1, count))  # 保留字、类型(1=图标)、条目数
    for size, data in payloads:
        # 256 在协议中用 0 表示；位深字段仍写 32 以兼容旧解析器
        field = 0 if size >= 256 else size
        out.write(struct.pack("<BBBBHHII", field, field, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    for _, data in payloads:
        out.write(data)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(out.getvalue())


def write_icns(path: Path, images: dict) -> None:
    """打包 macOS .icns：'icns' + 大端总长度 + 若干 (类型 + 长度 + PNG 数据) 条目。"""
    body = io.BytesIO()
    for tag, size in ICNS_TYPES:
        png = _png_bytes(images[size])
        body.write(tag + struct.pack(">I", 8 + len(png)) + png)
    data = body.getvalue()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"icns" + struct.pack(">I", 8 + len(data)) + data)


def save_png(path: Path, image: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, "PNG", optimize=True)


def generate(svg: Path, preview_dir: Path = None) -> list:
    """生成全部图标产物，返回写出的文件列表。"""
    rasterizer = SvgRasterizer(svg)
    master = rasterizer.render(2160)  # 2 倍源尺寸母版，所有目标尺寸由它下采样得到

    written = []

    # 1) Windows：exe 资源图标 + Qt 资源包内图标（同一套多分辨率数据）
    win_images = [downscale(master, s) for s in WIN_SIZES]
    for target in (ROOT / "icons" / "win" / "LANShare.ico", ROOT / "img" / "LANShare.ico"):
        write_ico(target, win_images)
        written.append(target)

    # 2) Qt 运行时：窗口/托盘用高清母图（QIcon 会自动按 DPI 下采样），聊天列表用小尺寸直出
    save_png(ROOT / "img" / "ic_launcher.png", downscale(master, 256))
    save_png(ROOT / "img" / "ic_launcher_32.png", downscale(master, 32))
    written += [ROOT / "img" / "ic_launcher.png", ROOT / "img" / "ic_launcher_32.png"]

    # 3) 网页端 favicon
    save_png(ROOT / "web" / "images" / "lanshare.png", downscale(master, 256))
    written.append(ROOT / "web" / "images" / "lanshare.png")

    # 4) macOS：iconset 全套 + .icns
    iconset_dir = ROOT / "icons" / "AppIcon.iconset"
    icns_sizes = {}
    for name, size in ICONSET:
        image = downscale(master, size)
        save_png(iconset_dir / name, image)
        icns_sizes[size] = image
        written.append(iconset_dir / name)
    write_icns(ROOT / "build" / "macos" / "AppIcon.icns", icns_sizes)
    written.append(ROOT / "build" / "macos" / "AppIcon.icns")

    # 预览目录：额外导出若干尺寸便于人工比对（不参与构建）
    if preview_dir:
        for size in (16, 32, 48, 256):
            save_png(preview_dir / f"preview_{size}.png", downscale(master, size))

    return written


def main() -> int:
    parser = argparse.ArgumentParser(description="从矢量 SVG 生成全平台多分辨率图标")
    parser.add_argument("--svg", type=Path, default=ROOT / "icons" / "lanshare.svg",
                        help="矢量母版路径（默认 icons/lanshare.svg）")
    parser.add_argument("--out", type=Path, default=None,
                        help="额外导出预览 PNG 的目录（可选）")
    args = parser.parse_args()

    svg = args.svg if args.svg.is_absolute() else ROOT / args.svg
    if not svg.exists():
        print(f"[错误] 找不到矢量母版: {svg}", file=sys.stderr)
        return 1

    files = generate(svg, args.out)
    print(f"母版: {svg.relative_to(ROOT)}")
    for f in files:
        print(f"  生成 {f.relative_to(ROOT)}  ({f.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
