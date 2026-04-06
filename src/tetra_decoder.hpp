#pragma once

#include "tetra_parser.hpp"
#include "tetra_phy.hpp"
#include "tetra_voice.hpp"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tetra {
    class TetraDecoder {
    public:
        using DataReadyHandler = std::function<void(const std::vector<ReceivedData>&)>;
        using SyncInfoReadyHandler = std::function<void(const ReceivedData&)>;

        TetraDecoder();

        void reset();
        void setIgnoreEncryptedSpeech(bool value) { globalContext_.ignoreEncryptedSpeech = value; }
        void setDataReadyHandler(DataReadyHandler handler) { dataReadyHandler_ = std::move(handler); }
        void setSyncInfoReadyHandler(SyncInfoReadyHandler handler) { syncInfoReadyHandler_ = std::move(handler); }

        int process(const Burst& burst, float* audioOut, int audioOutLength);

        float ber() const { return averageBer_; }
        float mer() const { return mer_; }
        bool burstReceived() const { return burstReceived_; }
        bool haveErrors() const { return haveErrors_; }
        Mode tetraMode() const { return tetraMode_; }
        int networkTimeTN() const { return networkTime_.timeSlot(); }
        int networkTimeFN() const { return networkTime_.frame(); }
        int networkTimeMN() const { return networkTime_.superFrame(); }
        bool voiceAvailable() const { return voiceDecoder_.available(); }
        const std::string& voiceError() const { return voiceDecoder_.error(); }
        const ReceivedData& lastSyncInfo() const { return syncInfo_; }
        const std::vector<ReceivedData>& neighbourList() const { return globalContext_.neighbourList; }

    private:
        void recreateState();
        void emitSyncInfo(const ReceivedData& syncInfo);
        void emitData(const std::vector<ReceivedData>& data);
        bool decodeAudio(float* audioBuffer, std::uint8_t* buf, int length, bool stolen, int channel, int audioOutLength);
        void shortToFloatPtr(const short* source, float* dest, int length) const;

        GlobalContext globalContext_;
        PhyLevel phyLevel_;
        std::unique_ptr<LowerMacLevel> lowerMac_;
        std::unique_ptr<MacLevel> parse_;
        VoiceDecoderLibrary voiceDecoder_;

        std::vector<std::uint8_t> bbBuffer_;
        std::vector<std::uint8_t> bkn1Buffer_;
        std::vector<std::uint8_t> bkn2Buffer_;
        std::vector<std::uint8_t> sb1Buffer_;

        LogicChannel logicChannel_{};
        NetworkTime networkTime_;
        ReceivedData syncInfo_;
        std::vector<ReceivedData> data_;

        int timeCounter_ = 0;
        float badBurstCounter_ = 0.0f;
        float averageBer_ = 0.0f;
        float mer_ = 0.0f;
        bool burstReceived_ = false;
        bool haveErrors_ = false;
        int fpass_ = 1;
        std::array<void*, 4> voiceChannels_{};
        std::array<short, 276> cdc_{};
        std::array<short, 480> sdc_{};
        DataReadyHandler dataReadyHandler_;
        SyncInfoReadyHandler syncInfoReadyHandler_;
        Mode tetraMode_ = Mode::TMO;
    };
}
