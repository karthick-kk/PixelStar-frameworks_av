/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef A_TRAFFIC_RECORDER_H_

#define A_TRAFFIC_RECORDER_H_

#include <android-base/logging.h>
#include <utils/RefBase.h>

namespace android {

// Circular array to save recent amount of bytes
template <class Time, class Bytes>
class TrafficRecorder : public RefBase {
private:
    size_t mSize;
    size_t mSizeMask;
    Time *mTimeArray = NULL;
    Bytes *mBytesArray = NULL;
    size_t mHeadIdx = 0;
    size_t mTailIdx = 0;

    Time mClock = 0;
    Time mLastTimeOfPrint = 0;
    Bytes mAccuBytesOfPrint = 0;
public:
    TrafficRecorder();
    TrafficRecorder(size_t size);
    virtual ~TrafficRecorder();

    void init();

    void updateClock(Time now);

    Bytes readBytesForLastPeriod(Time period);
    void writeBytes(Bytes bytes);

    void printAccuBitsForLastPeriod(Time period, Time unit);
};

template <class Time, class Bytes>
TrafficRecorder<Time, Bytes>::TrafficRecorder() {
    TrafficRecorder(128);
}

template <class Time, class Bytes>
TrafficRecorder<Time, Bytes>::TrafficRecorder(size_t size) {
    size_t exp;
    for (exp = 0; exp < 32; exp++) {
        if (size <= (1ul << exp)) {
            break;
        }
    }
    mSize = (1ul << exp);         // size = 2^exp
    mSizeMask = mSize - 1;

    LOG(VERBOSE) << "TrafficRecorder Init size " << mSize;
    mTimeArray = new Time[mSize];
    mBytesArray = new Bytes[mSize];

    init();
}

template <class Time, class Bytes>
TrafficRecorder<Time, Bytes>::~TrafficRecorder() {
    delete[] mTimeArray;
    delete[] mBytesArray;
}

template <class Time, class Bytes>
void TrafficRecorder<Time, Bytes>::init() {
    mHeadIdx = 0;
    mTailIdx = 0;
    mTimeArray[0] = 0;
    mBytesArray[0] = 0;
}

template <class Time, class Bytes>
void TrafficRecorder<Time, Bytes>::updateClock(Time now) {
    mClock = now;
}

template <class Time, class Bytes>
Bytes TrafficRecorder<Time, Bytes>::readBytesForLastPeriod(Time period) {
    Bytes bytes = 0;

    size_t i = mTailIdx;
    while (i != mHeadIdx) {
        LOG(VERBOSE) << "READ " << i << " time " << mTimeArray[i] << " \t EndOfPeriod " << mClock - period;
        if (mTimeArray[i] < mClock - period) {
            break;
        }
        bytes += mBytesArray[i];
        i = (i + mSize - 1) & mSizeMask;
    }
    mHeadIdx = i;
    return bytes;
}

template <class Time, class Bytes>
void TrafficRecorder<Time, Bytes>::writeBytes(Bytes bytes) {
    size_t writeIdx;
    if (mClock == mTimeArray[mTailIdx]) {
        writeIdx = mTailIdx;
        mBytesArray[writeIdx] += bytes;
    } else {
        writeIdx = (mTailIdx + 1) % mSize;
        mTimeArray[writeIdx] = mClock;
        mBytesArray[writeIdx] = bytes;
    }

    LOG(VERBOSE) << "WRITE " << writeIdx << " time " << mClock;
    if (writeIdx == mHeadIdx) {
        LOG(WARNING) << "Traffic recorder size exceeded at " << mHeadIdx;
        mHeadIdx = (mHeadIdx + 1) & mSizeMask;
    }

    mTailIdx = writeIdx;
    mAccuBytesOfPrint += bytes;
}

template <class Time, class Bytes>
void TrafficRecorder<Time, Bytes>::printAccuBitsForLastPeriod(Time period, Time unit) {
    Time duration = mClock - mLastTimeOfPrint;
    float numOfUnit = (float)duration / unit;
    if (duration > period) {
        ALOGD("Actual Tx period %.0f ms \t %.0f Bits/Unit",
              numOfUnit * 1000.f, mAccuBytesOfPrint * 8.f / numOfUnit);
        mLastTimeOfPrint = mClock;
        mAccuBytesOfPrint = 0;
        init();
    }
}

}  // namespace android

#endif  // A_TRAFFIC_RECORDER_H_