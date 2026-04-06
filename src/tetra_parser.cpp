#include "tetra_parser.hpp"
#include <algorithm>
#include <cstring>
#include <string>

namespace tetra {
    namespace {
        constexpr std::uint32_t LLC_POLY = 0xedb88320U;
        constexpr std::uint32_t LLC_GOOD_FCS = 0xdebb20e3U;
    }

    SdsParser::SdsParser(GlobalContext& globalContext)
        : globalContext_(globalContext) {}

    int SdsParser::parseTLService(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const int messageType = utils::bitsToInt32(channelData.ptr, offset, 4);
        offset += 4;
        result.add(GlobalNames::SDS_TL_MesaggeType, messageType);

        switch (messageType) {
        case 0:
            offset = globalContext_.parseParams(channelData, offset, rules::d_Sds_TL_ForwardRules, result);
            break;
        case 1:
            offset = globalContext_.parseParams(channelData, offset, rules::d_Sds_TL_ReportRules, result);
            break;
        default:
            break;
        }

        return offset;
    }

    void SdsParser::parseSDS(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const auto type = static_cast<SdsProtocolIdent>(utils::bitsToInt32(channelData.ptr, offset, 8));
        offset += 8;
        result.add(GlobalNames::Protocol_identifier, static_cast<int>(type));

        if (static_cast<int>(type) > 127) {
            offset = parseTLService(channelData, offset, result);
        }

        switch (type) {
        case SdsProtocolIdent::Simple_immediate_text:
        case SdsProtocolIdent::Simple_text_msg:
        case SdsProtocolIdent::Immediate_text_messaging_TL:
        case SdsProtocolIdent::Text_Messaging_TL: {
            offset = globalContext_.parseParams(channelData, offset, rules::sds_SimpleTextRules, result);
            parseTextMessage(channelData, offset, result);
            break;
        }
        case SdsProtocolIdent::Location_System_TL:
        case SdsProtocolIdent::Simple_location_system: {
            const int locationCodingScheme = utils::bitsToInt32(channelData.ptr, offset, 8);
            offset += 8;
            result.add(GlobalNames::Location_System_Coding, locationCodingScheme);
            if (locationCodingScheme == 0) {
                result.add(GlobalNames::Text_coding_scheme, 1);
                parseTextMessage(channelData, offset, result);
            }
            else {
                result.add(GlobalNames::UnknowData, 1);
            }
            break;
        }
        case SdsProtocolIdent::Location_information_protokol:
            parseLocationInformationProtocol(channelData, offset, result);
            break;
        default: {
            offset = globalContext_.parseParams(channelData, offset, rules::sds_SimpleTextRules, result);
            parseTextMessage(channelData, offset, result);
            result.add(GlobalNames::UnknowData, 1);
            break;
        }
        }
    }

    void SdsParser::parseLocationInformationProtocol(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const int pduType = utils::bitsToInt32(channelData.ptr, offset, 2);
        offset += 2;
        result.add(GlobalNames::Location_PDU_type, pduType);

        switch (pduType) {
        case 0:
            globalContext_.parseParams(channelData, offset, rules::sds_LocationShortRules, result);
            break;
        case 1: {
            const auto pduSubType = static_cast<LocationTypeExtension>(utils::bitsToInt32(channelData.ptr, offset, 4));
            offset += 4;
            result.add(GlobalNames::Location_PDU_type_extension, static_cast<int>(pduSubType));
            switch (pduSubType) {
            case LocationTypeExtension::Immediate_location_report:
                globalContext_.parseParams(channelData, offset, rules::sds_ImmediateLocRepRules, result);
                break;
            default:
                result.add(GlobalNames::UnknowData, 1);
                break;
            }
            break;
        }
        default:
            break;
        }
    }

