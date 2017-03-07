#include "Banner.hpp"

#include <zconf.h>
#include <iostream>


Banner::Banner(DeviceInfo realInfo, DeviceInfo desiredInfo) {
    mSize = BANNER_SIZE;
    unsigned char quirks = 0;
    quirks |= QUIRK_DUMB;

    // Prepare banner for clients.
    mData = new unsigned char[mSize];
    mData[0] = (unsigned char) BANNER_VERSION;
    mData[1] = (unsigned char) BANNER_SIZE;
    putUInt32LE(mData + 2, getpid());
    putUInt32LE(mData + 6,  realInfo.width);
    putUInt32LE(mData + 10,  realInfo.height);
    putUInt32LE(mData + 14, desiredInfo.width);
    putUInt32LE(mData + 18, desiredInfo.height);
    mData[22] = (unsigned char) desiredInfo.orientation;
    mData[23] = quirks;

    std::cerr << "== Banner ==" << std::endl;
    std::cerr << "version: " << BANNER_VERSION << std::endl;
    std::cerr << "size: " << BANNER_SIZE << std::endl;
    std::cerr << "pid: " << getpid() << std::endl;
    std::cerr << "real width: " << realInfo.width << std::endl;
    std::cerr << "real height: " << realInfo.height << std::endl;
    std::cerr << "desired width: " << desiredInfo.width << std::endl;
    std::cerr << "desired height: " << desiredInfo.height << std::endl;
    std::cerr << "orientation: " << desiredInfo.orientation << std::endl;
    std::cerr << "quirks: " << (int) quirks << std::endl;
}


unsigned char* Banner::getData() {
    return mData;
}

size_t Banner::getSize() {
    return mSize;
}

Banner::~Banner() {
    delete [] mData;
}
