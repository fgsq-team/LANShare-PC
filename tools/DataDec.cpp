//
// Created by fgsqme on 2021/5/10.
//

#include "DataDec.h"
#include "ByteUtils.h"


DataDec::DataDec() = default;

DataDec::DataDec(mbyte *bytes, int bytelen) {
    setData(bytes, bytelen);
}

void DataDec::setData(mbyte *bytes, int bytelen) {
    reset();
    m_byteLen = bytelen;
    m_bytes = bytes;
}

int DataDec::getInt() {
    int val = getInt(index);
    index += 4;
    return val;
}

mlong DataDec::getLong() {
    mlong val = getLong(index);
    index += 8;
    return val;
}

mbyte DataDec::getByte() {
    return getByte(index++);
}

bool DataDec::getBool() {
    return getByte();
}


char *DataDec::getStr() {
    int len = getInt();
    if (len > 0 && (index + len) <= m_byteLen) {
        char *str = new char[len + 1];
        memset(str, 0, len);
        memcpy(str, m_bytes + index, len);
        str[len] = '\0';
        index += len;
        return str;
    }
    return nullptr;
}

std::string DataDec::getString() {
    int len = getInt();
    if (len > 0 && (index + len) <= m_byteLen) {
        char str[len + 1];
        memset(str, 0, len);
        memcpy(str, m_bytes + index, len);
        str[len] = '\0';
        index += len;
        return str;
    }
    return "";
}


void DataDec::getStr(char *buff) {
    int len = getInt();
    if (len > 0 && (index + len) <= m_byteLen) {
        memset(buff, 0, len);
        memcpy(buff, m_bytes + index, len);
        buff[len] = '\0';
        index += len;
    }
}

float DataDec::getFloat() {
    float val = getFloat(index);
    index += 4;
    return val;
}

double DataDec::getDouble() {
    double val = getDouble(index);
    index += 8;
    return val;
}

int DataDec::getCmd() {
    return getInt(0);
}

mbyte DataDec::getByteCmd() {
    return getByte(0);
}

int DataDec::getCount() {
    return getInt(4);
}

int DataDec::getLength() {
    return getInt(8);
}

void DataDec::skip(int off) {
    index = HEADER_LEN + off;
}

void DataDec::reset() {
    index = HEADER_LEN;
}

int DataDec::headerSize() {
    return HEADER_LEN;
}

int DataDec::getInt(int i) {
    if ((i + 4) <= m_byteLen) {
        int val = ByteUtils::bytesToInt(m_bytes, i);
        return val;
    }
    return 0;
}

mlong DataDec::getLong(int i) {
    if ((i + 8) <= m_byteLen) {
        mlong lg = ByteUtils::bytesToLong(m_bytes, i);
        return lg;
    }
    return 0;
}

mbyte DataDec::getByte(int i) {
    if ((i + 1) <= m_byteLen) {
        mbyte b = m_bytes[i];
        return b;
    }
    return 0;
}

char *DataDec::getStr(int i) {
    int len = getInt(i);
    i += 4;
    if (len > 0 && (i + len) <= m_byteLen) {
        char *str = new char[len];
        memcpy(str, m_bytes + i, len);
        str[len] = '\0';
        return str;
    }
    return nullptr;
}

float DataDec::getFloat(int i) {
    return (float) getInt(i) / 1000;
}

double DataDec::getDouble(int i) {
    return (double) getLong(i) / 1000000;
}

mbyte *DataDec::getSurplusBytes() {
    if (m_byteLen - index > 0) {
        int surplus = m_byteLen - index;
        auto *temp = new mbyte[surplus];
        memcpy(temp, m_bytes + index, surplus);
        index = m_byteLen;
        return temp;
    }
    return nullptr;
}

void DataDec::getSurplusBytes(mbyte *buff) {
    if (m_byteLen - index > 0) {
        int surplus = m_byteLen - index;
        memcpy(buff, m_bytes + index, surplus);
        index = m_byteLen;
    }
}

void DataDec::decHeader() {
    for (int i = 0; i < HEADER_LEN; i++) {
        m_bytes[i] = (mbyte) ((m_bytes[i] ^ 0x45) + 1);
    }
}

void DataDec::decData() {
    int length = getLength();
    for (int i = HEADER_LEN; i < length + HEADER_LEN; i++) {
        m_bytes[i] = (mbyte) ((m_bytes[i] ^ 0x45) + 1);
    }
}

void DataDec::decAllData() {
    decHeader();
    decData();
}

int DataDec::getDataIndex() const {
    return index - HEADER_LEN;
}

void DataDec::setDataIndex(int i) {
    index = i + HEADER_LEN;
}

mbyte *DataDec::getBuffer() {
    return m_bytes;
}

mlong DataDec::getLongDefualt(int def) {
    if ((index + 8) <= m_byteLen) {
        mlong val = ByteUtils::bytesToLong(m_bytes, index);
        index += 8;
        return val;
    }
    return def;
}





