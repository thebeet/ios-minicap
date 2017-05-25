#include <iostream>
#include "FrameListener.hpp"

FrameListener::FrameListener() {
    mPendingFrames = 0;
    mTimeout = std::chrono::milliseconds(200);
    mRunning = true;
}

void FrameListener::stop() {
    mRunning = false;
}

bool FrameListener::isRunning() {
    return mRunning;
}

void FrameListener::onFrameAvailable() {
    std::unique_lock<std::mutex> lock(mMutex);
    mPendingFrames += 1;
    mCondition.notify_one();
}

int FrameListener::waitForFrame() {
    std::unique_lock<std::mutex> lock(mMutex);

    while (mRunning) {
        if (mCondition.wait_for(lock, mTimeout, [this]{return mPendingFrames > 0;})) {
            return mPendingFrames--;
        }
    }

    return 0;
}

int FrameListener::waitForFrameLimitTime(int time) {
    std::cerr << "Wait Frame" << std::endl;
    std::unique_lock<std::mutex> lock(mMutex);
    int limitTime = time;
    while (mRunning) {
        if (mCondition.wait_for(lock, mTimeout, [this]{return mPendingFrames > 0;})) {
            std::cerr << "Wait Frame done" << std::endl;
            return mPendingFrames--;
        }
        if (limitTime > 0) {
            limitTime--;
            if (limitTime == 0) {
                return -1;
            }
        }
        std::cerr << "Wait Frame none" << std::endl;
    }
    return 0;
}
