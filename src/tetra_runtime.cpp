#include "tetra_runtime.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <signal_path/signal_path.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace tetra {
    namespace {
        constexpr float TWO_PI = 6.28318530717958647692f;
        constexpr float PI_DIV_FOUR = 0.78539816339744830962f;
        constexpr int CHANNEL_ACTIVE_MS = 500;
        constexpr int AUDIO_SOURCE_RATE = 8000;
        constexpr int AUDIO_OUTPUT_SAMPLES = 480;

        std::tm localTime(std::time_t value) {
            std::tm result{};
#if defined(_WIN32)
            localtime_s(&result, &value);
#else
            localtime_r(&value, &result);
#endif
            return result;
        }
    }

    class TetraRuntime::AudioOutput {
    public:
        explicit AudioOutput(const std::string& streamName);
        ~AudioOutput();

        void start();
        void stop();
        void enqueueMono(const float* input, int count);

    private:
        static void sampleRateChangedThunk(float sampleRate, void* ctx);
        void onSampleRateChanged(float sampleRate);
        void writerLoop();

        std::string streamName_;
        dsp::stream<dsp::stereo_t> sourceStream_;
        SinkManager::Stream sinkStream_;
        EventHandler<float> srHandler_{};
        std::mutex queueMtx_;
        std::condition_variable queueCv_;
        std::deque<dsp::stereo_t> stereoQueue_;
        std::thread writerThread_;
        std::atomic<bool> running_{ false };

        std::mutex resampleMtx_;
        double outputSampleRate_ = 48000.0;
        double resamplerOutRate_ = 48000.0;
        double resamplerPos_ = 0.0;
        std::vector<float> resamplerBuffer_;
        std::vector<dsp::stereo_t> resampleBuffer_;
    };

    class TetraRuntime::UdpSender {
    public:
        UdpSender();
        ~UdpSender();
        void send(const std::uint8_t* data, int length, int port);

    private:
        bool ensureSocket(int port);
        void closeSocket();

#if defined(_WIN32)
        using socket_t = SOCKET;
        static constexpr SOCKET invalidSocket() { return INVALID_SOCKET; }
        bool wsaStarted_ = false;
#else
        using socket_t = int;
        static constexpr int invalidSocket() { return -1; }
#endif

        socket_t socket_ = invalidSocket();
        sockaddr_in address_{};
        int currentPort_ = 0;
    };

    TetraRuntime::AudioOutput::AudioOutput(const std::string& streamName)
        : streamName_(streamName) {
        srHandler_.handler = &AudioOutput::sampleRateChangedThunk;
        srHandler_.ctx = this;
        sinkStream_.init(&sourceStream_, &srHandler_, 48000.0f);
        sigpath::sinkManager.registerStream(streamName_, &sinkStream_);
        resampleBuffer_.reserve(4096);
    }

    TetraRuntime::AudioOutput::~AudioOutput() {
        stop();
        sigpath::sinkManager.unregisterStream(streamName_);
    }

    void TetraRuntime::AudioOutput::start() {
        if (running_.load()) {
            return;
        }
        running_.store(true);
        sourceStream_.clearWriteStop();
        sigpath::sinkManager.startStream(streamName_);
        writerThread_ = std::thread(&AudioOutput::writerLoop, this);
    }

    void TetraRuntime::AudioOutput::stop() {
        if (!running_.load()) {
            return;
        }
        running_.store(false);
        queueCv_.notify_all();
        sourceStream_.stopWriter();
        if (writerThread_.joinable()) {
            writerThread_.join();
        }
        sourceStream_.clearWriteStop();
        sigpath::sinkManager.stopStream(streamName_);
        std::lock_guard<std::mutex> lock(queueMtx_);
        stereoQueue_.clear();
    }

    void TetraRuntime::AudioOutput::enqueueMono(const float* input, int count) {
        if (input == nullptr || count <= 0) {
            return;
        }

        std::lock_guard<std::mutex> resampleLock(resampleMtx_);
        const double outRate = outputSampleRate_;
        if (resamplerOutRate_ != outRate) {
            resamplerBuffer_.clear();
            resamplerPos_ = 0.0;
            resamplerOutRate_ = outRate;
        }

        resamplerBuffer_.insert(resamplerBuffer_.end(), input, input + count);
        resampleBuffer_.clear();

        const double step = static_cast<double>(AUDIO_SOURCE_RATE) / std::max<double>(outRate, 1.0);
        while ((resamplerPos_ + 1.0) < static_cast<double>(resamplerBuffer_.size())) {
            const auto index = static_cast<std::size_t>(resamplerPos_);
            const float frac = static_cast<float>(resamplerPos_ - static_cast<double>(index));
            const float a = resamplerBuffer_[index];
            const float b = resamplerBuffer_[index + 1];
            const float sample = a + ((b - a) * frac);
            resampleBuffer_.push_back(dsp::stereo_t{ sample, sample });
            resamplerPos_ += step;
        }

        const auto consumed = static_cast<std::size_t>(resamplerPos_);
        if (consumed > 0) {
            if (consumed < resamplerBuffer_.size()) {
                resamplerBuffer_.erase(resamplerBuffer_.begin(), resamplerBuffer_.begin() + static_cast<std::ptrdiff_t>(consumed));
            }
            else {
                resamplerBuffer_.clear();
            }
            resamplerPos_ -= static_cast<double>(consumed);
        }

        {
            std::lock_guard<std::mutex> queueLock(queueMtx_);
            const std::size_t maxQueue = static_cast<std::size_t>(std::max<double>(outputSampleRate_ * 2.0, 2048.0));
            if ((stereoQueue_.size() + resampleBuffer_.size()) > maxQueue) {
                const std::size_t toDrop = std::min(stereoQueue_.size(), (stereoQueue_.size() + resampleBuffer_.size()) - maxQueue);
                stereoQueue_.erase(stereoQueue_.begin(), stereoQueue_.begin() + static_cast<std::ptrdiff_t>(toDrop));
            }
            for (const auto& sample : resampleBuffer_) {
                stereoQueue_.push_back(sample);
            }
        }
        queueCv_.notify_one();
    }

    void TetraRuntime::AudioOutput::sampleRateChangedThunk(float sampleRate, void* ctx) {
        static_cast<AudioOutput*>(ctx)->onSampleRateChanged(sampleRate);
    }

    void TetraRuntime::AudioOutput::onSampleRateChanged(float sampleRate) {
        std::lock_guard<std::mutex> lock(resampleMtx_);
        outputSampleRate_ = std::max<double>(sampleRate, 12000.0);
    }

    void TetraRuntime::AudioOutput::writerLoop() {
        std::vector<dsp::stereo_t> block;

        while (running_.load()) {
            double outputSampleRate = 48000.0;
            {
                std::lock_guard<std::mutex> resampleLock(resampleMtx_);
                outputSampleRate = outputSampleRate_;
            }
            const int blockFrames = std::clamp(static_cast<int>(std::llround(outputSampleRate / 60.0)), 120, 4096);
            block.assign(static_cast<std::size_t>(blockFrames), dsp::stereo_t{ 0.0f, 0.0f });

            {
                std::unique_lock<std::mutex> lock(queueMtx_);
                queueCv_.wait_for(lock, std::chrono::milliseconds(20), [this, blockFrames] {
                    return !running_.load() || static_cast<int>(stereoQueue_.size()) >= blockFrames;
                });
                if (!running_.load()) {
                    break;
                }

                const int available = std::min(blockFrames, static_cast<int>(stereoQueue_.size()));
                for (int i = 0; i < available; ++i) {
                    block[static_cast<std::size_t>(i)] = stereoQueue_.front();
                    stereoQueue_.pop_front();
                }
            }

            std::memcpy(sourceStream_.writeBuf, block.data(), static_cast<std::size_t>(blockFrames) * sizeof(dsp::stereo_t));
            if (!sourceStream_.swap(blockFrames)) {
                break;
            }
        }
    }

    TetraRuntime::UdpSender::UdpSender() {
#if defined(_WIN32)
        WSADATA wsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            wsaStarted_ = true;
        }
