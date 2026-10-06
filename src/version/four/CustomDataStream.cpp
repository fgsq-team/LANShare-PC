//
// Created by 16508 on 2025/8/30.
//

#include "CustomDataStream.h"
#include <stdexcept>
#include <cstring>

CustomDataStream::CustomDataStream(TCPClient *client) : client(client) {
}

// Write helper method
void CustomDataStream::writeBytes(const void *data, size_t len) const {
    int result = client->send(data, static_cast<int>(len));
    if (result <= 0) {
        throw std::runtime_error("Failed to write bytes");
    }
}

// Read helper method
void CustomDataStream::readBytes(void *buffer, size_t len) const {
    if (client->recvo(buffer, len) <= 0) {
        throw std::runtime_error("Failed to read bytes");
    }
}

// Write methods
void CustomDataStream::writeBoolean(bool value) const {
    writeByte(value ? 1 : 0);
}

void CustomDataStream::writeByte(int8_t value) const {
    writeBytes(&value, sizeof(value));
}

void CustomDataStream::writeUnsignedByte(uint8_t value) const {
    writeBytes(&value, sizeof(value));
}

void CustomDataStream::writeShort(int16_t value) const {
    uint8_t bytes[2];
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
    writeBytes(bytes, sizeof(bytes));
}

void CustomDataStream::writeUnsignedShort(uint16_t value) const {
    uint8_t bytes[2];
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
    writeBytes(bytes, sizeof(bytes));
}

void CustomDataStream::writeChar(char value) const {
    uint8_t bytes[2];
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
    writeBytes(bytes, sizeof(bytes));
}

void CustomDataStream::writeInt(int value) const {
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    writeBytes(bytes, sizeof(bytes));
}

void CustomDataStream::writeLong(mlong value) const {
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (value >> (56 - i * 8)) & 0xFF;
    }
    writeBytes(bytes, sizeof(bytes));
}

void CustomDataStream::writeFloat(float value) const {
    int intValue;
    std::memcpy(&intValue, &value, sizeof(float));
    writeInt(intValue);
}

void CustomDataStream::writeDouble(double value) const {
    mlong longValue;
    std::memcpy(&longValue, &value, sizeof(double));
    writeLong(longValue);
}

void CustomDataStream::writeString(const std::string &value) const {
    int length = static_cast<int>(value.length());
    writeInt(length);
    if (length > 0) {
        writeBytes(value.c_str(), length);
    }
}

void CustomDataStream::write(const void *buffer, size_t len) const {
    writeBytes(buffer, len);
}

// Read methods
bool CustomDataStream::readBoolean() const {
    return readByte() != 0;
}

mbyte CustomDataStream::readByte() const {
    int8_t value;
    readBytes(&value, sizeof(value));
    return value;
}

uint8_t CustomDataStream::readUnsignedByte() const {
    uint8_t value;
    readBytes(&value, sizeof(value));
    return value;
}

int16_t CustomDataStream::readShort() const {
    uint8_t bytes[2];
    readBytes(bytes, sizeof(bytes));
    return static_cast<int16_t>((bytes[0] << 8) | bytes[1]);
}

uint16_t CustomDataStream::readUnsignedShort() const {
    uint8_t bytes[2];
    readBytes(bytes, sizeof(bytes));
    return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

char CustomDataStream::readChar() const {
    uint8_t bytes[2];
    readBytes(bytes, sizeof(bytes));
    return static_cast<char>((bytes[0] << 8) | bytes[1]);
}

int CustomDataStream::readInt() const {
    uint8_t bytes[4];
    readBytes(bytes, sizeof(bytes));
    return (static_cast<int>(bytes[0]) << 24) |
           (static_cast<int>(bytes[1]) << 16) |
           (static_cast<int>(bytes[2]) << 8) |
           (static_cast<int>(bytes[3]));
}

mlong CustomDataStream::readLong() const {
    uint8_t bytes[8];
    readBytes(bytes, sizeof(bytes));

    mlong value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

float CustomDataStream::readFloat() const {
    int intValue = readInt();
    float floatValue;
    std::memcpy(&floatValue, &intValue, sizeof(float));
    return floatValue;
}

double CustomDataStream::readDouble() const {
    mlong longValue = readLong();
    double doubleValue;
    std::memcpy(&doubleValue, &longValue, sizeof(double));
    return doubleValue;
}

std::string CustomDataStream::readString() const {
    int length = readInt();
    if (length == -1) {
        return ""; // 或者返回一个特殊标记表示null字符串
    }
    if (length == 0) {
        return "";
    }

    std::vector<char> buffer(length);
    readFully(buffer.data(), length);
    return std::string(buffer.data(), length);
}

int CustomDataStream::readFully(void *buffer, int len) const {
    readBytes(buffer, len);
    return len;
}

int CustomDataStream::readFully(std::vector<uint8_t> &buffer, int len) const {
    buffer.resize(len);
    readFully(buffer.data(), len);
    return len;
}

int CustomDataStream::skipBytes(int n) const {
    return static_cast<int>(client->skip(n));
}

void CustomDataStream::close() const {
    client->close();
}
