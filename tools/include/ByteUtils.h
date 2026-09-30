//
// Created by fgsqme on 2021/5/10.
//
#include "Type.h"

#ifndef WZ_CHEAT_BYTEUTIL_H
#define WZ_CHEAT_BYTEUTIL_H


class ByteUtils {
public:
    static void ByteArrCopy(const mbyte *d, int d_index, mbyte *t, int t_index, int length);

    static void intToBytes(int i, mbyte *b, int index = 0);

    static int bytesToInt(mbyte *buf, int offset);

    static void longToBytes(mlong i, mbyte *b, int index = 0);

    static mlong bytesToLong(mbyte *buf, int offset = 0);

    static short bytesToShort(const mbyte *buf, int offset = 0);

    static void shortToBytes(short i, mbyte *b);
};


#endif //WZ_CHEAT_BYTEUTIL_H
