#pragma once

#include "generated_mothercode_tables.hpp"
#include "generated_rm3014_tables.hpp"
#include "tetra_core.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace tetra {
    class CRC16 {
    public:
        int calcBuffer(const std::uint8_t* buffer, int length) const;
        bool process(const std::uint8_t* source, std::uint8_t* dest, int sourceLength) const;
    };

    class Deinterleave {
    public:
        void process(const std::uint8_t* source, std::uint8_t* dest, std::uint32_t destLength, std::uint32_t a) const;
    };

    class Depuncture {
    public:
        enum class PunctType {
            PUNCT_2_3,
            PUNCT_1_3,
            PUNCT_292_432,
            PUNCT_148_432,
            PUNCT_112_168,
            PUNCT_72_162,
            PUNCT_38_80,
        };

        void process(PunctType puncType, const std::uint8_t* source, std::int8_t* dest, int sourceLength) const;
    };

    class Scrambler {
    public:
        static constexpr std::uint32_t DefaultScramblerInit = 3;

        Scrambler();
        void process(std::uint8_t* buffer, int length, std::uint32_t scrambSequence) const;

    private:
        std::array<std::uint8_t, 256> onesCount_{};
    };

    class MotherCode {
    public:
        float bufferDecode(const std::int8_t* source, std::uint8_t* dest, int sourceLength);

    private:
        std::array<std::uint8_t, 16384> hamingLengthResult_{};
        std::array<std::uint8_t, 8192> tempBuffer_{};
        std::array<int, 32> prevSum_{};
        std::array<int, 16> currentSum_{};
    };

    class Rm3014 {
    public:
        void init();
        bool process(const std::uint8_t* inBuffer, std::uint8_t* outBuffer) const;

    private:
        std::vector<std::uint8_t> onesCounter_;
        std::vector<std::uint32_t> syndromesDecoder_;
        std::array<std::uint32_t, 16> hMatrixUint_{};
    };
}