#endif
    }

    TetraRuntime::UdpSender::~UdpSender() {
        closeSocket();
#if defined(_WIN32)
        if (wsaStarted_) {
            WSACleanup();
        }
#endif
    }

    void TetraRuntime::UdpSender::send(const std::uint8_t* data, int length, int port) {
        if (data == nullptr || length <= 0 || port <= 0) {
            return;
        }
        if (!ensureSocket(port)) {
            return;
        }
        ::sendto(socket_, reinterpret_cast<const char*>(data), length, 0, reinterpret_cast<const sockaddr*>(&address_), sizeof(address_));
    }

    bool TetraRuntime::UdpSender::ensureSocket(int port) {
        if (socket_ != invalidSocket() && port == currentPort_) {
            return true;
        }

        closeSocket();
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == invalidSocket()) {
            return false;
        }

        std::memset(&address_, 0, sizeof(address_));
        address_.sin_family = AF_INET;
        address_.sin_port = htons(static_cast<std::uint16_t>(port));
        inet_pton(AF_INET, "127.0.0.1", &address_.sin_addr);
        currentPort_ = port;
        return true;
    }

    void TetraRuntime::UdpSender::closeSocket() {
        if (socket_ == invalidSocket()) {
            return;
        }
#if defined(_WIN32)
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = invalidSocket();
        currentPort_ = 0;
    }

    TetraRuntime::TetraRuntime(std::string instanceName)
        : instanceName_(std::move(instanceName)),
          audioStreamName_(instanceName_ + "_audio"),
          audioOutput_(std::make_unique<AudioOutput>(audioStreamName_)),
          udpSender_(std::make_unique<UdpSender>()) {
        decoder_.setDataReadyHandler([this](const std::vector<ReceivedData>& data) {
            onDecoderDataReady(data);
        });
        decoder_.setSyncInfoReadyHandler([this](const ReceivedData& syncInfo) {
            onDecoderSyncInfoReady(syncInfo);
        });
        cmceData_.reserve(100);
        reset();
    }

    TetraRuntime::~TetraRuntime() {
        stop();
    }

    void TetraRuntime::setSettings(const TetraSettings& settings) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        settings_ = settings;
        if (settings_.autoPlay && selectedChannel_ == 0) {
            selectedChannel_ = 1;
        }
        decoder_.setIgnoreEncryptedSpeech(settings_.ignoreEncryptedSpeech);
    }

    TetraSettings TetraRuntime::settings() const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        return settings_;
    }

    void TetraRuntime::importKnownGroups(const std::vector<KnownGroup>& groups) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        networkBase_.clear();
        for (const auto& entry : groups) {
            networkBase_[entry.nmi].knowGroups[entry.gssi] = GroupsEntry{ entry.name, entry.priority };
        }
    }

    std::vector<KnownGroup> TetraRuntime::exportKnownGroups() const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        std::vector<KnownGroup> result;
        for (const auto& [nmi, network] : networkBase_) {
            for (const auto& [gssi, group] : network.knowGroups) {
                result.push_back(KnownGroup{ nmi, gssi, group.name, group.priority });
            }
        }
        return result;
    }

    void TetraRuntime::setGroupMetadata(int gssi, const std::string& name, int priority) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        if (currentCellNmi_ == 0) {
            return;
        }
        networkBase_[currentCellNmi_].knowGroups[gssi] = GroupsEntry{ name, priority };
    }

    void TetraRuntime::setSelectedChannel(int channel) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        selectedChannel_ = std::clamp(channel, 0, 4);
        currentChPriority_ = std::numeric_limits<int>::min();
        if (selectedChannel_ > 0) {
            currentChPriority_ = currentCellLoad_[static_cast<std::size_t>(selectedChannel_ - 1)].groupPriority;
        }
    }

    void TetraRuntime::setAutoPlay(bool enabled) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        settings_.autoPlay = enabled;
        if (enabled && selectedChannel_ == 0) {
            selectedChannel_ = 1;
        }
        currentChPriority_ = std::numeric_limits<int>::min();
    }

    void TetraRuntime::setTunedFrequency(long long frequency) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        tunedFrequency_ = frequency;
        if (tunedFrequency_ != 0) {
            currentCellCarrier_ = stateContext_.carrierCalc(tunedFrequency_);
        }
    }

    void TetraRuntime::start() {
        if (running_) {
            return;
        }
        reset();
        running_ = true;
        audioOutput_->start();
        workerThread_ = std::thread(&TetraRuntime::workerLoop, this);
    }

    void TetraRuntime::stop() {
        {
            std::lock_guard<std::mutex> iqLock(iqMtx_);
            if (!running_) {
                return;
            }
            running_ = false;
            iqQueue_.clear();
        }
        iqCv_.notify_all();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        audioOutput_->stop();
    }

    void TetraRuntime::reset() {
        std::lock_guard<std::mutex> stateLock(stateMtx_);
        std::lock_guard<std::mutex> iqLock(iqMtx_);
        iqQueue_.clear();
        iqSampleRate_ = 0.0;
        lostBuffers_ = 0;
        demodulator_.reset();
        decoder_.reset();
        decoder_.setIgnoreEncryptedSpeech(settings_.ignoreEncryptedSpeech);
        syncInfo_.clear();
        sysInfo_.clear();
        cmceData_.clear();
        neighbourData_.clear();
        recentEvents_.clear();
        burstAngles_.clear();
        currentCalls_.clear();
        currentCellLoad_ = {};
        currentCellNmi_ = 0;
        currentCellMnc_ = 0;
        currentCellMcc_ = 0;
        currentCellLa_ = 0;
        currentCellCc_ = 0;
        currentCellCarrier_ = 0;
        mainCellCarrier_ = 0;
        mainCellFrequency_ = 0;
        selectedChannel_ = settings_.autoPlay ? 1 : 0;
        currentChPriority_ = std::numeric_limits<int>::min();
        lastWatchdogSecond_ = -1;
        freqErrorHz_ = 0.0;
        pendingAfcCorrection_ = 0.0;
        prevAngle_ = 0.0f;
        afcCounter_ = 0;
        averageAngle_ = 0.0f;
        const auto epoch = std::chrono::steady_clock::time_point{};
        lastChannelActivity_.fill(epoch);
    }

    bool TetraRuntime::running() const {
        std::lock_guard<std::mutex> lock(iqMtx_);
        return running_.load();
    }

    void TetraRuntime::pushIq(const dsp::complex_t* data, int count, double sampleRate) {
        if (data == nullptr || count <= 0 || sampleRate <= 0.0) {
            return;
        }

        std::lock_guard<std::mutex> lock(iqMtx_);
        if (!running_) {
            return;
        }

        iqSampleRate_ = sampleRate;
        const int blockSize = requiredInputBlockSize(sampleRate);
        const std::size_t maxQueue = static_cast<std::size_t>(std::max(blockSize * 10, 2048));
        if ((iqQueue_.size() + static_cast<std::size_t>(count)) > maxQueue) {
            ++lostBuffers_;
            return;
        }

        for (int i = 0; i < count; ++i) {
            iqQueue_.push_back(data[i]);
        }
        iqCv_.notify_one();
    }

    void TetraRuntime::showAudioVolumeSlider(const std::string& prefix, float width) {
        sigpath::sinkManager.showVolumeSlider(audioStreamName_, prefix, width);
    }

    double TetraRuntime::consumePendingAfcCorrection() {
        std::lock_guard<std::mutex> lock(stateMtx_);
        const double correction = pendingAfcCorrection_;
        pendingAfcCorrection_ = 0.0;
        return correction;
    }

    std::string TetraRuntime::previewLogEntry() const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        return parseStringToEntriesLocked(settings_.logEntryRules, nullptr);
    }

    std::string TetraRuntime::previewLogPath() const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        return makeFileNameLocked(settings_.logWriteFolder, settings_.logFileNameRules, ".csv");
    }

    Snapshot TetraRuntime::snapshot() const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        const auto now = std::chrono::steady_clock::now();

        Snapshot snap;
        snap.running = running_.load();
        snap.burstReceived = decoder_.burstReceived();
        snap.haveErrors = decoder_.haveErrors();
        snap.voiceAvailable = decoder_.voiceAvailable();
        snap.voiceError = decoder_.voiceError();
        snap.ber = decoder_.ber();
        snap.mer = decoder_.mer();
        snap.frequencyErrorHz = freqErrorHz_;
        snap.lostBuffers = lostBuffers_;
        snap.tetraMode = decoder_.tetraMode();
        snap.currentCellNmi = currentCellNmi_;
        snap.currentCellMcc = currentCellMcc_;
        snap.currentCellMnc = currentCellMnc_;
        snap.currentCellLa = currentCellLa_;
        snap.currentCellCc = currentCellCc_;
        snap.currentCellCarrier = currentCellCarrier_;
        snap.mainCellCarrier = mainCellCarrier_;
        snap.mainCellFrequency = mainCellFrequency_;
        snap.tunedFrequency = tunedFrequency_;
        snap.selectedChannel = selectedChannel_;
        snap.autoPlay = settings_.autoPlay;
        snap.currentLoads = currentCellLoad_;
        snap.cmceData = cmceData_;
        snap.neighbourData = neighbourData_;
        snap.syncInfo = syncInfo_;
        snap.sysInfo = sysInfo_;
        snap.recentEvents = recentEvents_;
        snap.burstAngles = burstAngles_;

        for (std::size_t i = 0; i < lastChannelActivity_.size(); ++i) {
            snap.channelActive[i] = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChannelActivity_[i]).count() < CHANNEL_ACTIVE_MS);
        }

        for (const auto& [id, call] : currentCalls_) {
            snap.currentCalls.push_back(call);
        }

        if (currentCellNmi_ != 0) {
            const auto networkIt = networkBase_.find(currentCellNmi_);
            if (networkIt != networkBase_.end()) {
                for (const auto& [gssi, group] : networkIt->second.knowGroups) {
                    snap.activeGroups.push_back(KnownGroup{ currentCellNmi_, gssi, group.name, group.priority });
                }
            }
        }

        return snap;
    }

    void TetraRuntime::workerLoop() {
        std::vector<dsp::complex_t> block;
        std::array<float, AUDIO_OUTPUT_SAMPLES> audioBuffer{};

        while (true) {
            double sampleRate = 0.0;
            {
                std::unique_lock<std::mutex> lock(iqMtx_);
                iqCv_.wait_for(lock, std::chrono::milliseconds(250), [this] {
                    return !running_ || (iqSampleRate_ > 0.0 && iqQueue_.size() >= static_cast<std::size_t>(requiredInputBlockSize(iqSampleRate_)));
                });
                if (!running_) {
                    break;
                }

                sampleRate = iqSampleRate_;
                const int blockSize = requiredInputBlockSize(sampleRate);
                if (iqQueue_.size() < static_cast<std::size_t>(blockSize)) {
                    lock.unlock();
                    std::lock_guard<std::mutex> stateLock(stateMtx_);
                    maintainStateLocked();
                    continue;
                }

                block.resize(static_cast<std::size_t>(blockSize));
                for (int i = 0; i < blockSize; ++i) {
                    block[static_cast<std::size_t>(i)] = iqQueue_.front();
                    iqQueue_.pop_front();
                }
            }

            const auto& result = demodulator_.processBuffer(block.data(), sampleRate, static_cast<int>(block.size()), decoder_.tetraMode());
            if (result.burst.type == BurstType::WaitBurst) {
                std::lock_guard<std::mutex> stateLock(stateMtx_);
                maintainStateLocked();
                continue;
            }

            automaticFrequencyControl(result.angles, result.angleCount);

            TetraSettings settingsCopy;
            {
                std::lock_guard<std::mutex> lock(stateMtx_);
                settingsCopy = settings_;
            }
            if (settingsCopy.udpEnabled && result.burst.ptr != nullptr && result.burst.length > 0) {
                udpSender_->send(result.burst.ptr, result.burst.length, settingsCopy.udpPort);
            }

            const int audioChannel = decoder_.process(result.burst, audioBuffer.data(), static_cast<int>(audioBuffer.size()));
            if (audioChannel > 0) {
                markChannelActive(audioChannel);
                if (isChannelSelected(audioChannel)) {
                    audioOutput_->enqueueMono(audioBuffer.data(), AUDIO_OUTPUT_SAMPLES);
                }
            }

            std::lock_guard<std::mutex> stateLock(stateMtx_);
            if (result.angles != nullptr && result.angleCount > 0) {
                burstAngles_.assign(result.angles, result.angles + result.angleCount);
            }
            else {
                burstAngles_.clear();
            }
            neighbourData_ = decoder_.neighbourList();
            maintainStateLocked();
        }
    }

    int TetraRuntime::requiredInputBlockSize(double sampleRate) const {
        return std::max(255, static_cast<int>(std::llround(255.0 * sampleRate / 18000.0)));
    }

    void TetraRuntime::automaticFrequencyControl(const float* buffer, int length) {
        if (buffer == nullptr || length <= 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(stateMtx_);
        bool found = false;
        float resultAngle = 0.0f;

        for (int i = 0; i < length; ++i) {
            if (std::fabs(buffer[i] - prevAngle_) < PI_DIV_FOUR) {
                ++afcCounter_;
                averageAngle_ += buffer[i];
            }
            else {
                found = (afcCounter_ == 31) || (afcCounter_ == 32);
                if (afcCounter_ != 0) {
                    resultAngle = averageAngle_ / static_cast<float>(afcCounter_);
                }
                afcCounter_ = 0;
                averageAngle_ = 0.0f;
            }

            prevAngle_ = buffer[i];

            if (found) {
                freqErrorHz_ = (freqErrorHz_ * 0.9) + (0.1 * ((resultAngle - PI_DIV_FOUR) / (TWO_PI / 18000.0f)));
                break;
            }
        }

        if (!settings_.afcDisabled && (freqErrorHz_ > 200.0 || freqErrorHz_ < -200.0)) {
            pendingAfcCorrection_ += freqErrorHz_;
            freqErrorHz_ = 0.0;
        }
    }

    void TetraRuntime::onDecoderDataReady(const std::vector<ReceivedData>& data) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        for (const auto& entry : data) {
            updateSysInfoLocked(entry);
            updateCmceInfoLocked(entry);
            if (currentCellNmi_ != 0) {
                updateCallsInfoLocked(entry);
            }
        }
    }

    void TetraRuntime::onDecoderSyncInfoReady(const ReceivedData& syncInfo) {
        std::lock_guard<std::mutex> lock(stateMtx_);
        syncInfo_ = syncInfo;
        syncInfo_.tryGetValue(GlobalNames::MCC, currentCellMcc_);
        syncInfo_.tryGetValue(GlobalNames::MNC, currentCellMnc_);
        syncInfo_.tryGetValue(GlobalNames::ColorCode, currentCellCc_);
        currentCellNmi_ = (currentCellMcc_ << 14) | currentCellMnc_;
    }

    void TetraRuntime::updateSysInfoLocked(const ReceivedData& data) {
        if (!data.contains(GlobalNames::MAC_PDU_Type)) {
            return;
        }
        if (static_cast<MAC_PDU_Type>(data.value(GlobalNames::MAC_PDU_Type)) != MAC_PDU_Type::Broadcast) {
            return;
        }

        sysInfo_ = data;
        sysInfo_.tryGetValue(GlobalNames::Location_Area, currentCellLa_);

        int band = 0;
        int offset = 0;
        int carrier = 0;
        const bool isFull = data.tryGetValue(GlobalNames::Frequency_Band, band)
            && data.tryGetValue(GlobalNames::Main_Carrier, carrier)
            && data.tryGetValue(GlobalNames::Offset, offset);

        mainCellFrequency_ = stateContext_.frequencyCalc(isFull, carrier, band, offset);
        mainCellCarrier_ = carrier;
        if (tunedFrequency_ != 0) {
            currentCellCarrier_ = stateContext_.carrierCalc(tunedFrequency_);
        }
    }

    void TetraRuntime::updateCmceInfoLocked(const ReceivedData& data) {
        if (!data.contains(GlobalNames::CMCE_Primitives_Type)) {
            return;
        }
        if (cmceData_.size() >= 100) {
            cmceData_.erase(cmceData_.begin());
        }
        cmceData_.push_back(data);

        const std::string eventLine = formatCmceEventLocked(data);
        if (!eventLine.empty()) {
            if (recentEvents_.size() >= 200) {
                recentEvents_.erase(recentEvents_.begin());
            }
            recentEvents_.push_back(eventLine);
        }
    }

    void TetraRuntime::updateCallsInfoLocked(const ReceivedData& data) {
        if (!data.contains(GlobalNames::CMCE_Primitives_Type)) {
            return;
        }

        int callId = 0;
        if (!data.tryGetValue(GlobalNames::Call_identifier, callId)) {
            return;
        }

        int ssi = 0;
        int type = 0;
        int value = 0;
        bool logEvent = false;

        if (networkBase_.find(currentCellNmi_) == networkBase_.end()) {
            NetworkEntry entry;
            entry.knowGroups[0] = GroupsEntry{ "Individual", 0 };
            networkBase_[currentCellNmi_] = entry;
        }

        data.tryGetValue(GlobalNames::SSI, ssi);

        if (data.tryGetValue(GlobalNames::Basic_service_Communication_type, value)) {
            type = value;
            if (value == static_cast<int>(CommunicationType::Group)) {
                auto& groups = networkBase_[currentCellNmi_].knowGroups;
                if (groups.find(ssi) == groups.end()) {
                    groups[ssi] = GroupsEntry{ "", 0 };
                }
            }
        }

        if (currentCalls_.find(callId) == currentCalls_.end()) {
            CallsEntry entry;
            entry.to = ssi;
            entry.callID = callId;
            entry.from = 0;
            entry.type = type;
            entry.isClear = 0;
            entry.duplex = 0;
            currentCalls_[callId] = entry;
            logEvent = true;
        }

        auto& currentCall = currentCalls_[callId];
        currentCall.to = ssi;

        if (data.tryGetValue(GlobalNames::Basic_service_Communication_type, value)) {
            currentCall.type = value;
        }
        if (data.tryGetValue(GlobalNames::Carrier_number, value)) {
            currentCall.carrier = value;
        }

        currentCall.watchDog = 5;

        int transmissionGrant = 0;
        const int timeslot = data.value(GlobalNames::CurrTimeSlot);
        int assignedSlot = -1;
        int fromNew = -1;

        switch (static_cast<CmcePrimitivesType>(data.value(GlobalNames::CMCE_Primitives_Type))) {
        case CmcePrimitivesType::D_Disconnect:
        case CmcePrimitivesType::D_Release:
            assignedSlot = 0;
            fromNew = 0;
            currentCall.watchDog = 5;
            break;

        case CmcePrimitivesType::D_TX_Ceased:
            fromNew = 0;
            currentCall.watchDog = 5;
            break;

        case CmcePrimitivesType::D_Info:
            if (data.tryGetValue(GlobalNames::Slot_granting_element, value)) {
                if (data.tryGetValue(GlobalNames::SSI, value)) {
                    fromNew = value;
                }
                if (data.tryGetValue(GlobalNames::Timeslot_assigned, value)) {
                    assignedSlot = value;
                }
                else if (timeslot >= 1 && timeslot <= 4) {
                    assignedSlot = 0x10 >> timeslot;
                }
            }
            break;

        case CmcePrimitivesType::D_Connect:
        case CmcePrimitivesType::D_Setup:
        case CmcePrimitivesType::D_TX_Granted: {
            if (data.tryGetValue(GlobalNames::Calling_party_address_SSI, value)) {
                currentCall.from = value;
            }
            if (data.tryGetValue(GlobalNames::Transmitting_party_address_SSI, value)) {
                currentCall.from = value;
            }

            bool pduEncrypted = false;
            bool baseEncrypted = false;
            bool encrypt = false;
            if (data.tryGetValue(GlobalNames::Encryption_mode, value)) {
                pduEncrypted = value != 0;
            }
            if (data.tryGetValue(GlobalNames::Encryption_control, value)) {
                encrypt = value != 0;
            }
            if (data.tryGetValue(GlobalNames::Basic_service_Encryption_flag, value)) {
                baseEncrypted = value != 0;
            }
            currentCall.isClear = (!pduEncrypted && !baseEncrypted && !encrypt) ? 1 : 0;

            if (data.tryGetValue(GlobalNames::Simplex_duplex, value)) {
                currentCall.duplex = value;
            }
            if (data.tryGetValue(GlobalNames::Timeslot_assigned, value)) {
                assignedSlot = value;
            }
            else if (timeslot >= 1 && timeslot <= 4) {
                assignedSlot = 0x10 >> timeslot;
            }

            if (data.tryGetValue(GlobalNames::Transmission_grant, transmissionGrant)) {
                switch (static_cast<TransmissionGranted>(transmissionGrant)) {
                case TransmissionGranted::Granted:
                    if (data.tryGetValue(GlobalNames::SSI, value)) {
                        fromNew = value;
                    }
                    break;

                case TransmissionGranted::Granted_to_another_user:
                    if (data.tryGetValue(GlobalNames::Calling_party_address_SSI, value)) {
                        fromNew = value;
                    }
                    else if (data.tryGetValue(GlobalNames::Transmitting_party_address_SSI, value)) {
                        fromNew = value;
                    }
                    break;

                default:
                    fromNew = 0;
                    break;
                }
            }

            currentCall.watchDog = 20;
            break;
        }

        default:
            break;
        }

        if (assignedSlot != -1) {
            currentCall.assignedSlot = assignedSlot;
        }
        if (fromNew != -1 && currentCall.from != fromNew) {
            currentCall.from = fromNew;
            logEvent = true;
        }

        if (logEvent) {
            logTickLocked(currentCall);
        }
    }

    void TetraRuntime::maintainStateLocked() {
        syncInfo_.tryGetValue(GlobalNames::MCC, currentCellMcc_);
        syncInfo_.tryGetValue(GlobalNames::MNC, currentCellMnc_);
        syncInfo_.tryGetValue(GlobalNames::ColorCode, currentCellCc_);
        currentCellNmi_ = (currentCellMcc_ << 14) | currentCellMnc_;

        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        const auto tm = localTime(nowTime);
        if (lastWatchdogSecond_ != tm.tm_sec) {
            lastWatchdogSecond_ = tm.tm_sec;
            for (auto it = currentCalls_.begin(); it != currentCalls_.end();) {
                if (it->second.watchDog > 0) {
                    --it->second.watchDog;
                    ++it;
                }
                else {
                    it = currentCalls_.erase(it);
                }
            }
        }

        rebuildCurrentLoadsLocked();
        updateAutoPlayLocked();
    }

    void TetraRuntime::rebuildCurrentLoadsLocked() {
        currentCellLoad_ = {};

        for (const auto& [id, entry] : currentCalls_) {
            if (entry.carrier != currentCellCarrier_ && entry.carrier != 0) {
                continue;
            }

            int priority = 0;
            std::string name;
            const auto networkIt = networkBase_.find(currentCellNmi_);
            if (networkIt != networkBase_.end()) {
                const auto groupIt = networkIt->second.knowGroups.find(entry.to);
                if (groupIt != networkIt->second.knowGroups.end()) {
                    priority = groupIt->second.priority;
                    name = groupIt->second.name;
                }
            }

            if (entry.from == 0) {
                continue;
            }

            auto assignLoad = [&](std::size_t index) {
                currentCellLoad_[index].callId = entry.callID;
                currentCellLoad_[index].type = entry.type;
                currentCellLoad_[index].from = entry.from;
                currentCellLoad_[index].to = entry.to;
                currentCellLoad_[index].groupPriority = priority;
                currentCellLoad_[index].groupName = name.empty() ? std::to_string(entry.to) : name;
                currentCellLoad_[index].isClear = (entry.isClear == 1);
            };

            if ((entry.assignedSlot & 0x8) != 0) { assignLoad(0); }
            if ((entry.assignedSlot & 0x4) != 0) { assignLoad(1); }
            if ((entry.assignedSlot & 0x2) != 0) { assignLoad(2); }
            if ((entry.assignedSlot & 0x1) != 0) { assignLoad(3); }
        }
    }

    void TetraRuntime::updateAutoPlayLocked() {
        if (!settings_.autoPlay) {
            return;
        }

        int maxPriority = settings_.blockedLevel;
        int priorityChannel = 0;

        for (int i = 0; i < 4; ++i) {
            const auto& load = currentCellLoad_[static_cast<std::size_t>(i)];
            if (load.from == 0) {
                continue;
            }
            if (load.groupPriority < maxPriority || load.groupPriority <= currentChPriority_) {
                continue;
            }
            if (!load.isClear && settings_.ignoreEncryptedSpeech) {
                continue;
            }

            maxPriority = load.groupPriority;
            priorityChannel = i + 1;
        }

        if (priorityChannel != 0) {
            if (selectedChannel_ != priorityChannel) {
                selectedChannel_ = priorityChannel;
                currentChPriority_ = currentCellLoad_[static_cast<std::size_t>(priorityChannel - 1)].groupPriority;
            }
            return;
        }

        if (selectedChannel_ > 0) {
            const auto& currentLoad = currentCellLoad_[static_cast<std::size_t>(selectedChannel_ - 1)];
            if (currentLoad.from == 0) {
                selectedChannel_ = 0;
                currentChPriority_ = std::numeric_limits<int>::min();
            }
        }
    }

    void TetraRuntime::logTickLocked(const CallsEntry& currentCall) {
        if (!settings_.logEnabled || settings_.logWriteFolder.empty()) {
            return;
        }

        const std::string line = parseStringToEntriesLocked(settings_.logEntryRules, &currentCall);
        const std::string path = makeFileNameLocked(settings_.logWriteFolder, settings_.logFileNameRules, ".csv");

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream output(path, std::ios::app | std::ios::binary);
        if (!output) {
            return;
        }
        output << line << '\n';
    }

    std::string TetraRuntime::formatCmceEventLocked(const ReceivedData& data) const {
        if (!data.contains(GlobalNames::CMCE_Primitives_Type)) {
            return {};
        }

        auto appendToken = [](std::string& line, const std::string& token) {
            if (token.empty()) {
                return;
            }
            if (!line.empty()) {
                line.push_back(' ');
            }
            line += token;
        };

        int value = 0;
        std::string line;
        const auto communicationTypeName = [](int type) -> std::string {
            switch (static_cast<CommunicationType>(type)) {
            case CommunicationType::Indiv: return "Indiv";
            case CommunicationType::Group: return "Group";
            case CommunicationType::P2MP: return "P2MP";
            case CommunicationType::Broadcast: return "Broadcast";
            default: return std::to_string(type);
            }
        };
        const auto transmissionGrantedName = [](int type) -> std::string {
            switch (static_cast<TransmissionGranted>(type)) {
            case TransmissionGranted::Granted: return "Granted";
            case TransmissionGranted::Not_granted: return "NotGranted";
            case TransmissionGranted::Request_queued: return "Queued";
            case TransmissionGranted::Granted_to_another_user: return "GrantedOther";
            default: return std::to_string(type);
            }
        };
        const auto circuitModeName = [](int type) -> std::string {
            switch (static_cast<CircuitModeType>(type)) {
            case CircuitModeType::Speech_TCH_S: return "Speech";
            case CircuitModeType::Unprotect_72: return "Unprotect72";
            case CircuitModeType::Low_Protect_48_1: return "LowProtect48_1";
            case CircuitModeType::Low_Protect_48_4: return "LowProtect48_4";
            case CircuitModeType::Low_Protect_48_8: return "LowProtect48_8";
            case CircuitModeType::High_Protect_24_1: return "HighProtect24_1";
            case CircuitModeType::High_Protect_24_4: return "HighProtect24_4";
            case CircuitModeType::High_Protect_24_8: return "HighProtect24_8";
            default: return std::to_string(type);
            }
        };
        const auto cmceTypeName = [](int type) -> std::string {
            switch (static_cast<CmcePrimitivesType>(type)) {
            case CmcePrimitivesType::D_Alert: return "D_Alert";
            case CmcePrimitivesType::D_Call_Proceeding: return "D_Call_Proceeding";
            case CmcePrimitivesType::D_Connect: return "D_Connect";
            case CmcePrimitivesType::D_Disconnect: return "D_Disconnect";
            case CmcePrimitivesType::D_Info: return "D_Info";
            case CmcePrimitivesType::D_Release: return "D_Release";
            case CmcePrimitivesType::D_Setup: return "D_Setup";
            case CmcePrimitivesType::D_TX_Ceased: return "D_TX_Ceased";
            case CmcePrimitivesType::D_TX_Granted: return "D_TX_Granted";
            case CmcePrimitivesType::D_SDS_Data: return "D_SDS_Data";
            default: return std::to_string(type);
            }
        };

        if (data.tryGetValue(GlobalNames::Encryption_mode, value) && value != 0) {
            appendToken(line, "PDU encrypted:" + std::to_string(value));
            appendToken(line, "Data incorrect!");
        }
        if (data.tryGetValue(GlobalNames::Carrier_number, value)) {
            appendToken(line, "Carrier:" + std::to_string(value));
        }
        if (data.tryGetValue(GlobalNames::Timeslot_assigned, value)) {
            if (value != 0) {
                std::string slotText;
                if ((value & 0x8) != 0) { slotText += '1'; }
                if ((value & 0x4) != 0) { slotText += '2'; }
                if ((value & 0x2) != 0) { slotText += '3'; }
                if ((value & 0x1) != 0) { slotText += '4'; }
                appendToken(line, "TimeSlot:" + slotText);
            }
            else {
                appendToken(line, "TimeSlot:0");
            }
        }
        if (data.tryGetValue(GlobalNames::SSI, value)) {
            appendToken(line, "SSI:" + std::to_string(value));
        }
        if (data.tryGetValue(GlobalNames::Call_identifier, value)) {
            appendToken(line, "CallID:" + std::to_string(value));
        }
        if (data.tryGetValue(GlobalNames::Encryption_control, value)) {
            appendToken(line, std::string("Encrypt:") + (value == 0 ? "Clear" : "E2EE"));
        }
        if (data.tryGetValue(GlobalNames::CMCE_Primitives_Type, value)) {
            appendToken(line, cmceTypeName(value));
        }
        if (data.tryGetValue(GlobalNames::Transmission_grant, value)) {
            appendToken(line, "Transmission:" + transmissionGrantedName(value));
        }
        if (data.tryGetValue(GlobalNames::Calling_party_address_SSI, value)) {
            appendToken(line, "PartySSI:" + std::to_string(value));
        }
        if (data.tryGetValue(GlobalNames::Transmitting_party_address_SSI, value)) {
            appendToken(line, "PartySSI:" + std::to_string(value));
        }
        if (data.tryGetValue(GlobalNames::Basic_service_Communication_type, value)) {
            appendToken(line, "BasicService:" + communicationTypeName(value));
        }
        if (data.tryGetValue(GlobalNames::Basic_service_Encryption_flag, value)) {
            appendToken(line, value == 0 ? "Clear" : "E2EE");
        }
        if (data.tryGetValue(GlobalNames::Basic_service_Circuit_mode_type, value)) {
            appendToken(line, "Circuit:" + circuitModeName(value));
        }

        int shortDataType = 0;
        if (data.tryGetValue(GlobalNames::Short_data_type_identifier, shortDataType)) {
            appendToken(line, "Type:" + std::to_string(shortDataType));

            switch (shortDataType) {
            case 0:
                if (data.tryGetValue(GlobalNames::User_Defined_Data_16, value)) {
                    appendToken(line, "Length:16");
                    appendToken(line, "Data:" + std::to_string(value));
                }
                break;

            case 1:
                if (data.tryGetValue(GlobalNames::User_Defined_Data_32, value)) {
                    appendToken(line, "Length:32");
                    appendToken(line, "Data:" + std::to_string(static_cast<std::uint32_t>(value)));
                }
                break;

            case 2: {
                std::uint64_t data64 = 0;
                int high = 0;
                int low = 0;
                if (data.tryGetValue(GlobalNames::User_Defined_Data_64_1, high)) {
                    data64 |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(high)) << 32;
                }
                if (data.tryGetValue(GlobalNames::User_Defined_Data_64_2, low)) {
                    data64 |= static_cast<std::uint64_t>(static_cast<std::uint32_t>(low));
                }
                if (data.contains(GlobalNames::User_Defined_Data_64_1) || data.contains(GlobalNames::User_Defined_Data_64_2)) {
                    appendToken(line, "Length:64");
                    appendToken(line, "Data:" + std::to_string(data64));
                }
                break;
            }

            default:
                break;
            }

            if (data.tryGetValue(GlobalNames::Protocol_identifier, value)) {
                appendToken(line, "Protocol:" + std::to_string(value));
            }
            if (data.tryGetValue(GlobalNames::Location_PDU_type_extension, value)) {
                appendToken(line, "SubType:" + std::to_string(value));
            }
            if (data.tryGetValue(GlobalNames::Latitude, value)) {
                double latitude = static_cast<double>(value) * 1.07288360595703E-05;
                if (latitude >= 90.0) {
                    latitude -= 180.0;
                }
                std::ostringstream stream;
                stream << std::fixed << std::setprecision(6) << "Lat:" << latitude << "deg";
                appendToken(line, stream.str());
            }
            if (data.tryGetValue(GlobalNames::Longitude, value)) {
                double longitude = static_cast<double>(value) * 1.07288360595703E-05;
                if (longitude >= 180.0) {
                    longitude -= 360.0;
                }
                std::ostringstream stream;
                stream << std::fixed << std::setprecision(6) << "Long:" << longitude << "deg";
                appendToken(line, stream.str());
            }
            if (data.tryGetValue(GlobalNames::Position_error, value)) {
                appendToken(line, "Accuracy:" + std::to_string(value * 20) + "m");
            }
            if (data.tryGetValue(GlobalNames::Horizontal_velocity, value)) {
                const double velocity = (value <= 28) ? static_cast<double>(value) : 16.0 * std::pow(1.038, static_cast<double>(value - 13));
                std::ostringstream stream;
                stream << std::fixed << std::setprecision(1) << "Velocity:" << velocity << "km/h";
                appendToken(line, stream.str());
            }
            if (data.tryGetValue(GlobalNames::Direction_of_travel, value)) {
                std::ostringstream stream;
                stream << "Dir:" << (static_cast<double>(value) * 22.5) << "deg";
                appendToken(line, stream.str());
            }
        }

        return line;
    }

    std::string TetraRuntime::parseStringToEntriesLocked(const std::string& entryString, const CallsEntry* call) const {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        const auto tm = localTime(nowTime);

        char dateBuffer[64];
        char timeBuffer[64];
        std::strftime(dateBuffer, sizeof(dateBuffer), "%x", &tm);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &tm);

        const std::string dateString = dateBuffer;
        const std::string timeString = timeBuffer;
        const std::string mccString = std::to_string(currentCellMcc_);
        const std::string mncString = std::to_string(currentCellMnc_);
        const std::string laString = std::to_string(currentCellLa_);
        const std::string ccString = std::to_string(currentCellCc_);
        const std::string typeString = call ? std::to_string(call->type) : "unknown";
        const std::string fromString = call ? std::to_string(call->from) : "unknown";
        const std::string toString = call ? std::to_string(call->to) : "unknown";
        const std::string encryptString = call ? (call->isClear == 1 ? "Clear" : "Encrypted") : "unknown";
        const std::string duplexString = call ? (call->duplex == 0 ? "Simplex" : "Duplex") : "unknown";
        const std::string tsString = call ? std::to_string(call->assignedSlot) : "unknown";
        const std::string idString = call ? std::to_string(call->callID) : "unknown";
        const std::string carrierString = call ? std::to_string(call->carrier) : "unknown";

        std::string entry;
        std::size_t index = 0;
        while (index < entryString.size()) {
            if (compareString(entryString, "date", index)) {
                index += 4;
                entry += dateString + settings_.logSeparator;
            }
            else if (compareString(entryString, "time", index)) {
                index += 4;
                entry += timeString + settings_.logSeparator;
            }
            else if (compareString(entryString, "carrier", index)) {
                index += 7;
                entry += carrierString + settings_.logSeparator;
            }
            else if (compareString(entryString, "mnc", index)) {
                index += 3;
                entry += mncString + settings_.logSeparator;
            }
            else if (compareString(entryString, "mcc", index)) {
                index += 3;
                entry += mccString + settings_.logSeparator;
            }
            else if (compareString(entryString, "la", index)) {
                index += 2;
                entry += laString + settings_.logSeparator;
            }
            else if (compareString(entryString, "cc", index)) {
                index += 2;
                entry += ccString + settings_.logSeparator;
            }
            else if (compareString(entryString, "type", index)) {
                index += 4;
                entry += typeString + settings_.logSeparator;
            }
            else if (compareString(entryString, "from", index)) {
                index += 4;
                entry += fromString + settings_.logSeparator;
            }
            else if (compareString(entryString, "to", index)) {
                index += 2;
                entry += toString + settings_.logSeparator;
            }
            else if (compareString(entryString, "encryption", index)) {
                index += 10;
                entry += encryptString + settings_.logSeparator;
            }
            else if (compareString(entryString, "duplex", index)) {
                index += 6;
                entry += duplexString + settings_.logSeparator;
            }
            else if (compareString(entryString, "slot", index)) {
                index += 4;
                entry += tsString + settings_.logSeparator;
            }
            else if (compareString(entryString, "callid", index)) {
                index += 6;
                entry += idString + settings_.logSeparator;
            }
            else if (entryString[index] == '+' || entryString[index] == ' ') {
                ++index;
            }
            else if (entryString[index] == '"') {
                ++index;
                const auto end = entryString.find('"', index);
                if (end == std::string::npos) {
                    return entry + "-error!";
                }
                entry += entryString.substr(index, end - index);
                entry += settings_.logSeparator;
                index = end + 1;
            }
            else {
                return entry + "-error!";
            }
        }

        return entry;
    }

    std::string TetraRuntime::makeFileNameLocked(const std::string& folder, const std::string& nameRules, const std::string& fileExtension) const {
        std::string filename = folder;
        filename += parseStringToPathLocked(nameRules, fileExtension);
        return filename;
    }

    std::string TetraRuntime::parseStringToPathLocked(const std::string& nameString, const std::string& extension) const {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        const auto tm = localTime(nowTime);

        char dateBuffer[32];
        char timeBuffer[32];
        std::strftime(dateBuffer, sizeof(dateBuffer), "%Y_%m_%d", &tm);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H-%M-%S", &tm);

        const std::string currentFrequencyString = getFrequencyDisplay(tunedFrequency_);
        const std::string currentDateString = dateBuffer;
        const std::string currentStartTimeString = timeBuffer;
        const std::string mccString = std::to_string(currentCellMcc_);
        const std::string mncString = std::to_string(currentCellMnc_);
        const std::string laString = std::to_string(currentCellLa_);
        const std::string ccString = std::to_string(currentCellCc_);

        std::string filename;
        std::size_t index = 0;
        while (index < nameString.size()) {
            if (compareString(nameString, "date", index)) {
                index += 4;
                filename += currentDateString;
            }
            else if (compareString(nameString, "time", index)) {
                index += 4;
                filename += currentStartTimeString;
            }
            else if (compareString(nameString, "frequency", index)) {
                index += 9;
                filename += currentFrequencyString;
            }
            else if (compareString(nameString, "mcc", index)) {
                index += 3;
                filename += mccString;
            }
            else if (compareString(nameString, "mnc", index)) {
                index += 3;
                filename += mncString;
            }
            else if (compareString(nameString, "cc", index)) {
                index += 2;
                filename += ccString;
            }
            else if (compareString(nameString, "la", index)) {
                index += 2;
                filename += laString;
            }
            else if (compareString(nameString, "to", index)) {
                index += 2;
            }
            else if (nameString[index] == '\\' || nameString[index] == '/') {
                ++index;
                filename += "\\";
            }
            else if (nameString[index] == '+' || nameString[index] == ' ') {
                ++index;
            }
            else if (nameString[index] == '"') {
                ++index;
                const auto end = nameString.find('"', index);
                if (end == std::string::npos) {
                    return filename + "-error!";
                }
                for (std::size_t i = index; i < end; ++i) {
                    const char ch = nameString[i];
                    if (ch == '?' || ch == '/' || ch == '\\' || ch == ':' || ch == '<' || ch == '>' || ch == '|' || ch == '*' || ch == '"') {
                        continue;
                    }
                    filename.push_back(ch);
                }
                index = end + 1;
            }
            else {
                return filename + "-error!";
            }
        }

        if (!filename.empty()) {
            if (filename.size() < extension.size() || filename.substr(filename.size() - extension.size()) != extension) {
                filename += extension;
            }
            if (filename.front() != '\\') {
                filename.insert(filename.begin(), '\\');
            }
        }

        return filename;
    }

    std::string TetraRuntime::getFrequencyDisplay(long long frequency) {
        std::ostringstream stream;
        const auto absFrequency = std::llabs(frequency);
        if (absFrequency == 0) {
            return "DC";
        }
        if (absFrequency > 1500000000LL) {
            stream << std::fixed << std::setprecision(6) << (static_cast<double>(frequency) / 1000000000.0) << " GHz";
        }
        else if (absFrequency > 30000000LL) {
            stream << std::fixed << std::setprecision(6) << (static_cast<double>(frequency) / 1000000.0) << " MHz";
        }
        else if (absFrequency > 1000LL) {
            stream << std::fixed << std::setprecision(3) << (static_cast<double>(frequency) / 1000.0) << " kHz";
        }
        else {
            stream << frequency;
        }
        return stream.str();
    }

    bool TetraRuntime::compareString(const std::string& source, const std::string& compare, std::size_t index) {
        return (index + compare.size()) <= source.size() && source.compare(index, compare.size(), compare) == 0;
    }

    void TetraRuntime::markChannelActive(int channel) {
        if (channel < 1 || channel > 4) {
            return;
        }
        std::lock_guard<std::mutex> lock(stateMtx_);
        lastChannelActivity_[static_cast<std::size_t>(channel - 1)] = std::chrono::steady_clock::now();
    }

    bool TetraRuntime::isChannelSelected(int channel) const {
        std::lock_guard<std::mutex> lock(stateMtx_);
        return selectedChannel_ == channel;
    }
}
