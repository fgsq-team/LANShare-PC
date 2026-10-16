//
// 聊天界面集中配色器。
//
// 过去气泡颜色、文字颜色、状态色在 initView()/changeEvent()/paintEvent() 里
// 各写一遍字符串，主题切换时容易漏改且难维护。这里把所有"聊天相关配色"收敛到
// 一组静态访问函数，唯一数据源仍是 Config（其值来自当前主题 QSS 解析）。
//
// 约定（与历史视觉保持一致）：
//   - 右侧气泡使用主题主色（蓝/绿），文字为白色，文件大小为淡灰 #B0B6BF；
//   - 左侧气泡使用表面色，文字/大小均为主题正文色。
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

/// 次要信息（文件大小）颜色：右侧固定淡灰，左侧正文色。
inline QColor subTextColor(ChatDirection dir) {
    return dir == ChatDirection::Right ? QColor("#B0B6BF") : Config::instance().textColor;
}

/// 状态文本（成功/失败）颜色。
inline QColor successColor() {
    return Config::instance().successColor;
}

inline QColor errorColor() {
    return Config::instance().errorColor;
}

/// 传输进行中的状态文字颜色（跟随正文色）。
inline QColor statusNeutralColor(ChatDirection dir) {
    return subTextColor(dir);
}

/// 进度条前景（已完成部分）。
inline QColor progressChunkColor() {
    return Config::instance().primaryColor;
}

/// 进度条背景（未完成部分轨道）。
inline QColor progressTrackColor() {
    return Config::instance().selectionBgColor;
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
