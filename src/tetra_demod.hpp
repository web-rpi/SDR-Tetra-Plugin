#pragma once

#include "generated_burst_tables.hpp"
#include "tetra_core.hpp"
#include <dsp/types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetra {
    class Demodulator {
    public:
        struct Result {
            Burst burst;
            const float* angles = nullptr;
            int angleCount = 0;
        };

        Demodulator();

        const Result& processBuffer(const dsp::complex_t* iqBuffer, double iqSamplerate, int iqBufferLength, Mode mode);
        void reset();

    private:
        void reconfigure(double iqSamplerate, int iqBufferLength);
        void createFrameSynchronization();
        void createFsBuffers(double symbolLength);
        void applyMatchedFilter(const std::vector<dsp::complex_t>& input, std::vector<dsp::complex_t>& output);
        void applyFloatFilter(std::vector<float>& buffer);
        void angleToSymbol(std::uint8_t* bitsBuffer, const float* angles, int sourceLength) const;
        float symbolToAngle(const std::uint8_t* trainingSequence, int symbolIndex) const;
        void updateAngles(const std::vector<dsp::complex_t>& filtered);
        void sampleBurst(float* digitalBuffer, int offset);
        std::vector<float> makeSincTaps(double sampleRate, double cutoff, int tapCount) const;

        static constexpr float PI_THREE_QUARTER = 2.356194f;
        static constexpr float PI_QUARTER = 0.7853982f;
        static constexpr float PI_HALF = 1.570796f;
        static constexpr double SYMBOL_RATE = 18000.0;
        static constexpr int NTS_SEQUENCE_OFFSET_TMO = 122;
        static constexpr int NTS_SEQUENCE_OFFSET_DMO = 115;
        static constexpr int STS_SEQUENCE_OFFSET = 107;
        static constexpr int SMALL_TRAINING_WINDOW = 6;
        static constexpr int SYNC_LOST_VALUE = 8;
        static constexpr double MIN_WORK_SAMPLE_RATE = 90000.0;
        static constexpr int BURST_SYMBOL_COUNT = 255;
        static constexpr int BURST_BIT_COUNT = BURST_SYMBOL_COUNT * 2;

        Result result_{};
        Burst burst_{};
        std::vector<std::uint8_t> burstBits_;
        std::vector<dsp::complex_t> interpolated_;
        std::vector<dsp::complex_t> filtered_;
        std::vector<dsp::complex_t> firHistory_;
        std::vector<dsp::complex_t> symbolTail_;
        std::vector<float> angleWindow_;
        std::vector<float> burstAngles_;
        std::vector<float> nts1Buffer_;
        std::vector<float> nts2Buffer_;
        std::vector<float> stsBuffer_;
        std::vector<float> firTaps_;
        int filterLength_ = 0;
        double samplerateIn_ = 0.0;
        double samplerate_ = 0.0;
        int length_ = 0;
        int interpolation_ = 1;
        double symbolLength_ = 0.0;
        int windowLength_ = 0;
        int writeAddress_ = 0;
        int tailBufferLength_ = 0;
        int syncCounter_ = 0;
        int ntsOffset_ = 0;
        int stsOffset_ = 0;
    };
}
