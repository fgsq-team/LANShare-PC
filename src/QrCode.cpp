//
// 二维码(QR Code)编码器实现
//
#include "QrCode.hpp"

#include <QByteArray>
#include <cstdint>
#include <vector>
#include <climits>
#include <algorithm>
#include <cstdlib>

namespace {

// ======================= 版本/纠错常量（仅等级 M） =======================

// 每块的纠错码字数（按版本索引，index 0 非法）
constexpr int8_t ECC_PER_BLOCK_M[11] = {
        -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26
};
// 每版本的纠错分块数（按版本索引，index 0 非法）
constexpr int8_t NUM_BLOCKS_M[11] = {
        -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5
};
// 每版本总码字数（数据 + 纠错），可由 rawDataModules(ver)/8 推得
constexpr int16_t TOTAL_CODEWORDS[11] = {
        -1, 26, 44, 70, 100, 134, 172, 196, 242, 292, 346
};

/** 某版本(等级 M)的数据码字数 */
inline int dataCodewords(int ver) {
    return TOTAL_CODEWORDS[ver] - ECC_PER_BLOCK_M[ver] * NUM_BLOCKS_M[ver];
}

/** 字节模式下字符计数指示器的位宽：版本 1~9 为 8 位，10~26 为 16 位 */
inline int charCountBits(int ver) {
    return ver <= 9 ? 8 : 16;
}

/** 取整数第 i 位 */
inline bool getBit(int val, int i) {
    return ((val >> i) & 1) != 0;
}

// ======================= GF(256) 纠错运算 =======================

/** GF(256) 乘法，本原多项式 0x11D */
uint8_t gfMul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        if ((b & 1) != 0) r ^= a;
        bool hi = (a & 0x80) != 0;
        a = static_cast<uint8_t>(a << 1);
        if (hi) a ^= 0x1D;
        b = static_cast<uint8_t>(b >> 1);
    }
    return r;
}

/** 生成 degree 次 Reed-Solomon 生成多项式系数 */
std::vector<uint8_t> rsGenerator(int degree) {
    std::vector<uint8_t> coef(degree, 0);
    coef[degree - 1] = 1;               // 初始多项式 = 1
    int root = 1;                        // 连乘 (x - α^0)(x - α^1)...
    for (int i = 0; i < degree; i++) {
        for (size_t j = 0; j < coef.size(); j++) {
            coef[j] = gfMul(coef[j], static_cast<uint8_t>(root));
            if (j + 1 < coef.size()) coef[j] ^= coef[j + 1];
        }
        root = gfMul(static_cast<uint8_t>(root), 0x02);
    }
    return coef;
}

/** 计算数据段 data[0..len) 相对生成多项式 gen 的纠错余式 */
std::vector<uint8_t> rsRemainder(const std::vector<uint8_t> &data, const std::vector<uint8_t> &gen) {
    std::vector<uint8_t> rem(gen.size(), 0);
    for (uint8_t b : data) {
        uint8_t factor = b ^ rem.front();
        rem.erase(rem.begin());
        rem.push_back(0);
        for (size_t j = 0; j < gen.size(); j++) {
            rem[j] ^= gfMul(gen[j], factor);
        }
    }
    return rem;
}

// ======================= 比特流缓冲 =======================

struct BitBuffer {
    std::vector<bool> bits;

    void append(int val, int len) {
        for (int i = len - 1; i >= 0; i--) {
            bits.push_back(((val >> i) & 1) != 0);
        }
    }
};

// ======================= 二维码编码核心 =======================

/**
 * 生成二维码模块矩阵。
 * @return 成功返回 size，并把 size×size 的黑白模块写入 modules；失败返回 0
 */
