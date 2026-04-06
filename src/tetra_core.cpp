#include "tetra_core.hpp"
#include <algorithm>
#include <cmath>

namespace tetra {
    ReceivedData::ReceivedData() {
        clear();
    }

    void ReceivedData::clear() {
        data.fill(-1);
    }

    bool ReceivedData::tryGetValue(GlobalNames name, int& valueOut) const {
        const int current = data[toIndex(name)];
        if (current != -1) {
            valueOut = current;
            return true;
        }
        return false;
    }

    bool ReceivedData::contains(GlobalNames name) const {
        return data[toIndex(name)] != -1;
    }

    int ReceivedData::value(GlobalNames name) const {
        return data[toIndex(name)];
    }

    void ReceivedData::setValue(GlobalNames name, int newValue) {
        data[toIndex(name)] = newValue;
    }

    void ReceivedData::add(GlobalNames name, int newValue) {
        data[toIndex(name)] = newValue;
    }

    namespace utils {
        std::uint8_t bitsToByte(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::uint8_t result = 0;
            for (int i = 0; i < length; ++i) {
                result <<= 1;
                result |= static_cast<std::uint8_t>(bitsBuffer[offset + i] & 1U);
            }
            return result;
        }

        std::uint8_t bitsToChar(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::uint8_t result = 0;
            for (int i = 0; i < length; ++i) {
                result >>= 1;
                result |= static_cast<std::uint8_t>((bitsBuffer[offset + i] & 1U) << 7);
            }
            return result;
        }

        std::uint32_t bitsToUInt32(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::uint32_t result = 0;
            for (int i = 0; i < length; ++i) {
                result <<= 1;
                result |= static_cast<std::uint32_t>(bitsBuffer[offset + i] & 1U);
            }
            return result;
        }

        int bitsToInt32(const std::uint8_t* bitsBuffer, int offset, int length) {
            int result = 0;
            for (int i = 0; i < length; ++i) {
                result <<= 1;
                result |= static_cast<int>(bitsBuffer[offset + i] & 1U);
            }
            return result;
        }

        std::uint64_t bitsToULong(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::uint64_t result = 0;
            for (int i = 0; i < length; ++i) {
                result <<= 1;
                result |= static_cast<std::uint64_t>(bitsBuffer[offset + i] & 1U);
            }
            return result;
        }

        std::int64_t bitsToLong(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::int64_t result = 0;
            for (int i = 0; i < length; ++i) {
                result <<= 1;
                result |= static_cast<std::int64_t>(bitsBuffer[offset + i] & 1U);
            }
            return result;
        }

        std::string bitsToString(const std::uint8_t* bitsBuffer, int offset, int length) {
            std::string result;
            result.reserve(static_cast<std::size_t>(length));
            for (int i = 0; i < length; ++i) {
                result.push_back(bitsBuffer[offset + i] == 0 ? '0' : '1');
            }
            return result;
        }

        std::string bitsToString(const std::int8_t* bitsBuffer, int offset, int length) {
            std::string result;
            result.reserve(static_cast<std::size_t>(length));
            for (int i = 0; i < length; ++i) {
                const auto value = bitsBuffer[offset + i];
                result.push_back(value == 0 ? '-' : (value == -1 ? '0' : '1'));
            }
            return result;
        }

        void byteToBits(std::uint8_t data, std::uint8_t* bitsBuffer, int bufferOffset) {
            for (int i = 0; i < 8; ++i) {
                bitsBuffer[bufferOffset + i] = (data & 0x80U) == 0 ? 0 : 1;
                data <<= 1;
            }
        }

        void intToBits(int data, std::uint8_t* bitsBuffer, int bufferOffset) {
            for (int i = 0; i < 32; ++i) {
                bitsBuffer[bufferOffset + i] = (data & 0x80000000) == 0 ? 0 : 1;
                data <<= 1;
            }
        }

        void uintToBits(std::uint32_t data, std::uint8_t* bitsBuffer, int firstBitOffset) {
            const int length = 32 - firstBitOffset;
            const std::uint32_t msb = 0x80000000U >> firstBitOffset;
            for (int i = 0; i < length; ++i) {
                bitsBuffer[i] = (data & msb) == 0 ? 0 : 1;
                data <<= 1;
            }
        }

        std::uint32_t createScramblerCode(int mcc, int mnc, int colour) {
            mcc &= 0x3ff;
            mnc &= 0x3fff;
            colour &= 0x3f;
            return (static_cast<std::uint32_t>(mcc) << 22)
                | (static_cast<std::uint32_t>(mnc) << 8)
                | (static_cast<std::uint32_t>(colour) << 2)
                | 3U;
        }

        std::uint32_t createScramblerCode(int mnc, int sourceAddress) {
            mnc &= 0x3f;
            sourceAddress &= 0xffffff;
            return (static_cast<std::uint32_t>(mnc) << 26)
                | (static_cast<std::uint32_t>(sourceAddress) << 2)
                | 3U;
        }
    }

