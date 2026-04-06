#pragma once

#include "generated_rules.hpp"
#include "tetra_core.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace tetra {
    class SdsParser {
    public:
        explicit SdsParser(GlobalContext& globalContext);
        int parseTLService(const LogicChannel& channelData, int offset, ReceivedData& result);
        void parseSDS(const LogicChannel& channelData, int offset, ReceivedData& result);

    private:
        void parseLocationInformationProtocol(const LogicChannel& channelData, int offset, ReceivedData& result);
        void parseTextMessage(const LogicChannel& channelData, int offset, ReceivedData& result);

        GlobalContext& globalContext_;
    };

    class MleLevel {
    public:
        explicit MleLevel(GlobalContext& globalContext);
        void parse(const LogicChannel& channelData, int offset, ReceivedData& result);

    private:
        void parseCMCEPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void parseMLEPDU(const LogicChannel& channelData, int offset, ReceivedData& result);

        GlobalContext& globalContext_;
        SdsParser sds_;
    };

    class LlcLevel {
    public:
        explicit LlcLevel(GlobalContext& globalContext);
        void parse(const LogicChannel& channelData, int offset, ReceivedData& result);

    private:
        bool calculateFCS(const LogicChannel& channelData, int offset) const;

        MleLevel mle_;
    };

    class MacLevel {
    public:
        explicit MacLevel(GlobalContext& globalContext);

        void accessAsignPDU(const LogicChannel& channelData);
        void resetAACH();
        void usignalPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void syncPDU(const LogicChannel& channelData, ReceivedData& result);
        void syncPDUHalfSlot(const LogicChannel& channelData, ReceivedData& result);
        int sysInfoPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void tmoParseMacPDU(const LogicChannel& channelData, std::vector<ReceivedData>& result);
        void resourcePDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void macEndPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void macFraqPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void dmoParseMacPDU(const LogicChannel& channelData, std::vector<ReceivedData>& result);
        void dmacDataPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void dmacEndPDU(const LogicChannel& channelData, int offset, ReceivedData& result);
        void dmacFraqPDU(const LogicChannel& channelData, int offset, ReceivedData& result);

        ChannelType downLinkChannelType = ChannelType::Common;
        ChannelType upLinkChannelType = ChannelType::Common;
        int field1 = 0;
        int field2 = 0;
        bool halfSlotStolen = false;

    private:
        int calcRealLength(const std::uint8_t* buffer, int offset, int currentLength) const;
        void createFraqmentsBuffer(const LogicChannel& buffer, int offset, int length, const ReceivedData* header);
        void addFraqmentsToBuffer(const LogicChannel& buffer, int offset, int length);
        bool fraqmentsBufferIsEmpty(int timeSlot) const;
        LogicChannel getDeFragmentedBuffer(const LogicChannel& channelData, ReceivedData& header);

        GlobalContext& globalContext_;
        LlcLevel llc_;
        std::array<std::vector<std::uint8_t>, 4> tempBuffers_;
        std::array<int, 4> writeAddress_{};
        std::array<ReceivedData, 4> fragmentsHeader_{};
    };
}