int encodeMatrix(const std::vector<uint8_t> &data, std::vector<std::vector<uint8_t>> &modules) {
    const int len = static_cast<int>(data.size());
    int ver = 0;
    for (int v = 1; v <= 10; v++) {
        int neededBits = 4 + charCountBits(v) + 8 * len; // 模式(4) + 计数 + 数据
        if (neededBits <= dataCodewords(v) * 8) {
            ver = v;
            break;
        }
    }
    if (ver == 0) return 0; // 超出支持容量

    const int size = ver * 4 + 17;

    // ---- 1. 编码数据比特流 ----
    BitBuffer bb;
    bb.append(0x4, 4);          // 字节模式指示符
    bb.append(len, charCountBits(ver));
    for (uint8_t b : data) bb.append(b, 8);

    const int capacityBits = dataCodewords(ver) * 8;
    // 终止符（不超过剩余容量且最多 4 个 0 位）
    bb.append(0, std::min(4, capacityBits - static_cast<int>(bb.bits.size())));
    // 补齐到字节边界
    while (static_cast<int>(bb.bits.size()) % 8 != 0) bb.bits.push_back(false);
    // 用 0xEC / 0x11 交替填充至满
    for (int pad = 0xEC; static_cast<int>(bb.bits.size()) < capacityBits; pad ^= 0xEC ^ 0x11) {
        bb.append(pad, 8);
    }

    // ---- 2. 分块并计算纠错，交叉合并 ----
    const int numBlocks = NUM_BLOCKS_M[ver];
    const int eccLen = ECC_PER_BLOCK_M[ver];
    const int rawCw = TOTAL_CODEWORDS[ver];
    const int dcw = dataCodewords(ver);
    const int numShort = numBlocks - dcw % numBlocks;   // 短数据块数量
    const int shortLen = dcw / numBlocks;               // 短块数据长度

    std::vector<std::vector<uint8_t>> dataBlocks;
    std::vector<std::vector<uint8_t>> eccBlocks;
    std::vector<uint8_t> gen = rsGenerator(eccLen);
    size_t bitPos = 0;
    for (int blk = 0; blk < numBlocks; blk++) {
        int blkDataLen = shortLen + (blk < numShort ? 0 : 1);
        std::vector<uint8_t> blockData;
        for (int i = 0; i < blkDataLen; i++) {
            uint8_t byte = 0;
            for (int k = 0; k < 8; k++) {
                byte = static_cast<uint8_t>((byte << 1) | (bb.bits[bitPos++] ? 1 : 0));
            }
            blockData.push_back(byte);
        }
        dataBlocks.push_back(blockData);
        eccBlocks.push_back(rsRemainder(blockData, gen));
    }

    // 交叉合并：数据按块轮询，然后纠错按块轮询
    std::vector<uint8_t> allCodewords;
    allCodewords.reserve(rawCw);
    for (int i = 0; i < shortLen + 1; i++) {
        for (int blk = 0; blk < numBlocks; blk++) {
            if (i < static_cast<int>(dataBlocks[blk].size())) {
                allCodewords.push_back(dataBlocks[blk][i]);
            }
        }
    }
    for (int i = 0; i < eccLen; i++) {
        for (int blk = 0; blk < numBlocks; blk++) {
            allCodewords.push_back(eccBlocks[blk][i]);
        }
    }

    // ---- 3. 初始化模块矩阵 + 功能图形 ----
    modules.assign(size, std::vector<uint8_t>(size, 0));
    std::vector<std::vector<uint8_t>> isFunction(size, std::vector<uint8_t>(size, 0));

    auto setFunc = [&](int x, int y, bool dark) {
        if (x >= 0 && x < size && y >= 0 && y < size) {
            modules[y][x] = dark ? 1 : 0;
            isFunction[y][x] = 1;
        }
    };

    // 定位图形 + 分隔符
    auto drawFinder = [&](int cx, int cy) {
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                int dist = std::max(std::abs(dx), std::abs(dy));
                setFunc(cx + dx, cy + dy, dist != 2 && dist != 4);
            }
        }
    };

    // 校正图形
    auto drawAlign = [&](int cx, int cy) {
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                setFunc(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
            }
        }
    };

    // 格式信息（15 位：等级 M=00 + 掩码 3 位，经 BCH 与 0x5412 异或）
    auto drawFormatBits = [&](int mask) {
        int dataBits = 0x00 << 3 | mask;      // 等级 M 格式位固定 00
        int rem = dataBits;
        for (int i = 0; i < 10; i++) {
            rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        }
        int bits = ((dataBits << 10) | rem) ^ 0x5412;
        // 第一份拷贝
        for (int i = 0; i <= 5; i++) setFunc(8, i, getBit(bits, i));
        setFunc(8, 7, getBit(bits, 6));
        setFunc(8, 8, getBit(bits, 7));
        setFunc(7, 8, getBit(bits, 8));
        for (int i = 9; i < 15; i++) setFunc(14 - i, 8, getBit(bits, i));
        // 第二份拷贝
        for (int i = 0; i < 8; i++) setFunc(size - 1 - i, 8, getBit(bits, i));
        for (int i = 8; i < 15; i++) setFunc(8, size - 15 + i, getBit(bits, i));
        setFunc(8, size - 8, true);            // 固定黑点
    };

    // 版本信息（版本 >= 7）
    auto drawVersionBits = [&]() {
        int rem = ver;
        for (int i = 0; i < 12; i++) {
            rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        }
        int bits = ver << 12 | rem;
        for (int i = 0; i < 18; i++) {
            bool bit = getBit(bits, i);
            int a = size - 11 + i % 3;
            int b = i / 3;
            setFunc(a, b, bit);
            setFunc(b, a, bit);
        }
    };

    // 时序图形
    for (int i = 0; i < size; i++) {
        setFunc(6, i, i % 2 == 0);
        setFunc(i, 6, i % 2 == 0);
    }
    // 三个定位图形
    drawFinder(3, 3);
    drawFinder(size - 4, 3);
    drawFinder(3, size - 4);

    // 校正图形位置
    std::vector<int> alignPos;
    if (ver != 1) {
        int numAlign = ver / 7 + 2;
        int step = (ver == 32) ? 26 : (ver * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;
        for (int pos = size - 7; static_cast<int>(alignPos.size()) < numAlign; pos -= step) {
            alignPos.insert(alignPos.begin(), pos);
        }
        alignPos[0] = 6;
    }
    for (size_t i = 0; i < alignPos.size(); i++) {
        for (size_t j = 0; j < alignPos.size(); j++) {
            // 跳过与三个定位图形重叠的角落
            bool corner = (i == 0 && j == 0) || (i == 0 && j == alignPos.size() - 1) ||
                          (i == alignPos.size() - 1 && j == 0);
            if (corner) continue;
            drawAlign(alignPos[i], alignPos[j]);
        }
    }

    // 预留格式与版本区域
    drawFormatBits(0);
    if (ver >= 7) drawVersionBits();

    // ---- 4. 画数据码字（蛇形排列）----
    auto drawCodewords = [&]() {
        size_t i = 0;
        for (int right = size - 1; right >= 1; right -= 2) {
            if (right == 6) right = 5;
            for (int vert = 0; vert < size; vert++) {
                for (int j = 0; j < 2; j++) {
                    int x = right - j;
                    bool upward = ((right + 1) & 2) == 0;
                    int y = upward ? size - 1 - vert : vert;
                    if (!isFunction[y][x] && i < allCodewords.size() * 8) {
                        modules[y][x] = getBit(allCodewords[i >> 3], 7 - static_cast<int>(i & 7)) ? 1 : 0;
                        i++;
                    }
                }
            }
        }
    };
    drawCodewords();

    // ---- 5. 掩码 ----
    auto applyMask = [&](int mask) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (isFunction[y][x]) continue;
                bool invert = false;
                switch (mask) {
                    case 0: invert = (x + y) % 2 == 0; break;
                    case 1: invert = y % 2 == 0; break;
                    case 2: invert = x % 3 == 0; break;
                    case 3: invert = (x + y) % 3 == 0; break;
                    case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                    case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                    case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                    case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                    default: break;
                }
                if (invert) modules[y][x] ^= 1;
            }
        }
    };

    // 惩罚分（4 条规则，参照规范/Nayuki）
    auto penaltyScore = [&]() -> int {
        int result = 0;
        // 规则 1：行/列同色连续 >=5
        for (int y = 0; y < size; y++) {
            int runColor = -1, runLen = 0;
            for (int x = 0; x < size; x++) {
                if (static_cast<int>(modules[y][x]) == runColor) {
                    runLen++;
                    if (runLen == 5) result += 3;
                    else if (runLen > 5) result += 1;
                } else {
                    runColor = modules[y][x];
                    runLen = 1;
                }
            }
        }
        for (int x = 0; x < size; x++) {
            int runColor = -1, runLen = 0;
            for (int y = 0; y < size; y++) {
                if (static_cast<int>(modules[y][x]) == runColor) {
                    runLen++;
                    if (runLen == 5) result += 3;
                    else if (runLen > 5) result += 1;
                } else {
                    runColor = modules[y][x];
                    runLen = 1;
                }
            }
        }
        // 规则 2：2x2 同色块
        for (int y = 0; y < size - 1; y++) {
            for (int x = 0; x < size - 1; x++) {
                uint8_t c = modules[y][x];
                if (c == modules[y][x + 1] && c == modules[y + 1][x] && c == modules[y + 1][x + 1]) {
                    result += 3;
                }
            }
        }
        // 规则 3：类似定位图形 1:1:3:1:1 + 4 空白
        const int pattern[] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
        const int size4[] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
        for (int y = 0; y < size; y++) {
            for (int x = 0; x + 11 <= size; x++) {
                bool m1 = true, m2 = true;
                for (int k = 0; k < 11; k++) {
                    int v = modules[y][x + k];
                    if (v != pattern[k]) m1 = false;
                    if (v != size4[k]) m2 = false;
                }
                if (m1) result += 40;
                if (m2) result += 40;
            }
        }
        for (int x = 0; x < size; x++) {
            for (int y = 0; y + 11 <= size; y++) {
                bool m1 = true, m2 = true;
                for (int k = 0; k < 11; k++) {
                    int v = modules[y + k][x];
                    if (v != pattern[k]) m1 = false;
                    if (v != size4[k]) m2 = false;
                }
                if (m1) result += 40;
                if (m2) result += 40;
            }
        }
        // 规则 4：黑点比例偏离 50%
        int dark = 0;
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (modules[y][x]) dark++;
            }
        }
        int total = size * size;
        int k = (std::abs(dark * 20 - total * 10) + total - 1) / total - 1;
        result += k * 10;
        return result;
    };

    int bestMask = 0, minPenalty = INT32_MAX;
    for (int mask = 0; mask < 8; mask++) {
        applyMask(mask);
        drawFormatBits(mask);
        int p = penaltyScore();
        if (p < minPenalty) {
            minPenalty = p;
            bestMask = mask;
        }
        applyMask(mask); // 撤销（XOR 自反）
    }
    applyMask(bestMask);
    drawFormatBits(bestMask);

    return size;
}

} // namespace

namespace lanqrcode {

QImage makeImage(const QString &text, int scale, int border) {
    QImage fail;
    if (scale <= 0) return fail;
    QByteArray utf8 = text.toUtf8();
    std::vector<uint8_t> data(utf8.begin(), utf8.end());
    if (data.empty()) return fail;

    std::vector<std::vector<uint8_t>> modules;
    int size = encodeMatrix(data, modules);
    if (size <= 0) return fail;

    const int dim = (size + border * 2) * scale;
    QImage img(dim, dim, QImage::Format_Grayscale8);
    if (img.isNull()) return img;
    img.fill(255); // 背景白
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (modules[y][x] == 0) continue;
            int px = (x + border) * scale;
            int py = (y + border) * scale;
            for (int dy = 0; dy < scale; dy++) {
                uchar *line = img.scanLine(py + dy);
                for (int dx = 0; dx < scale; dx++) {
                    line[px + dx] = 0; // 前景黑
                }
            }
        }
    }
    return img;
}

} // namespace lanqrcode
