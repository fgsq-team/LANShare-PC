//
// 二维码(QR Code)生成器
//
// @author fgsq
// @version 1.0
#ifndef LANSHARE_QRCODE_HPP
#define LANSHARE_QRCODE_HPP

#include <QImage>
#include <QString>

namespace lanqrcode {
    /**
     * 生成二维码位图。
     * @param text   待编码内容(按 UTF-8 字节编码)
     * @param scale  每个模块(像素点)的放大倍数
     * @param border 四周留白模块数(静区，标准要求 >= 4)
     * @return 黑白 QImage(Format_Grayscale8)；内容为空或超出版本 10-M 容量时返回空图
     */
    QImage makeImage(const QString &text, int scale = 6, int border = 4);
} // namespace lanqrcode

#endif //LANSHARE_QRCODE_HPP