    void SdsParser::parseTextMessage(const LogicChannel& channelData, int offset, ReceivedData& result) {
        if (result.contains(GlobalNames::OutOfBuffer)) {
            return;
        }

        const int messageLength = (channelData.length - offset) / 8;
        if (messageLength <= 0) {
            return;
        }

        std::string message;
        message.reserve(static_cast<std::size_t>(messageLength));
        for (int i = 0; i < messageLength; ++i) {
            const auto symbol = utils::bitsToByte(channelData.ptr, offset, 8);
            offset += 8;
            message.push_back(static_cast<char>(symbol));
        }
    }

    MleLevel::MleLevel(GlobalContext& globalContext)
        : globalContext_(globalContext), sds_(globalContext) {}

    void MleLevel::parse(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const auto protocol = static_cast<MLEPduType>(utils::bitsToInt32(channelData.ptr, offset, 3));
        offset += 3;
        result.add(GlobalNames::MLE_PDU_Type, static_cast<int>(protocol));

        switch (protocol) {
        case MLEPduType::CMCE:
            parseCMCEPDU(channelData, offset, result);
            break;
        case MLEPduType::MLE:
            parseMLEPDU(channelData, offset, result);
            break;
        default:
            result.add(GlobalNames::UnknowData, 1);
            break;
        }
    }

    void MleLevel::parseCMCEPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const auto type = static_cast<CmcePrimitivesType>(utils::bitsToInt32(channelData.ptr, offset, 5));
        offset += 5;
        result.add(GlobalNames::CMCE_Primitives_Type, static_cast<int>(type));

