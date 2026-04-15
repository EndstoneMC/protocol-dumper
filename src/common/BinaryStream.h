#pragma once

class ReadOnlyBinaryStream {
public:
    explicit ReadOnlyBinaryStream(std::string &&buffer);
    explicit ReadOnlyBinaryStream(std::string_view buffer, bool copyBuffer);
    virtual ~ReadOnlyBinaryStream() = default;

protected:
    std::string mOwnedBuffer;
    std::string_view mView;

private:
    size_t mReadPointer;
    bool mHasOverflowed;
};

class BinaryStream : public ReadOnlyBinaryStream {
public:
    BinaryStream();

private:
    std::string &mBuffer;
};
