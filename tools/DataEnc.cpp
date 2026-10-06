//
// Created by fgsqme on 2021/5/10.
//

#include "DataEnc.h"
#include "ByteUtils.h"

/** 默认构造函数 */
DataEnc::DataEnc() = default;

/** 构造函数 */
DataEnc::DataEnc(mbyte *bytes, int bytelen) {
    setData(bytes, bytelen);
}

/** 获取包头大小 */
int DataEnc::headerSize() {
    return HEADER_LEN;
}

/** 设置数据缓冲区 */
void DataEnc::setData(mbyte *bytes, int bytelen) {
    if (bytes != nullptr) {
        m_bytes = bytes;
        m_byteLen = bytelen;
    }
}

/** 设置命令字 */
void DataEnc::setCmd(int cmd) {
    putInt(cmd, 0);
}

/** 设置字节型命令字 */
void DataEnc::setByteCmd(mbyte cmd) {
    putByte(cmd, 0);
}


/** 设置计数器 */
void DataEnc::setCount(int count) {
    putInt(count, 4);
}

/** 设置数据长度 */
void DataEnc::setLength(int length) {
    putInt(length, 8);
}


DataEnc &DataEnc::putInt(int val) {
    if ((index + 4) <= m_byteLen) {
        ByteUtils::intToBytes(val, m_bytes, index);
        index += 4;
    }
    return *this;
}

DataEnc &DataEnc::putLong(mlong val) {
    if ((index + 8) <= m_byteLen) {
        ByteUtils::longToBytes(val, m_bytes, index);
        index += 8;
    }
    return *this;
}

DataEnc &DataEnc::putByte(mbyte val) {
    if ((index + 1) <= m_byteLen) {
        m_bytes[index] = val;
        index += 1;
    }
    return *this;
}

DataEnc &DataEnc::putBool(bool val) {
    return putByte((mbyte) val);
}

DataEnc &DataEnc::putFloat(float val) {
    return putInt((int) (val * 1000));
}

DataEnc &DataEnc::putDouble(double val) {
    return putLong((mlong) (val * 1000000));
}

DataEnc &DataEnc::putStr(const char *str, int len) {
    if ((index + len + 4) <= m_byteLen) {
        putInt(len);
        ByteUtils::ByteArrCopy((mbyte *) str, 0, m_bytes, index, len);
        index += len;
    }
    return *this;
}

DataEnc &DataEnc::putStr(const char *str) {
    return putStr(str, (int) strlen(str));
}

DataEnc &DataEnc::putString(const std::string &str) {
    return putStr(str.c_str(), (int) str.size());
}

DataEnc &DataEnc::putString(const QString &str) {
    auto basic_string = str.toStdString();
    return putString(basic_string);
}


DataEnc &DataEnc::putInt(int val, int i) {
    if ((i + 4) <= m_byteLen) {
        ByteUtils::intToBytes(val, m_bytes, i);
    }
    return *this;
}

DataEnc &DataEnc::putLong(mlong val, int i) {
    if ((i + 8) <= m_byteLen) {
        ByteUtils::longToBytes(val, m_bytes, i);
    }
    return *this;
}

DataEnc &DataEnc::putByte(mbyte val, int i) {
    if ((i + 1) <= m_byteLen) {
        m_bytes[i] = val;
    }
    return *this;
}

DataEnc &DataEnc::putFloat(float val, int i) {
    putInt((int) (val * 1000), i);
    return *this;
}

DataEnc &DataEnc::putDouble(double val, int i) {
    putLong((mlong) (val * 1000000), i);
    return *this;
}

DataEnc &DataEnc::putStr(const char *str, int len, int i) {
    if ((i + len + 4) <= m_byteLen) {
        putInt(len, i);
        i += 4;
        ByteUtils::ByteArrCopy((mbyte *) str, 0, m_bytes, i, len);
    }
    return *this;
}

int DataEnc::getDataIndex() const {
    return index - HEADER_LEN;
}

int DataEnc::getDataLen() const {
    return index;
}

/** 重置写入位置 */
void DataEnc::reset() {
    index = HEADER_LEN;
}

/** 获取打包后的数据 */
mbyte *DataEnc::getData() {
    setLength(index - HEADER_LEN);
    return m_bytes;
}

/** 加密并返回数据 */
mbyte *DataEnc::encData() {
    if (dataEncrypted) {
        return m_bytes;
    }
    setLength(index - HEADER_LEN);
    for (int i = 0; i < index; i++) {
        m_bytes[i] = (mbyte) ((m_bytes[i] - 1) ^ 0x45);
    }
    dataEncrypted = true;
    return m_bytes;
}

void DataEnc::setDataIndex(int i) {
    index = i + HEADER_LEN;
}

mbyte *DataEnc::getBuffer() {
    return m_bytes;
}