        switch (type) {
        case CmcePrimitivesType::D_Connect:
            globalContext_.parseParams(channelData, offset, rules::d_connectRules, result);
            break;
        case CmcePrimitivesType::D_Setup:
            globalContext_.parseParams(channelData, offset, rules::d_setupRules, result);
            break;
        case CmcePrimitivesType::D_TX_Granted:
            globalContext_.parseParams(channelData, offset, rules::d_TX_GrantedRules, result);
            break;
        case CmcePrimitivesType::D_TX_Ceased:
            globalContext_.parseParams(channelData, offset, rules::d_TX_CeasedRules, result);
            break;
        case CmcePrimitivesType::D_Release:
            globalContext_.parseParams(channelData, offset, rules::d_ReleaseRules, result);
            break;
        case CmcePrimitivesType::D_Info:
            globalContext_.parseParams(channelData, offset, rules::d_infoRules, result);
            break;
        case CmcePrimitivesType::D_SDS_Data: {
            offset = globalContext_.parseParams(channelData, offset, rules::d_SdsDataRules, result);
            if (result.contains(GlobalNames::User_Defined_Data4_Length)) {
                LogicChannel sdsData;
                sdsData.ptr = channelData.ptr + offset;
                sdsData.length = result.value(GlobalNames::User_Defined_Data4_Length);
                sds_.parseSDS(sdsData, 0, result);
            }
            break;
        }
        default:
            result.add(GlobalNames::UnknowData, 1);
            break;
        }
    }

    void MleLevel::parseMLEPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const auto type = static_cast<MlePrimitivesType>(utils::bitsToInt32(channelData.ptr, offset, 3));
        offset += 3;
        result.add(GlobalNames::MLE_Primitives_Type, static_cast<int>(type));

        switch (type) {
        case MlePrimitivesType::D_NWRK_BROADCAST: {
            offset = globalContext_.parseParams(channelData, offset, rules::d_Nwrk_BroadcastRules, result);
            int count = 0;
            if (result.tryGetValue(GlobalNames::Number_of_Neighbour_cells_element, count)) {
                for (int i = 0; i < count; ++i) {
                    ReceivedData neighbourData;
                    offset = globalContext_.parseParams(channelData, offset, rules::neighbour_Cell_InfoRules, neighbourData);
                    int cellID = 0;
                    if (neighbourData.tryGetValue(GlobalNames::Cell_identifier, cellID)) {
                        if (globalContext_.neighbourList.size() < 32) {
                            globalContext_.neighbourList.push_back(neighbourData);
                        }
                    }
                }
            }
            break;
        }
        default:
            result.add(GlobalNames::UnknowData, 1);
            break;
        }
    }

    LlcLevel::LlcLevel(GlobalContext& globalContext)
        : mle_(globalContext) {}

    void LlcLevel::parse(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const auto llcType = static_cast<LLCPduType>(utils::bitsToInt32(channelData.ptr, offset, 4));
        offset += 4;
        result.add(GlobalNames::LLC_Pdu_Type, static_cast<int>(llcType));

        bool fcsIsGood = true;

        switch (llcType) {
        case LLCPduType::BL_ADATA:
            offset += 2;
            break;
        case LLCPduType::BL_ADATA_FCS:
            offset += 2;
            fcsIsGood = calculateFCS(channelData, offset);
            break;
        case LLCPduType::BL_DATA:
            offset += 1;
            break;
        case LLCPduType::BL_DATA_FCS:
            offset += 1;
            fcsIsGood = calculateFCS(channelData, offset);
            break;
        case LLCPduType::BL_UDATA:
            break;
        case LLCPduType::BL_UDATA_FCS:
            fcsIsGood = calculateFCS(channelData, offset);
            break;
        case LLCPduType::BL_ACK:
            offset += 1;
            break;
        case LLCPduType::BL_ACK_FCS:
            offset += 1;
            fcsIsGood = calculateFCS(channelData, offset);
            break;
        default:
            result.add(GlobalNames::UnknowData, 1);
            return;
        }

        if (fcsIsGood) {
            mle_.parse(channelData, offset, result);
        }
        else {
            result.add(GlobalNames::UnknowData, 1);
        }
    }

    bool LlcLevel::calculateFCS(const LogicChannel& channelData, int offset) const {
        std::uint32_t lsfr = 0xffffffffU;
        for (int i = offset; i < channelData.length; ++i) {
            const std::uint32_t bit = channelData.ptr[i] ^ (lsfr & 0x1U);
            lsfr >>= 1U;
            if (bit != 0) {
                lsfr ^= LLC_POLY;
            }
        }
        return lsfr == LLC_GOOD_FCS;
    }

    MacLevel::MacLevel(GlobalContext& globalContext)
        : globalContext_(globalContext), llc_(globalContext) {
        for (auto& buffer : tempBuffers_) {
            buffer.resize(4096);
        }
    }

    void MacLevel::accessAsignPDU(const LogicChannel& channelData) {
        int offset = 0;
        const int header = utils::bitsToInt32(channelData.ptr, offset, 2);
        offset += 2;
        const int newField1 = utils::bitsToInt32(channelData.ptr, offset, 6);
        offset += 6;
        const int newField2 = utils::bitsToInt32(channelData.ptr, offset, 6);

        field1 = newField1;
        field2 = newField2;

        if (channelData.frame == 18) {
            downLinkChannelType = ChannelType::Common;
            return;
        }

        switch (header) {
        case 0:
            downLinkChannelType = ChannelType::Common;
            break;
        case 1:
        case 2:
        case 3:
            switch (field1) {
            case 0: downLinkChannelType = ChannelType::Unalloc; break;
            case 1: downLinkChannelType = ChannelType::Assigned; break;
            case 2: downLinkChannelType = ChannelType::Common; break;
            case 3: downLinkChannelType = ChannelType::Reserved; break;
            default: downLinkChannelType = ChannelType::Traffic; break;
            }
            break;
        }
    }

    void MacLevel::resetAACH() {
        field1 = 0;
        downLinkChannelType = ChannelType::Common;
        field2 = 0;
        upLinkChannelType = ChannelType::Common;
    }

    void MacLevel::usignalPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        halfSlotStolen = utils::bitsToInt32(channelData.ptr, offset, 1) != 0;
        ++offset;
    }

    void MacLevel::syncPDU(const LogicChannel& channelData, ReceivedData& result) {
        int offset = 0;
        const int systemCode = utils::bitsToInt32(channelData.ptr, offset, 4);
        offset += 4;
        result.setValue(GlobalNames::SystemCode, systemCode);

        switch (systemCode) {
        case 12:
        case 13: {
            const int pduType = utils::bitsToInt32(channelData.ptr, offset, 2);
            offset += 2;
            result.setValue(GlobalNames::SYNC_PDU_type, pduType);
            switch (pduType) {
            case 0:
                globalContext_.parseParams(channelData, offset, rules::dmacSyncInfoRulesDMO, result);
                break;
            case 1:
                globalContext_.parseParams(channelData, offset, rules::dpresSyncInfoRules, result);
                break;
            default:
                result.setValue(GlobalNames::UnknowData, 1);
                break;
            }
            break;
        }
        default:
            globalContext_.parseParams(channelData, offset, rules::syncInfoRulesTMO, result);
            break;
        }
    }

    void MacLevel::syncPDUHalfSlot(const LogicChannel& channelData, ReceivedData& result) {
        int pduType = -1;
        if (!result.tryGetValue(GlobalNames::SYNC_PDU_type, pduType)) {
            return;
        }

        switch (pduType) {
        case 0: {
            int offset = globalContext_.parseParams(channelData, 0, rules::dmacSyncInfoHalfSlotRules, result);
            int realLength = channelData.length - offset;
            if (result.value(GlobalNames::Fill_bit) != 0) {
                realLength = calcRealLength(channelData.ptr, offset, realLength);
            }
            if (result.value(GlobalNames::Fragmentation_flag) == 1) {
                createFraqmentsBuffer(channelData, offset, realLength, &result);
            }
            break;
        }
        case 1:
            globalContext_.parseParams(channelData, 0, rules::dpresSyncInfoHalfSlotRules, result);
            break;
        default:
            result.setValue(GlobalNames::UnknowData, 1);
            break;
        }
    }

    int MacLevel::sysInfoPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const int subType = utils::bitsToByte(channelData.ptr, offset, 2);
        result.setValue(GlobalNames::MAC_Broadcast_Type, subType);
        offset += 2;

        switch (subType) {
        case 1:
            offset = globalContext_.parseParams(channelData, offset, rules::accessDefineRules, result);
            break;
        case 0:
            offset = globalContext_.parseParams(channelData, offset, rules::sysInfoRules, result);
            break;
        default:
            break;
        }
        return offset;
    }

    void MacLevel::tmoParseMacPDU(const LogicChannel& channelData, std::vector<ReceivedData>& result) {
        constexpr int nullPduLength = 16;
        int blockOffset = 0;
        bool pduEnd = false;

        for (int i = 0; i < 5; ++i) {
            int offset = blockOffset;
            ReceivedData pdu;
            pdu.add(GlobalNames::CurrTimeSlot, channelData.timeSlot);

            const auto resourceType = static_cast<MAC_PDU_Type>(utils::bitsToInt32(channelData.ptr, offset, 2));
            offset += 2;
            pdu.add(GlobalNames::MAC_PDU_Type, static_cast<int>(resourceType));

            switch (resourceType) {
            case MAC_PDU_Type::Broadcast:
                blockOffset = sysInfoPDU(channelData, offset, pdu);
                break;
            case MAC_PDU_Type::MAC_resource:
                resourcePDU(channelData, offset, pdu);
                blockOffset += (pdu.value(GlobalNames::Length_indication) * 8);
                break;
            case MAC_PDU_Type::MAC_U_Signal:
                pduEnd = true;
                if (channelData.length == 124) {
                    usignalPDU(channelData, offset, pdu);
                }
                break;
            case MAC_PDU_Type::MAC_frag: {
                const int isTheEnd = utils::bitsToInt32(channelData.ptr, offset, 1);
                ++offset;
                if (isTheEnd == 0) {
                    macFraqPDU(channelData, offset, pdu);
                    pduEnd = true;
                }
                else {
                    macEndPDU(channelData, offset, pdu);
                    blockOffset += (pdu.value(GlobalNames::Length_indication) * 8);
                }
                break;
            }
            }

            const bool nullPdu = pdu.contains(GlobalNames::Null_pdu) || pdu.contains(GlobalNames::OutOfBuffer);
            if (nullPdu) {
                pduEnd = true;
            }
            if (!nullPdu) {
                result.push_back(pdu);
            }
            if ((blockOffset >= (channelData.length - nullPduLength)) || pduEnd) {
                break;
            }
        }
    }

    void MacLevel::resourcePDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const int startOffset = offset - 2;
        offset = globalContext_.parseParams(channelData, offset, rules::macResourceRules, result);

        const int encr = result.value(GlobalNames::Encryption_mode);
        const int lengthIndication = result.value(GlobalNames::Length_indication);
        halfSlotStolen = (lengthIndication == 62) || (lengthIndication == 63);

        if (((lengthIndication > 58) && (lengthIndication < 62)) || (lengthIndication == 0) || result.value(GlobalNames::Address_type) == 0) {
            result.add(GlobalNames::Null_pdu, 1);
            return;
        }

        if (encr != 0) {
            return;
        }

        offset = globalContext_.parseParams(channelData, offset, rules::channelAllocationRules, result);

        if (lengthIndication != 63) {
            llc_.parse(channelData, offset, result);
            return;
        }

        int realLength = std::min(lengthIndication * 8 - (offset - startOffset), channelData.length - offset);
        if (result.value(GlobalNames::Fill_bit) != 0) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        if (realLength < 0) {
            result.add(GlobalNames::UnknowData, 1);
            return;
        }
        createFraqmentsBuffer(channelData, offset, realLength, &result);
    }

    void MacLevel::macEndPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const int startOffset = offset - 3;
        offset = globalContext_.parseParams(channelData, offset, rules::macEndRules, result);

        if (fraqmentsBufferIsEmpty(channelData.timeSlot)) {
            return;
        }

        offset = globalContext_.parseParams(channelData, offset, rules::channelAllocationRules, result);
        const int lengthIndication = result.value(GlobalNames::Length_indication);
        int realLength = std::min(lengthIndication * 8 - (offset - startOffset), channelData.length - offset);

        if (lengthIndication > 58) {
            result.add(GlobalNames::Null_pdu, 1);
            return;
        }
        if (result.value(GlobalNames::Fill_bit) != 0) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        if (realLength < 0) {
            result.add(GlobalNames::UnknowData, 1);
            return;
        }

        addFraqmentsToBuffer(channelData, offset, realLength);
        LogicChannel newBuffer = getDeFragmentedBuffer(channelData, result);
        if (newBuffer.length == 0) {
            result.add(GlobalNames::Null_pdu, 1);
            return;
        }
        llc_.parse(newBuffer, 0, result);
    }

    void MacLevel::macFraqPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        if (fraqmentsBufferIsEmpty(channelData.timeSlot)) {
            return;
        }

        const bool fill = utils::bitsToInt32(channelData.ptr, offset, 1) != 0;
        ++offset;
        int realLength = channelData.length - offset;
        if (fill) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        if (realLength < 0) {
            result.add(GlobalNames::UnknowData, 1);
            return;
        }
        addFraqmentsToBuffer(channelData, offset, realLength);
    }

    void MacLevel::dmoParseMacPDU(const LogicChannel& channelData, std::vector<ReceivedData>& result) {
        int offset = 0;
        ReceivedData pdu;
        pdu.add(GlobalNames::CurrTimeSlot, channelData.timeSlot);
        const auto pduType = static_cast<MAC_PDU_Type>(utils::bitsToInt32(channelData.ptr, offset, 2));
        offset += 2;
        pdu.add(GlobalNames::MAC_PDU_Type, static_cast<int>(pduType));

        switch (pduType) {
        case MAC_PDU_Type::MAC_resource:
            dmacDataPDU(channelData, offset, pdu);
            break;
        case MAC_PDU_Type::MAC_frag: {
            const int isTheEnd = utils::bitsToInt32(channelData.ptr, offset, 1);
            ++offset;
            if (isTheEnd == 0) {
                dmacFraqPDU(channelData, offset, pdu);
            }
            else {
                dmacEndPDU(channelData, offset, pdu);
            }
            break;
        }
        default:
            break;
        }

        result.push_back(pdu);
    }

    void MacLevel::dmacDataPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        offset = globalContext_.parseParams(channelData, offset, rules::dmacDataRules, result);
        int realLength = channelData.length - offset;
        if (result.value(GlobalNames::Fill_bit) != 0) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        halfSlotStolen = result.value(GlobalNames::Second_half_slot_stolen) == 1;
        if (result.value(GlobalNames::Fragmentation_flag) == 1) {
            createFraqmentsBuffer(channelData, offset, realLength, &result);
            return;
        }
        if (result.value(GlobalNames::Null_pdu) == 1) {
            return;
        }
    }

    void MacLevel::dmacEndPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        offset = globalContext_.parseParams(channelData, offset, rules::dmacEndRules, result);
        int realLength = channelData.length - offset;
        if (result.value(GlobalNames::Fill_bit) != 0) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        addFraqmentsToBuffer(channelData, offset, realLength);
        LogicChannel newBuffer = getDeFragmentedBuffer(channelData, result);
        if (newBuffer.length == 0) {
            return;
        }
    }

    void MacLevel::dmacFraqPDU(const LogicChannel& channelData, int offset, ReceivedData& result) {
        const bool fill = utils::bitsToInt32(channelData.ptr, offset, 1) != 0;
        ++offset;
        int realLength = channelData.length - offset;
        if (fill) {
            realLength = calcRealLength(channelData.ptr, offset, realLength);
        }
        addFraqmentsToBuffer(channelData, offset, realLength);
    }

    int MacLevel::calcRealLength(const std::uint8_t* buffer, int offset, int currentLength) const {
        int pos = offset + currentLength - 1;
        while (pos > offset && buffer[pos] == 0) {
            --pos;
        }
        return pos - offset;
    }

    void MacLevel::createFraqmentsBuffer(const LogicChannel& buffer, int offset, int length, const ReceivedData* header) {
        const int ts = buffer.timeSlot - 1;
        if (ts < 0 || ts >= 4) {
            return;
        }
        if (writeAddress_[ts] + length < static_cast<int>(tempBuffers_[ts].size())) {
            std::memcpy(tempBuffers_[ts].data(), buffer.ptr + offset, static_cast<std::size_t>(length));
            writeAddress_[ts] = length;
            if (header != nullptr) {
                fragmentsHeader_[ts] = *header;
            }
        }
    }

    void MacLevel::addFraqmentsToBuffer(const LogicChannel& buffer, int offset, int length) {
        const int ts = buffer.timeSlot - 1;
        if (ts < 0 || ts >= 4 || writeAddress_[ts] == 0) {
            return;
        }
        if (writeAddress_[ts] + length < static_cast<int>(tempBuffers_[ts].size())) {
            std::memcpy(tempBuffers_[ts].data() + writeAddress_[ts], buffer.ptr + offset, static_cast<std::size_t>(length));
            writeAddress_[ts] += length;
        }
    }

    bool MacLevel::fraqmentsBufferIsEmpty(int timeSlot) const {
        const int ts = timeSlot - 1;
        return ts < 0 || ts >= 4 || writeAddress_[ts] == 0;
    }

    LogicChannel MacLevel::getDeFragmentedBuffer(const LogicChannel& channelData, ReceivedData& header) {
        const int ts = channelData.timeSlot - 1;
        LogicChannel result;
        if (ts < 0 || ts >= 4) {
            return result;
        }

        result.ptr = tempBuffers_[ts].data();
        result.length = writeAddress_[ts];
        result.crcIsOk = true;
        result.timeSlot = channelData.timeSlot;
        result.frame = channelData.frame;

        for (GlobalNames key : { GlobalNames::Encryption_mode, GlobalNames::Address_type, GlobalNames::SSI }) {
            if (fragmentsHeader_[ts].contains(key)) {
                header.add(key, fragmentsHeader_[ts].value(key));
            }
        }

        fragmentsHeader_[ts].clear();
        writeAddress_[ts] = 0;
        return result;
    }
}
