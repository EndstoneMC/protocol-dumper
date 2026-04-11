#pragma once

class ReadOnlyBinaryStream {
public:
    virtual ~ReadOnlyBinaryStream() = default;

protected:
    std::string mOwnedBuffer;
    std::string_view mView;

private:
    size_t mReadPointer;
    bool mHasOverflowed;
};

class BinaryStream : public ReadOnlyBinaryStream {
private:
    std::string &mBuffer;
};
