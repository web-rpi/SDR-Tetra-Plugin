#include "tetra_fec.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace tetra {
    namespace {
        constexpr int GEN_POLY = 33800;
        constexpr int GOOD_CRC = 61624;
        constexpr std::uint32_t SCRAMBLER_POLY = 0xDB710641U;
        constexpr std::uint32_t RM_MSB = 536870912U;
    }

    int CRC16::calcBuffer(const std::uint8_t* buffer, int length) const {
        int crc = static_cast<int>(std::numeric_limits<std::uint16_t>::max());
        for (int index = 0; index < length; ++index) {
            const int bit = static_cast<int>(static_cast<std::uint16_t>(buffer[index] ^ (crc & 1)));
            crc >>= 1;
            if (bit != 0) {
                crc ^= GEN_POLY;
            }
        }
        return crc;
    }

    bool CRC16::process(const std::uint8_t* source, std::uint8_t* dest, int sourceLength) const {
        const int crc = calcBuffer(source, sourceLength);
        const int payloadLength = sourceLength - 16;
        std::memcpy(dest, source, static_cast<std::size_t>(payloadLength));
        return crc == GOOD_CRC;
    }

    void Deinterleave::process(const std::uint8_t* source, std::uint8_t* dest, std::uint32_t destLength, std::uint32_t a) const {
        for (std::uint32_t i = 1; i <= destLength; ++i) {
            const std::uint32_t k = 1U + (a * i) % destLength;
            dest[i - 1U] = source[k - 1U];
        }
    }

    void Depuncture::process(PunctType puncType, const std::uint8_t* source, std::int8_t* dest, int sourceLength) const {
        static constexpr std::uint8_t rate1_3[] = { 0, 1, 2, 3, 5, 6, 7 };
        static constexpr std::uint8_t rate2_3[] = { 0, 1, 2, 5 };
        static constexpr std::uint8_t rate8_12[] = { 0, 1, 2, 4 };
        static constexpr std::uint8_t rate8_18[] = { 0, 1, 2, 3, 4, 5, 7, 8, 10, 11 };
        static constexpr std::uint8_t rate8_17[] = { 0, 1, 2, 3, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 };

        std::fill(dest, dest + static_cast<std::size_t>(sourceLength * 4), 0);

        const std::uint8_t* pattern = nullptr;
        std::uint8_t num1 = 0;
        std::uint8_t num2 = 0;
        enum class FunctionType { Equals, Func292, Func148 };
        FunctionType functionType = FunctionType::Equals;

        switch (puncType) {
        case PunctType::PUNCT_2_3:
            pattern = rate2_3;
            num1 = 3;
            num2 = 8;
            break;
        case PunctType::PUNCT_1_3:
            pattern = rate1_3;
            num1 = 6;
            num2 = 8;
            break;
        case PunctType::PUNCT_292_432:
            pattern = rate2_3;
            num1 = 3;
            num2 = 8;
            functionType = FunctionType::Func292;
            break;
        case PunctType::PUNCT_148_432:
            pattern = rate1_3;
            num1 = 6;
            num2 = 8;
            functionType = FunctionType::Func148;
            break;
        case PunctType::PUNCT_112_168:
            pattern = rate8_12;
            num1 = 3;
            num2 = 6;
            break;
        case PunctType::PUNCT_72_162:
            pattern = rate8_18;
            num1 = 9;
            num2 = 12;
            break;
        case PunctType::PUNCT_38_80:
            pattern = rate8_17;
            num1 = 17;
            num2 = 24;
            break;
        }

        for (std::uint32_t index = 1; static_cast<int>(index) <= sourceLength; ++index) {
            std::uint32_t num3 = index;
            if (functionType == FunctionType::Func292) {
                num3 = index + (index - 1U) / 65U;
            }
            else if (functionType == FunctionType::Func148) {
                num3 = index + (index - 1U) / 35U;
            }

            const std::uint32_t num4 = static_cast<std::uint32_t>(num2) * ((num3 - 1U) / static_cast<std::uint32_t>(num1))
                + static_cast<std::uint32_t>(pattern[num3 - static_cast<std::uint32_t>(num1) * ((num3 - 1U) / static_cast<std::uint32_t>(num1))]);
            dest[num4 - 1U] = source[index - 1U] == 0 ? static_cast<std::int8_t>(-1) : static_cast<std::int8_t>(1);
        }
    }

    Scrambler::Scrambler() {
        for (std::uint32_t j = 0; j < onesCount_.size(); ++j) {
            std::uint32_t predBits = j;
            std::uint32_t counter = 0;
            while (predBits != 0) {
                counter += predBits & 1U;
                predBits >>= 1;
            }
            onesCount_[j] = static_cast<std::uint8_t>(counter);
        }
    }

    void Scrambler::process(std::uint8_t* buffer, int length, std::uint32_t scrambSequence) const {
        for (int i = 0; i < length; ++i) {
            const std::uint32_t key = scrambSequence & SCRAMBLER_POLY;
            const std::uint32_t bit = static_cast<std::uint32_t>(
                onesCount_[(key >> 24) & 0xffU] +
                onesCount_[(key >> 16) & 0xffU] +
                onesCount_[(key >> 8) & 0xffU] +
                onesCount_[key & 0xffU]
            ) & 0x1U;

            scrambSequence = (scrambSequence >> 1U) | (bit << 31U);
            buffer[i] ^= static_cast<std::uint8_t>(bit);
        }
    }

    float MotherCode::bufferDecode(const std::int8_t* source, std::uint8_t* dest, int sourceLength) {
        int sourceIndex = 0;
        const int outputLength = sourceLength / 4 - 4;
        int erasureCount = 0;

        for (int index1 = 0; index1 < outputLength; ++index1) {
            const int offset = index1 * 32;
            const int num6 = source[sourceIndex++];
            const int num8 = source[sourceIndex++];
            const int num10 = source[sourceIndex++];
            const int num11 = source[sourceIndex++];

            erasureCount += (num6 == 0) + (num8 == 0) + (num10 == 0) + (num11 == 0);

            for (int index6 = 0; index6 < 32; ++index6) {
                const int num12 =
                    absLut[lutBitsG1[index6] - num6 + 2] +
                    absLut[lutBitsG2[index6] - num8 + 2] +
                    absLut[lutBitsG3[index6] - num10 + 2] +
                    absLut[lutBitsG4[index6] - num11 + 2];
                hamingLengthResult_[offset + index6] = static_cast<std::uint8_t>(num12);
            }
        }

        prevSum_.fill(0);
        currentSum_.fill(0);

        for (int index1 = 0; index1 < outputLength; ++index1) {
            const int hamingOffset = index1 * 32;
            const int tempOffset = index1 * 16;

            for (int index2 = 0; index2 < 16; ++index2) {
                const int left = index2 * 2;
                const int right = left + 1;
                const int sumLeft = hamingLengthResult_[hamingOffset + left] + prevSum_[left];
                const int sumRight = hamingLengthResult_[hamingOffset + right] + prevSum_[right];

                if (sumRight > sumLeft) {
                    currentSum_[index2] = sumLeft;
                    tempBuffer_[tempOffset + index2] = static_cast<std::uint8_t>(left);
                }
                else {
                    currentSum_[index2] = sumRight;
                    tempBuffer_[tempOffset + index2] = static_cast<std::uint8_t>(right);
                }
            }

            for (int index2 = 0; index2 < 16; ++index2) {
                prevSum_[index2] = currentSum_[index2];
                prevSum_[index2 + 16] = currentSum_[index2];
            }
        }

        int bestMetric = std::numeric_limits<int>::max();
        int state = 0;
        for (int index = 0; index < 16; ++index) {
            if (currentSum_[index] < bestMetric) {
                bestMetric = currentSum_[index];
                state = index;
            }
        }

        for (int index = outputLength - 1; index >= 0; --index) {
            dest[index] = state < 8 ? 0 : 1;
            state = tempBuffer_[index * 16 + state] & 15;
        }

        return static_cast<float>((static_cast<double>(bestMetric - erasureCount) / static_cast<double>(sourceLength - erasureCount)) * 100.0);
    }

    void Rm3014::init() {
        syndromesDecoder_.assign(1U << 16U, 0U);
        onesCounter_.assign(256, 0U);

        int syndromeI = 0;

        for (int i1 = 0; i1 < 30; ++i1) {
            for (int i2 = i1 + 1; i2 < 30; ++i2) {
                for (int i3 = i2 + 1; i3 < 30; ++i3) {
                    syndromeI = 0;
                    for (int ir = 0; ir < 16; ++ir) {
                        syndromeI <<= 1;
                        syndromeI += hMatrix[i1][ir] ^ hMatrix[i2][ir] ^ hMatrix[i3][ir];
                    }
                    syndromesDecoder_[static_cast<std::size_t>(syndromeI)] =
                        (RM_MSB >> i1) | (RM_MSB >> i2) | (RM_MSB >> i3);
                }

                syndromeI = 0;
                for (int ir = 0; ir < 16; ++ir) {
                    syndromeI <<= 1;
                    syndromeI += hMatrix[i1][ir] ^ hMatrix[i2][ir];
                }
                syndromesDecoder_[static_cast<std::size_t>(syndromeI)] = (RM_MSB >> i1) | (RM_MSB >> i2);
            }

            syndromeI = 0;
            for (int ir = 0; ir < 16; ++ir) {
                syndromeI <<= 1;
                syndromeI += hMatrix[i1][ir];
            }
            syndromesDecoder_[static_cast<std::size_t>(syndromeI)] = (RM_MSB >> i1);
        }

        for (std::uint32_t j = 0; j < onesCounter_.size(); ++j) {
            std::uint32_t predBits = j;
            std::uint32_t counter = 0;
            while (predBits != 0) {
                counter += predBits & 1U;
                predBits >>= 1;
            }
            onesCounter_[j] = static_cast<std::uint8_t>(counter);
        }

        for (int j = 0; j < 16; ++j) {
            hMatrixUint_[j] = 0;
            for (int i = 0; i < 30; ++i) {
                hMatrixUint_[j] <<= 1U;
                hMatrixUint_[j] |= static_cast<std::uint32_t>(hMatrix[i][j]);
            }
        }
    }

    bool Rm3014::process(const std::uint8_t* inBuffer, std::uint8_t* outBuffer) const {
        std::uint32_t alphaValue = 0;
        int counter = 0;
        std::uint32_t vector = utils::bitsToUInt32(inBuffer, 0, 30);

        int syndromeI = 0;
        for (int i = 0; i < 16; ++i) {
            alphaValue = vector & hMatrixUint_[i];
            counter =
                onesCounter_[alphaValue & 0xffU] +
                onesCounter_[(alphaValue >> 8U) & 0xffU] +
                onesCounter_[(alphaValue >> 16U) & 0xffU] +
                onesCounter_[(alphaValue >> 24U) & 0xffU];

            syndromeI <<= 1;
            syndromeI |= (counter & 1);
        }

        bool noErrors = syndromeI == 0;
        if (!noErrors) {
            const std::uint32_t bitMask = syndromesDecoder_[static_cast<std::size_t>(syndromeI)];
            vector ^= bitMask;
            noErrors = bitMask != 0;
        }

        for (int i = 0; i < 14; ++i) {
            outBuffer[i] = (vector & RM_MSB) == 0 ? 0 : 1;
            vector <<= 1U;
        }

        return noErrors;
    }
}
