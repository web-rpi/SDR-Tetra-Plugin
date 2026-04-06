#pragma once

#include "generated_burst_tables.hpp"
#include "tetra_core.hpp"
#include "tetra_fec.hpp"
#include "tetra_parser.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace tetra {
    class PhyLevel {
    public:
        static constexpr int BurstLength = 510;
        static constexpr int preGuardPeriod_Length = 34;
        static constexpr int postGuardPeriod_Length = 14;
        static constexpr int tailBits_Length = 4;
        static constexpr int nts_Length = 22;
        static constexpr int nts3_pre_Length = 12;
        static constexpr int nts3_post_Length = 10;
        static constexpr int sts_Length = 38;
        static constexpr int ets_Length = 30;
        static constexpr int phaseAdjust_Length = 2;
        static constexpr int bkn_Length = 216;
        static constexpr int bb1_Length = 14;
        static constexpr int bb2_Length = 16;
        static constexpr int bb_Length = bb1_Length + bb2_Length;
        static constexpr int cb_Length = 84;
        static constexpr int freqCorrection_Length = 80;
        static constexpr int sb_Length = 120;

        PhyLevel();

        Burst parseTrainingSequence(const float* inBuffer, int length);
        void extractPhyChannels(Mode mode, const Burst& burst, std::uint8_t* bbBuffer, std::uint8_t* bkn1Buffer, std::uint8_t* bkn2Buffer) const;
        void extractSBChannels(const Burst& burst, std::uint8_t* sb1Buffer) const;

    private:
        void convertAngleToDiBits(std::uint8_t* bitsBuffer, const float* angles, int sourceLength) const;
        void blockCopy(const std::uint8_t* source, int sourceOffset, std::uint8_t* dest, int destOffset, int length) const;

        std::vector<std::uint8_t> tempBuffer_;
        std::vector<std::uint8_t> outBuffer_;
    };

    class LowerMacLevel {
    public:
        LowerMacLevel();

        void setScramblerCode(std::uint32_t value) { scramblerSequence_ = value; }
        float ber() const { return ber_; }
        int ts = 0;
        int fr = 0;

        LogicChannel extractLogicChannelFromBB(std::uint8_t* type5BufferPtr, int length);
        LogicChannel extractVoiceDataFromBKN1BKN2(const std::uint8_t* type5Buffer1, const std::uint8_t* type5Buffer2, int length);
        LogicChannel extractVoiceDataFromBKN2(const std::uint8_t* type5Buffer1, int length);
        LogicChannel extractLogicChannelFromBKN1BKN2(const std::uint8_t* type5Buffer1, const std::uint8_t* type5Buffer2, int length);
        LogicChannel extractLogicChannelFromBKN(std::uint8_t* type5Buffer, int length);
        LogicChannel extractLogicChannelFromSB(std::uint8_t* type5Buffer, int length);
        LogicChannel extractLogicChannelFromBKN2(std::uint8_t* type5Buffer, int length);

    private:
        Scrambler scrambler_;
        Deinterleave deinterleaver_;
        Depuncture depuncture_;
        CRC16 crc_;
        Rm3014 rmd_;
        MotherCode mother_;
        std::vector<std::uint8_t> type1Buffer_;
        std::vector<std::uint8_t> type2Buffer_;
        std::vector<std::uint8_t> type3Buffer_;
        std::vector<std::uint8_t> type4Buffer_;
        std::vector<std::int8_t> tempBuffer_;
        std::uint32_t scramblerSequence_ = 0;
        float ber_ = 0.0f;
    };
}
