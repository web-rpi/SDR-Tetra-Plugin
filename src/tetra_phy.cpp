#include "tetra_phy.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace tetra {
    namespace {
        constexpr float PI_DIV_TWO = 1.57079632679489661923f;
    }

    PhyLevel::PhyLevel() {
        tempBuffer_.resize(BurstLength * 2);
        outBuffer_.resize(BurstLength);
    }

    Burst PhyLevel::parseTrainingSequence(const float* inBuffer, int length) {
        Burst burst;
        burst.type = BurstType::None;
        burst.length = BurstLength;
        burst.ptr = outBuffer_.data();
        convertAngleToDiBits(burst.ptr, inBuffer, length);
        return burst;
    }

    void PhyLevel::convertAngleToDiBits(std::uint8_t* bitsBuffer, const float* angles, int sourceLength) const {
        while (sourceLength-- > 0) {
            const float delta = *angles++;
            *bitsBuffer++ = delta < 0.0f ? 1 : 0;
            *bitsBuffer++ = std::fabs(delta) > PI_DIV_TWO ? 1 : 0;
        }
    }

    void PhyLevel::extractPhyChannels(Mode mode, const Burst& burst, std::uint8_t* bbBuffer, std::uint8_t* bkn1Buffer, std::uint8_t* bkn2Buffer) const {
        int offset = 2;
        switch (mode) {
        case Mode::TMO:
            switch (burst.type) {
            case BurstType::NDB1:
            case BurstType::NDB2:
                offset += nts3_pre_Length + phaseAdjust_Length;
                blockCopy(burst.ptr, offset, bkn1Buffer, 0, bkn_Length);
                offset += bkn_Length;
                blockCopy(burst.ptr, offset, bbBuffer, 0, bb1_Length);
                offset += bb1_Length + nts_Length;
                blockCopy(burst.ptr, offset, bbBuffer, bb1_Length, bb2_Length);
                offset += bb2_Length;
                blockCopy(burst.ptr, offset, bkn2Buffer, 0, bkn_Length);
                break;
            case BurstType::SYNC:
                offset += nts3_pre_Length + phaseAdjust_Length + freqCorrection_Length;
                offset += sts_Length + sb_Length;
                blockCopy(burst.ptr, offset, bbBuffer, 0, bb_Length);
                offset += bb_Length;
                blockCopy(burst.ptr, offset, bkn2Buffer, 0, bkn_Length);
                break;
            default:
                break;
            }
            break;
        case Mode::DMO:
            switch (burst.type) {
            case BurstType::NDB1:
            case BurstType::NDB2:
                offset += nts3_pre_Length + phaseAdjust_Length;
                blockCopy(burst.ptr, offset, bkn1Buffer, 0, bkn_Length);
                offset += bkn_Length + nts_Length;
                blockCopy(burst.ptr, offset, bkn2Buffer, 0, bkn_Length);
                break;
            case BurstType::SYNC:
                offset += nts3_pre_Length + phaseAdjust_Length + freqCorrection_Length;
                offset += sb_Length + sts_Length;
                blockCopy(burst.ptr, offset, bkn2Buffer, 0, bkn_Length);
                break;
            default:
                break;
            }
            break;
        }
    }

    void PhyLevel::extractSBChannels(const Burst& burst, std::uint8_t* sb1Buffer) const {
        int offset = 2;
        offset += nts3_pre_Length + phaseAdjust_Length + freqCorrection_Length;
        blockCopy(burst.ptr, offset, sb1Buffer, 0, sb_Length);
    }

    void PhyLevel::blockCopy(const std::uint8_t* source, int sourceOffset, std::uint8_t* dest, int destOffset, int length) const {
        std::memcpy(dest + destOffset, source + sourceOffset, static_cast<std::size_t>(length));
    }

    LowerMacLevel::LowerMacLevel() {
        type1Buffer_.resize(2048);
        type2Buffer_.resize(2048);
        type3Buffer_.resize(2048);
        type4Buffer_.resize(2048);
        tempBuffer_.resize(2048);
        rmd_.init();
    }

    LogicChannel LowerMacLevel::extractLogicChannelFromBB(std::uint8_t* type5BufferPtr, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type1Buffer_.data();
        logicChannel.length = 14;
        scrambler_.process(type5BufferPtr, length, scramblerSequence_);
        logicChannel.crcIsOk = rmd_.process(type5BufferPtr, type1Buffer_.data());
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractVoiceDataFromBKN1BKN2(const std::uint8_t* type5Buffer1, const std::uint8_t* type5Buffer2, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type4Buffer_.data();
        logicChannel.length = 432;
        std::memcpy(type4Buffer_.data(), type5Buffer1, static_cast<std::size_t>(length));
        std::memcpy(type4Buffer_.data() + length, type5Buffer2, static_cast<std::size_t>(length));
        scrambler_.process(type4Buffer_.data(), length * 2, scramblerSequence_);
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractVoiceDataFromBKN2(const std::uint8_t* type5Buffer1, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type4Buffer_.data();
        logicChannel.length = 432;
        std::fill(type4Buffer_.begin(), type4Buffer_.begin() + 216, 0);
        std::memcpy(type4Buffer_.data() + 216, type5Buffer1, static_cast<std::size_t>(length));
        scrambler_.process(type4Buffer_.data() + length, length, scramblerSequence_);
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractLogicChannelFromBKN1BKN2(const std::uint8_t* type5Buffer1, const std::uint8_t* type5Buffer2, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type1Buffer_.data();
        logicChannel.length = 268;
        std::memcpy(type4Buffer_.data(), type5Buffer1, static_cast<std::size_t>(length));
        std::memcpy(type4Buffer_.data() + length, type5Buffer2, static_cast<std::size_t>(length));
        scrambler_.process(type4Buffer_.data(), length * 2, scramblerSequence_);
        deinterleaver_.process(type4Buffer_.data(), type3Buffer_.data(), 432U, 103U);
        depuncture_.process(Depuncture::PunctType::PUNCT_2_3, type3Buffer_.data(), tempBuffer_.data(), 432);
        ber_ = mother_.bufferDecode(tempBuffer_.data(), type2Buffer_.data(), 1152);
        logicChannel.crcIsOk = crc_.process(type2Buffer_.data(), type1Buffer_.data(), 284);
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractLogicChannelFromBKN(std::uint8_t* type5Buffer, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type1Buffer_.data();
        logicChannel.length = 124;
        scrambler_.process(type5Buffer, length, scramblerSequence_);
        deinterleaver_.process(type5Buffer, type3Buffer_.data(), 216U, 101U);
        depuncture_.process(Depuncture::PunctType::PUNCT_2_3, type3Buffer_.data(), tempBuffer_.data(), 216);
        ber_ = mother_.bufferDecode(tempBuffer_.data(), type2Buffer_.data(), 576);
        logicChannel.crcIsOk = crc_.process(type2Buffer_.data(), type1Buffer_.data(), 140);
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractLogicChannelFromSB(std::uint8_t* type5Buffer, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type1Buffer_.data();
        logicChannel.length = 60;
        scrambler_.process(type5Buffer, length, 3U);
        deinterleaver_.process(type5Buffer, type3Buffer_.data(), 120U, 11U);
        depuncture_.process(Depuncture::PunctType::PUNCT_2_3, type3Buffer_.data(), tempBuffer_.data(), 120);
        ber_ = mother_.bufferDecode(tempBuffer_.data(), type2Buffer_.data(), 320);
        logicChannel.crcIsOk = crc_.process(type2Buffer_.data(), type1Buffer_.data(), 76);
        return logicChannel;
    }

    LogicChannel LowerMacLevel::extractLogicChannelFromBKN2(std::uint8_t* type5Buffer, int length) {
        LogicChannel logicChannel;
        logicChannel.timeSlot = ts;
        logicChannel.frame = fr;
        logicChannel.ptr = type1Buffer_.data();
        logicChannel.length = 124;
        scrambler_.process(type5Buffer, length, 3U);
        deinterleaver_.process(type5Buffer, type3Buffer_.data(), 216U, 101U);
        depuncture_.process(Depuncture::PunctType::PUNCT_2_3, type3Buffer_.data(), tempBuffer_.data(), 216);
        ber_ = mother_.bufferDecode(tempBuffer_.data(), type2Buffer_.data(), 576);
        logicChannel.crcIsOk = crc_.process(type2Buffer_.data(), type1Buffer_.data(), 140);
        return logicChannel;
    }
}
