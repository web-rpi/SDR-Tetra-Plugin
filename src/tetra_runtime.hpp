#pragma once

#include "tetra_decoder.hpp"
#include "tetra_demod.hpp"
#include <dsp/stream.h>
#include <dsp/types.h>
#include <signal_path/sink.h>
#include <utils/event.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tetra {
    struct TetraSettings {
        bool autoPlay = true;
        bool ignoreEncryptedSpeech = false;
        bool udpEnabled = false;
        int udpPort = 20025;
        bool afcDisabled = false;
        int blockedLevel = 0;
        bool logEnabled = false;
        std::string logWriteFolder;
        std::string logEntryRules = "date + time + mcc + mnc + la + cc + carrier + slot + callid + type + from + to + encryption + duplex";
        std::string logFileNameRules = "date \\ frequency \\ mcc \"_\" mnc \"_\" la";
        std::string logSeparator = " ; ";
    };

    struct KnownGroup {
        int nmi = 0;
        int gssi = 0;
        std::string name;
        int priority = 0;
    };

    struct Snapshot {
        bool running = false;
        bool burstReceived = false;
        bool haveErrors = false;
        bool voiceAvailable = false;
        std::string voiceError;
        float ber = 0.0f;
        float mer = 0.0f;
        double frequencyErrorHz = 0.0;
        int lostBuffers = 0;
        Mode tetraMode = Mode::TMO;
        int currentCellNmi = 0;
        int currentCellMcc = 0;
        int currentCellMnc = 0;
        int currentCellLa = 0;
        int currentCellCc = 0;
        int currentCellCarrier = 0;
        int mainCellCarrier = 0;
        long long mainCellFrequency = 0;
        long long tunedFrequency = 0;
        int selectedChannel = 0;
        bool autoPlay = true;
        std::array<bool, 4> channelActive{};
        std::array<CurrentLoad, 4> currentLoads{};
        std::vector<CallsEntry> currentCalls;
        std::vector<KnownGroup> activeGroups;
        std::vector<ReceivedData> cmceData;
        std::vector<ReceivedData> neighbourData;
        ReceivedData syncInfo;
        ReceivedData sysInfo;
        std::vector<std::string> recentEvents;
        std::vector<float> burstAngles;
    };

    class TetraRuntime {
    public:
        explicit TetraRuntime(std::string instanceName);
        ~TetraRuntime();

        void setSettings(const TetraSettings& settings);
        TetraSettings settings() const;

        void importKnownGroups(const std::vector<KnownGroup>& groups);
        std::vector<KnownGroup> exportKnownGroups() const;
        void setGroupMetadata(int gssi, const std::string& name, int priority);

        void setSelectedChannel(int channel);
        void setAutoPlay(bool enabled);

        void setTunedFrequency(long long frequency);
        void start();
        void stop();
        void reset();
        bool running() const;

        void pushIq(const dsp::complex_t* data, int count, double sampleRate);
        Snapshot snapshot() const;
        double consumePendingAfcCorrection();
        std::string previewLogEntry() const;
        std::string previewLogPath() const;

        const std::string& audioStreamName() const { return audioStreamName_; }
        void showAudioVolumeSlider(const std::string& prefix, float width);

    private:
        class AudioOutput;
        class UdpSender;

        void workerLoop();
        int requiredInputBlockSize(double sampleRate) const;
        void automaticFrequencyControl(const float* buffer, int length);
        void onDecoderDataReady(const std::vector<ReceivedData>& data);
        void onDecoderSyncInfoReady(const ReceivedData& syncInfo);
        void updateSysInfoLocked(const ReceivedData& data);
        void updateCmceInfoLocked(const ReceivedData& data);
        void updateCallsInfoLocked(const ReceivedData& data);
        void maintainStateLocked();
        void rebuildCurrentLoadsLocked();
        void updateAutoPlayLocked();
        void logTickLocked(const CallsEntry& currentCall);
        std::string formatCmceEventLocked(const ReceivedData& data) const;
        std::string parseStringToEntriesLocked(const std::string& entryString, const CallsEntry* call) const;
        std::string makeFileNameLocked(const std::string& folder, const std::string& nameRules, const std::string& fileExtension) const;
        std::string parseStringToPathLocked(const std::string& nameString, const std::string& extension) const;
        static std::string getFrequencyDisplay(long long frequency);
        static bool compareString(const std::string& source, const std::string& compare, std::size_t index);
        void markChannelActive(int channel);
        bool isChannelSelected(int channel) const;

        std::string instanceName_;
        std::string audioStreamName_;

        mutable std::mutex stateMtx_;
        mutable std::mutex iqMtx_;
        std::condition_variable iqCv_;
        std::deque<dsp::complex_t> iqQueue_;
        double iqSampleRate_ = 0.0;
        std::atomic<bool> running_{ false };
        std::atomic<int> lostBuffers_{ 0 };
        std::thread workerThread_;

        Demodulator demodulator_;
        TetraDecoder decoder_;
        std::unique_ptr<AudioOutput> audioOutput_;
        std::unique_ptr<UdpSender> udpSender_;

        TetraSettings settings_;
        GlobalContext stateContext_;
        ReceivedData syncInfo_;
        ReceivedData sysInfo_;
        std::vector<ReceivedData> cmceData_;
        std::vector<ReceivedData> neighbourData_;
        std::vector<std::string> recentEvents_;
        std::vector<float> burstAngles_;
        std::map<int, CallsEntry> currentCalls_;
        std::map<int, NetworkEntry> networkBase_;
        std::array<CurrentLoad, 4> currentCellLoad_{};
        std::array<std::chrono::steady_clock::time_point, 4> lastChannelActivity_{};
        long long tunedFrequency_ = 0;
        int currentCellNmi_ = 0;
        int currentCellMnc_ = 0;
        int currentCellMcc_ = 0;
        int currentCellLa_ = 0;
        int currentCellCc_ = 0;
        int currentCellCarrier_ = 0;
        int mainCellCarrier_ = 0;
        long long mainCellFrequency_ = 0;
        int selectedChannel_ = 1;
        int currentChPriority_ = std::numeric_limits<int>::min();
        int lastWatchdogSecond_ = -1;
        double freqErrorHz_ = 0.0;
        double pendingAfcCorrection_ = 0.0;
        float prevAngle_ = 0.0f;
        int afcCounter_ = 0;
        float averageAngle_ = 0.0f;
    };
}
