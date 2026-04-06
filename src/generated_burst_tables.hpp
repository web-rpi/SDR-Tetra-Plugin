#pragma once

#include <cstdint>

namespace tetra::tables {
inline constexpr std::uint8_t NormalTrainingSequence1[22] = {
    1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1,
    0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0
};

inline constexpr std::uint8_t NormalTrainingSequence2[22] = {
    0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0,
    0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0
};

inline constexpr std::uint8_t SynchronizationTrainingSequence[38] = {
    1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1,
    0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1
};

inline constexpr std::uint8_t NormalTrainingSequence3[22] = {
    1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1
};

inline constexpr std::uint8_t ExtendedTrainingSequence[30] = {
    1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1,
    0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1
};

inline constexpr std::uint8_t TailBits[4] = { 1, 1, 0, 0 };

inline constexpr float FrequencyCorrectionField[40] = {
    -2.356194f, -2.356194f, -2.356194f, -2.356194f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,  0.7853982f,
     0.7853982f,  0.7853982f,
    -2.356194f, -2.356194f, -2.356194f, -2.356194f
};
}
