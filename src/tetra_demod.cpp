#include "tetra_demod.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace tetra {
    namespace {
        constexpr float PI_VALUE = 3.14159265358979323846f;
    }

    Demodulator::Demodulator() {
        reset();
    }

    void Demodulator::reset() {
        samplerateIn_ = 0.0;
        samplerate_ = 0.0;
        length_ = 0;
        interpolation_ = 1;
        symbolLength_ = 0.0;
        windowLength_ = 0;
        writeAddress_ = 0;
        tailBufferLength_ = 0;
        syncCounter_ = 0;
        ntsOffset_ = 0;
        stsOffset_ = 0;
        filterLength_ = 0;
        firTaps_.clear();
        firHistory_.clear();
        symbolTail_.clear();
        angleWindow_.clear();
        burstAngles_.assign(BURST_SYMBOL_COUNT, 0.0f);
        burstBits_.assign(BURST_BIT_COUNT, 0);
        burst_ = {};
        burst_.ptr = burstBits_.data();
        burst_.length = 0;
        result_.burst = burst_;
        result_.angles = burstAngles_.data();
        result_.angleCount = BURST_SYMBOL_COUNT;
    }

    const Demodulator::Result& Demodulator::processBuffer(
        const dsp::complex_t* iqBuffer,
        double iqSamplerate,
        int iqBufferLength,
        Mode mode
    ) {
        burst_.mode = mode;
        burst_.type = BurstType::WaitBurst;
        burst_.ptr = burstBits_.data();
        burst_.length = 0;
        result_.burst = burst_;
        result_.angles = burstAngles_.data();
        result_.angleCount = BURST_SYMBOL_COUNT;

        if (iqBuffer == nullptr || iqBufferLength <= 0 || iqSamplerate <= 0.0) {
            return result_;
        }

        if (samplerateIn_ != iqSamplerate || length_ != iqBufferLength * interpolation_) {
            reconfigure(iqSamplerate, iqBufferLength);
        }

        interpolated_.assign(static_cast<std::size_t>(length_), dsp::complex_t{ 0.0f, 0.0f });
        if (interpolation_ > 1) {
            for (int i = 0; i < iqBufferLength; ++i) {
                interpolated_[static_cast<std::size_t>(i * interpolation_)] = iqBuffer[i];
            }
        }
        else {
            std::memcpy(interpolated_.data(), iqBuffer, static_cast<std::size_t>(length_) * sizeof(dsp::complex_t));
        }

        applyMatchedFilter(interpolated_, filtered_);
        updateAngles(filtered_);

        if (writeAddress_ < (windowLength_ * 2)) {
            return result_;
        }

        ntsOffset_ = static_cast<int>(std::llround((mode == Mode::TMO ? NTS_SEQUENCE_OFFSET_TMO : NTS_SEQUENCE_OFFSET_DMO) * symbolLength_));
        stsOffset_ = static_cast<int>(std::llround(STS_SEQUENCE_OFFSET * symbolLength_));

        float ndb1MinSum = std::numeric_limits<float>::max();
        int ndb1Index = 0;
        float ndb2MinSum = std::numeric_limits<float>::max();
        int ndb2Index = 0;
        float stsMinSum = std::numeric_limits<float>::max();
        int stsIndex = 0;

        int trainingWindow = (syncCounter_ > 0) ? (SMALL_TRAINING_WINDOW * tailBufferLength_) : windowLength_;
        if (syncCounter_ > 0) {
            --syncCounter_;
        }

        const int maxTrainingWindow = std::max(
            0,
            writeAddress_ - std::max(ntsOffset_ + static_cast<int>(nts1Buffer_.size()), stsOffset_ + static_cast<int>(stsBuffer_.size()))
        );
        trainingWindow = std::min(trainingWindow, maxTrainingWindow);

        for (int index1 = 0; index1 < trainingWindow; ++index1) {
            float ndb1Sum = 0.0f;
            for (std::size_t index2 = 0; index2 < nts1Buffer_.size(); ++index2) {
                const float sample = angleWindow_[static_cast<std::size_t>(index1 + ntsOffset_) + index2];
                const float training = nts1Buffer_[index2];
                const float delta = training - sample;
                ndb1Sum += delta * delta;
            }
            if (ndb1Sum < ndb1MinSum) {
                ndb1MinSum = ndb1Sum;
                ndb1Index = index1;
            }

            float ndb2Sum = 0.0f;
            for (std::size_t index2 = 0; index2 < nts2Buffer_.size(); ++index2) {
                const float sample = angleWindow_[static_cast<std::size_t>(index1 + ntsOffset_) + index2];
                const float training = nts2Buffer_[index2];
                const float delta = training - sample;
                ndb2Sum += delta * delta;
            }
            if (ndb2Sum < ndb2MinSum) {
                ndb2MinSum = ndb2Sum;
                ndb2Index = index1;
            }

            float stsSum = 0.0f;
            for (std::size_t index2 = 0; index2 < stsBuffer_.size(); ++index2) {
                const float sample = angleWindow_[static_cast<std::size_t>(index1 + stsOffset_) + index2];
                const float training = stsBuffer_[index2];
                const float delta = training - sample;
                stsSum += delta * delta;
            }
            if (stsSum < stsMinSum) {
                stsMinSum = stsSum;
                stsIndex = index1;
            }
        }

        if (!nts1Buffer_.empty()) {
            ndb1MinSum /= static_cast<float>(nts1Buffer_.size());
        }
        if (!nts2Buffer_.empty()) {
            ndb2MinSum /= static_cast<float>(nts2Buffer_.size());
        }
        if (!stsBuffer_.empty()) {
            stsMinSum /= static_cast<float>(stsBuffer_.size());
        }

        int offset = 0;
        if (ndb1MinSum < 1.0f || ndb2MinSum < 1.0f || stsMinSum < 1.0f) {
            if (ndb1MinSum < ndb2MinSum && ndb1MinSum < stsMinSum) {
                offset = ndb1Index;
                burst_.type = BurstType::NDB1;
            }
            else if (ndb2MinSum < ndb1MinSum && ndb2MinSum < stsMinSum) {
                offset = ndb2Index;
                burst_.type = BurstType::NDB2;
            }
            else {
                offset = stsIndex;
                burst_.type = BurstType::SYNC;
            }
            syncCounter_ = SYNC_LOST_VALUE;
        }
        else {
            burst_.type = BurstType::None;
            offset = tailBufferLength_ * 2;
        }

        sampleBurst(burstAngles_.data(), offset);
        angleToSymbol(burstBits_.data(), burstAngles_.data(), BURST_SYMBOL_COUNT);
        burst_.length = BURST_BIT_COUNT;
        result_.burst = burst_;
        return result_;
    }

    void Demodulator::reconfigure(double iqSamplerate, int iqBufferLength) {
        samplerateIn_ = iqSamplerate;
        interpolation_ = 1;
        while ((samplerateIn_ * static_cast<double>(interpolation_)) < MIN_WORK_SAMPLE_RATE) {
            ++interpolation_;
        }
        samplerate_ = samplerateIn_ * static_cast<double>(interpolation_);
        length_ = iqBufferLength * interpolation_;
        symbolLength_ = samplerate_ / SYMBOL_RATE;
        windowLength_ = static_cast<int>(std::llround(symbolLength_ * BURST_SYMBOL_COUNT));
        tailBufferLength_ = std::max(1, static_cast<int>(std::llround(symbolLength_)));
        writeAddress_ = 0;
        syncCounter_ = 0;

        filterLength_ = std::max((static_cast<int>(samplerate_ / SYMBOL_RATE) | 1), 5);
        firTaps_ = makeSincTaps(samplerate_, 13500.0, filterLength_);
        firHistory_.assign(static_cast<std::size_t>(std::max(filterLength_ - 1, 0)), dsp::complex_t{ 0.0f, 0.0f });
        symbolTail_.assign(static_cast<std::size_t>(tailBufferLength_), dsp::complex_t{ 0.0f, 0.0f });
        interpolated_.resize(static_cast<std::size_t>(length_));
        filtered_.resize(static_cast<std::size_t>(length_));
        angleWindow_.assign(static_cast<std::size_t>(std::max(1, static_cast<int>(std::llround(samplerate_)))), 0.0f);
        createFrameSynchronization();
    }

    void Demodulator::createFrameSynchronization() {
        createFsBuffers(symbolLength_);

        int symbolIndex = 0;
        for (std::size_t index = 0; index < nts1Buffer_.size(); ++index) {
            const double samplePos = static_cast<double>(index) + symbolLength_ * 0.5;
            if (std::fmod(samplePos, symbolLength_) < 1.0) {
                nts1Buffer_[index] = symbolToAngle(tables::NormalTrainingSequence1, symbolIndex++);
            }
            else {
                nts1Buffer_[index] = 0.0f;
            }
        }

        symbolIndex = 0;
        for (std::size_t index = 0; index < nts2Buffer_.size(); ++index) {
            const double samplePos = static_cast<double>(index) + symbolLength_ * 0.5;
            if (std::fmod(samplePos, symbolLength_) < 1.0) {
                nts2Buffer_[index] = symbolToAngle(tables::NormalTrainingSequence2, symbolIndex++);
            }
            else {
                nts2Buffer_[index] = 0.0f;
            }
        }

        symbolIndex = 0;
        for (std::size_t index = 0; index < stsBuffer_.size(); ++index) {
            const double samplePos = static_cast<double>(index) + symbolLength_ * 0.5;
            if (std::fmod(samplePos, symbolLength_) < 1.0) {
                stsBuffer_[index] = symbolToAngle(tables::SynchronizationTrainingSequence, symbolIndex++);
            }
            else {
                stsBuffer_[index] = 0.0f;
            }
        }

        applyFloatFilter(nts1Buffer_);
        applyFloatFilter(nts2Buffer_);
        applyFloatFilter(stsBuffer_);
    }

    void Demodulator::createFsBuffers(double symbolLength) {
        const int length1 = (static_cast<int>(symbolLength * (sizeof(tables::NormalTrainingSequence1) / sizeof(std::uint8_t) * 0.5)) | 1);
        const int length2 = (static_cast<int>(symbolLength * (sizeof(tables::SynchronizationTrainingSequence) / sizeof(std::uint8_t) * 0.5)) | 1);
        nts1Buffer_.assign(static_cast<std::size_t>(std::max(length1, 1)), 0.0f);
        nts2Buffer_.assign(static_cast<std::size_t>(std::max(length1, 1)), 0.0f);
        stsBuffer_.assign(static_cast<std::size_t>(std::max(length2, 1)), 0.0f);
    }

    void Demodulator::applyMatchedFilter(const std::vector<dsp::complex_t>& input, std::vector<dsp::complex_t>& output) {
        if (firTaps_.empty()) {
            output = input;
            return;
        }

        std::vector<dsp::complex_t> work;
        work.reserve(firHistory_.size() + input.size());
        work.insert(work.end(), firHistory_.begin(), firHistory_.end());
        work.insert(work.end(), input.begin(), input.end());

        output.resize(input.size());
        for (std::size_t i = 0; i < input.size(); ++i) {
            dsp::complex_t acc{ 0.0f, 0.0f };
            for (std::size_t tap = 0; tap < firTaps_.size(); ++tap) {
                const auto& sample = work[i + tap];
                const float coeff = firTaps_[tap];
                acc.re += sample.re * coeff;
                acc.im += sample.im * coeff;
            }
            output[i] = acc;
        }

        const std::size_t histSize = firHistory_.size();
        if (histSize != 0) {
            std::copy(work.end() - static_cast<std::ptrdiff_t>(histSize), work.end(), firHistory_.begin());
        }
    }

    void Demodulator::applyFloatFilter(std::vector<float>& buffer) {
        if (firTaps_.empty() || buffer.empty()) {
            return;
        }

        std::vector<float> work(buffer.size() + firTaps_.size() - 1, 0.0f);
        std::copy(buffer.begin(), buffer.end(), work.begin() + static_cast<std::ptrdiff_t>(firTaps_.size() - 1));

        std::vector<float> out(buffer.size(), 0.0f);
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            float acc = 0.0f;
            for (std::size_t tap = 0; tap < firTaps_.size(); ++tap) {
                acc += work[i + tap] * firTaps_[tap];
            }
            out[i] = acc;
        }
        buffer.swap(out);
    }

    void Demodulator::angleToSymbol(std::uint8_t* bitsBuffer, const float* angles, int sourceLength) const {
        for (int i = 0; i < sourceLength; ++i) {
            const float angle = angles[i];
            *bitsBuffer++ = angle < 0.0f ? 1 : 0;
            *bitsBuffer++ = std::fabs(angle) > PI_HALF ? 1 : 0;
        }
    }

    float Demodulator::symbolToAngle(const std::uint8_t* trainingSequence, int symbolIndex) const {
        float value = trainingSequence[symbolIndex * 2 + 1] == 1 ? PI_THREE_QUARTER : PI_QUARTER;
        return trainingSequence[symbolIndex * 2] == 1 ? -value : value;
    }

    void Demodulator::updateAngles(const std::vector<dsp::complex_t>& filtered) {
        if (angleWindow_.empty()) {
            return;
        }

        const int count = static_cast<int>(filtered.size());
        if ((writeAddress_ + count) >= static_cast<int>(angleWindow_.size())) {
            const int overlap = std::min(writeAddress_, tailBufferLength_ * 4);
            if (overlap > 0) {
                std::memmove(
                    angleWindow_.data(),
                    angleWindow_.data() + (writeAddress_ - overlap),
                    static_cast<std::size_t>(overlap) * sizeof(float)
                );
            }
            writeAddress_ = overlap;
        }

        for (int i = 0; i < count; ++i) {
            dsp::complex_t delayed = (i < tailBufferLength_)
                ? symbolTail_[static_cast<std::size_t>(i)]
                : filtered[static_cast<std::size_t>(i - tailBufferLength_)];
            dsp::complex_t sample = filtered[static_cast<std::size_t>(i)];
            angleWindow_[static_cast<std::size_t>(writeAddress_++)] = (sample * delayed.conj()).fastPhase();
        }

        if (count >= tailBufferLength_) {
            std::copy(
                filtered.end() - tailBufferLength_,
                filtered.end(),
                symbolTail_.begin()
            );
        }
        else {
            std::move(symbolTail_.begin() + count, symbolTail_.end(), symbolTail_.begin());
            std::copy(filtered.begin(), filtered.end(), symbolTail_.end() - count);
        }
    }

    void Demodulator::sampleBurst(float* digitalBuffer, int offset) {
        int lastSample = 0;
        for (int index = 0; index < BURST_SYMBOL_COUNT; ++index) {
            lastSample = static_cast<int>(std::llround(index * symbolLength_));
            digitalBuffer[index] = angleWindow_[static_cast<std::size_t>(offset + lastSample)];
        }

        // The original SDR# demodulator advances the rolling buffer by the
        // sample position of symbol index 255, while only exporting 255
        // decoded symbols. Using the last emitted symbol index here leaves the
        // window behind by roughly one symbol and breaks MAC CRC over time.
        lastSample = static_cast<int>(std::llround(BURST_SYMBOL_COUNT * symbolLength_));
        offset += lastSample;
        offset -= tailBufferLength_ * 2;
        if (offset < 0) {
            offset = 0;
        }
        writeAddress_ -= offset;
        if (writeAddress_ < 0) {
            writeAddress_ = 0;
        }
        if (writeAddress_ > 0) {
            std::memmove(
                angleWindow_.data(),
                angleWindow_.data() + offset,
                static_cast<std::size_t>(writeAddress_) * sizeof(float)
            );
        }
    }

    std::vector<float> Demodulator::makeSincTaps(double sampleRate, double cutoff, int tapCount) const {
        std::vector<float> taps(static_cast<std::size_t>(tapCount), 0.0f);
        if (tapCount <= 0 || sampleRate <= 0.0) {
            return taps;
        }

        const int mid = tapCount / 2;
        double sum = 0.0;
        for (int i = 0; i < tapCount; ++i) {
            const int x = i - mid;
            double value = 0.0;
            if (x == 0) {
                value = 2.0 * cutoff / sampleRate;
            }
            else {
                const double phase = 2.0 * PI_VALUE * cutoff * static_cast<double>(x) / sampleRate;
                value = std::sin(phase) / (PI_VALUE * static_cast<double>(x));
            }

            const double window = 0.54 - 0.46 * std::cos((2.0 * PI_VALUE * i) / static_cast<double>(tapCount - 1));
            value *= window;
            taps[static_cast<std::size_t>(i)] = static_cast<float>(value);
            sum += value;
        }

        if (sum != 0.0) {
            for (auto& tap : taps) {
                tap = static_cast<float>(tap / sum);
            }
        }

        return taps;
    }
}