    void NetworkTime::synchronize(int tn, int fn, int mn) {
        ++tn;
        isSynchronized_ = (tn == tn_) && (fn == fn_) && (mn == mn_);
        tn_ = tn < 5 ? (tn > 0 ? tn : 1) : 4;
        fn_ = fn < 19 ? (fn > 0 ? fn : 1) : 18;
        mn_ = mn < 61 ? (mn > 0 ? mn : 1) : 60;
        calculateSlaveTime();
        timeChecker();
    }

    void NetworkTime::synchronizeMaster(int tn, int fn) {
        ++tn;
        isSynchronized_ = (tn == tn_) && (fn == fn_);
        tn_ = tn < 5 ? (tn > 0 ? tn : 1) : 4;
        fn_ = fn < 19 ? (fn > 0 ? fn : 1) : 18;
        calculateSlaveTime();
        timeChecker();
    }

    void NetworkTime::synchronizeSlave(int tn, int fn) {
        ++tn;
        isSynchronized_ = (tn == tnSlave_) && (fn == fnSlave_);
        tn_ = tn + 3;
        fn_ = fn;
        if (tn_ > 4) {
            tn_ -= 4;
            ++fn_;
        }
        calculateSlaveTime();
        timeChecker();
    }

    void NetworkTime::addTimeSlot() {
        ++tn_;
        if (tn_ > 4) {
            tn_ = 1;
            ++fn_;
        }
        if (fn_ > 18) {
            fn_ = 1;
            ++mn_;
        }
        if (mn_ > 60) {
            mn_ = 1;
        }
        calculateSlaveTime();
        timeChecker();
    }

    void NetworkTime::timeChecker() {
        fn18_ = fn_ == 18;
        timeBSCH_ = ((4 - (mn_ + 1) % 4) == tn_) && fn18_;
        timeBNCH_ = ((4 - (mn_ + 3) % 4) == tn_) && fn18_;
        fn18Slave_ = fnSlave_ == 18;
    }

    void NetworkTime::calculateSlaveTime() {
        tnSlave_ = tn_ - 3;
        fnSlave_ = fn_;
        if (tnSlave_ < 1) {
            tnSlave_ += 4;
            --fnSlave_;
        }
        if (fnSlave_ < 1) {
            fnSlave_ += 18;
        }
    }

    int GlobalContext::parseParams(const LogicChannel& channelData, int offset, const std::vector<Rule>& rules, ReceivedData& result) {
        int skipRules = 0;

        for (std::size_t i = 0; i < rules.size(); ++i) {
            if (offset >= channelData.length) {
                if (!result.contains(GlobalNames::OutOfBuffer)) {
                    result.setValue(GlobalNames::OutOfBuffer, 1);
                }
                return offset;
            }

            if (skipRules > 0) {
                --skipRules;
                continue;
            }

            const Rule& param = rules[i];
            const int value = utils::bitsToInt32(channelData.ptr, offset, param.length);

            switch (param.type) {
            case RulesType::Direct:
                if (param.globalName != GlobalNames::Reserved) {
                    result.setValue(param.globalName, value);
                }
                offset += param.length;
                continue;

            case RulesType::Options_bit:
                offset += param.length;
                if (value == 0) {
                    return offset;
                }
                break;

            case RulesType::Presence_bit:
                if (value == 0) {
                    skipRules = param.ext1;
                }
                offset += param.length;
                break;

            case RulesType::More_bit:
                offset += param.length;
                return offset;

            case RulesType::Switch:
                if (result.value(static_cast<GlobalNames>(param.ext1)) == param.ext2) {
                    if (param.globalName != GlobalNames::Reserved) {
                        result.setValue(param.globalName, value);
                    }
                    offset += param.length;
                }
                break;

            case RulesType::SwitchNot:
                if (result.value(static_cast<GlobalNames>(param.ext1)) != param.ext2) {
                    if (param.globalName != GlobalNames::Reserved) {
                        result.setValue(param.globalName, value);
                    }
                    offset += param.length;
                }
                break;

            case RulesType::Jamp:
                if (result.value(static_cast<GlobalNames>(param.ext1)) == param.ext2) {
                    skipRules = param.ext3;
                }
                break;

            case RulesType::JampNot:
                if (result.value(static_cast<GlobalNames>(param.ext1)) != param.ext2) {
                    skipRules = param.ext3;
                }
                break;

            case RulesType::Reserved:
                offset += param.length;
                break;
            }
        }

        return offset;
    }

    long long GlobalContext::frequencyCalc(bool isFull, int carrier, int band, int offset) {
        if (isFull) {
            currentBand_ = band;
            currentOffset_ = offset;
        }

        long long freq = static_cast<long long>(currentBand_) * 100000000LL + static_cast<long long>(carrier) * 25000LL;
        switch (currentOffset_) {
        case 1:
            freq += 6250;
            break;
        case 2:
            freq -= 6250;
            break;
        case 3:
            freq += 12500;
            break;
        default:
            break;
        }
        return freq;
    }

    int GlobalContext::carrierCalc(long long frequency) {
        switch (currentOffset_) {
        case 1:
            frequency -= 6250;
            break;
        case 2:
            frequency += 6250;
            break;
        case 3:
            frequency -= 12500;
            break;
        default:
            break;
        }
        return static_cast<int>(std::llround((frequency - static_cast<long long>(currentBand_) * 100000000LL) / 25000.0));
    }
}
