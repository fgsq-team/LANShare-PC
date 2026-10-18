//
// 聊天界面集中配色器。
//
// 过去气泡颜色、文字颜色、状态色在 initView()/changeEvent()/paintEvent() 里
// 各写一遍字符串，主题切换时容易漏改且难维护。这里把所有"聊天相关配色"收敛到
// 一组静态访问函数，唯一数据源仍是 Config（其值来自当前主题 QSS 解析）。
//
// 约定（Material You 风格）：
//   - 右侧气泡使用主题主色（蓝/绿），文字为白色，文件大小为淡灰 #B0B6BF；
//   - 左侧气泡使用带色调的表面变体色（surfaceVariant，比列表背景深一档），
//     文件名用正文色、大小用淡化正文色（onSurfaceVariant 观感）；
//   - 文件图块统一白底 + 主色 glyph，两侧不再区分；
//   - 文件气泡的进度条/状态胶囊按方向取色：右侧白色进度 + 半透明黑底胶囊，
//     左侧绿色进度 + tone-on-tone 加深胶囊，徽标圆点用成功/失败色。
//   - QTextBrowser 正文颜色不在这里设置，由 QSS 的 #chatTextLeft/#chatTextRight 控制。
//
#ifndef LANSHARE_CHATTHEME_H
#define LANSHARE_CHATTHEME_H

#include <QColor>
#include <QBrush>
#include "Config.hpp"
#include "ChatMessage.h"

namespace ChatTheme {

/// 气泡填充色。
inline QColor bubbleColor(ChatDirection dir) {
    return dir == ChatDirection::Left
               ? Config::instance().chatBubblColorLeft.color()
               : Config::instance().chatBubblColorRight.color();
}

/// 气泡描边色：与填充色同色系，左侧略深、右侧与填充一致（贴近原 QSS 观感）。
inline QColor bubbleBorderColor(ChatDirection dir) {
    if (dir == ChatDirection::Left) {
        return bubbleColor(dir).darker(110);
    }
    return bubbleColor(dir);
}

/// 昵称颜色：弱化正文色（MD3 onSurfaceVariant 观感），rgba 输出供样式表使用。
inline QColor nicknameColor(ChatDirection dir) {
    (void) dir;
    QColor c = Config::instance().textColor;
    c.setAlpha(170);
    return c;
}

/// 颜色转 CSS 字符串（支持透明度）。
inline QString cssColor(const QColor &c) {
    if (c.alpha() != 255) {
        return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    }
    return c.name();
}

/// 气泡内非正文标签（文件名）颜色。
inline QColor titleColor(ChatDirection dir) {
    return dir == ChatDirection::Right ? QColor("#FFFFFF") : Config::instance().textColor;
}

/// 次要信息（文件大小）颜色：右侧固定淡灰，左侧正文色淡化（MD3 onSurfaceVariant 观感）。
inline QColor subTextColor(ChatDirection dir) {
    if (dir == ChatDirection::Right) {
        return QColor("#B0B6BF");
    }
    QColor c = Config::instance().textColor;
    c.setAlpha(150);
    return c;
}

/// 状态文本（成功/失败）颜色。
inline QColor successColor() {
    return Config::instance().successColor;
}

inline QColor errorColor() {
    return Config::instance().errorColor;
}

/// 进度条前景（已完成部分）：右侧白色压在主题主色上，左侧用成功绿。
inline QColor progressChunkColor(ChatDirection dir) {
    return dir == ChatDirection::Right ? QColor("#FFFFFF") : Config::instance().successColor;
}

/// 进度条背景轨道：右侧半透明黑（叠在主色上），左侧浅灰半透明黑。
inline QColor progressTrackColor(ChatDirection dir) {
    return dir == ChatDirection::Right ? QColor(0, 0, 0, 60) : QColor(0, 0, 0, 35);
}

/// 状态胶囊背景：右侧半透明黑，左侧在气泡底色上加深一档（tone-on-tone）。
inline QColor chipBgColor(ChatDirection dir) {
    if (dir == ChatDirection::Right) {
        return QColor(0, 0, 0, 55);
    }
    return bubbleColor(dir).darker(115);
}

/// 气泡主体圆角半径（MD3 大圆角）。
inline int bubbleRadius() {
    return 16;
}

/// 靠近头像一侧的"尾巴"小圆角。
inline int bubbleTailRadius() {
    return 6;
}

}  // namespace ChatTheme

#endif //LANSHARE_CHATTHEME_H
