#ifndef CUSTOMDATASTREAM_H
#define CUSTOMDATASTREAM_H

#include "TCPClient.h"
#include <cstdint>
#include <string>
#include <vector>

class CustomDataStream {
private:
    TCPClient *client;

    // Helper methods
    void writeBytes(const void* data, size_t len) const;
    void readBytes(void* buffer, size_t len) const;

public:
    CustomDataStream(TCPClient* client);

    // Write methods
    void writeBoolean(bool value) const;
    void writeByte(int8_t value) const;
    void writeUnsignedByte(uint8_t value) const;
    void writeShort(int16_t value) const;
    void writeUnsignedShort(uint16_t value) const;
    void writeChar(char value) const;
    void writeInt(int value) const;
    void writeLong(mlong value) const;
    void writeFloat(float value) const;
    void writeDouble(double value) const;
    void writeString(const std::string& value) const;
    void write(const void* buffer, size_t len) const;

    // Read methods
    bool readBoolean() const;
    mbyte readByte() const;
    uint8_t readUnsignedByte() const;
    int16_t readShort() const;
    uint16_t readUnsignedShort() const;
    char readChar() const;
    int readInt() const;
    mlong readLong() const;
    float readFloat() const;
    double readDouble() const;
    std::string readString() const;

    int readFully(void* buffer, int len) const;
    int readFully(std::vector<uint8_t>& buffer, int len) const;

    int skipBytes(int n) const;

    void close() const;
};

#endif // CUSTOMDATASTREAM_H
