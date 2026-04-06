#include "tetra_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tetra {
    namespace {
        constexpr int AUDIO_OUTPUT_SAMPLES = 480;
        constexpr float AUDIO_GAIN = 0.0001f / static_cast<float>(INT16_MAX);
    }

    TetraDecoder::TetraDecoder() {
        recreateState();
        if (voiceDecoder_.available()) {
            for (auto& channel : voiceChannels_) {
                channel = voiceDecoder_.createChannelState();
            }
        }
    }

    void TetraDecoder::reset() {
        globalContext_.neighbourList.clear();
        networkTime_ = NetworkTime();
        syncInfo_.clear();
        data_.clear();
        timeCounter_ = 0;
        badBurstCounter_ = 0.0f;
        averageBer_ = 0.0f;
        mer_ = 0.0f;
        burstReceived_ = false;
        haveErrors_ = false;
        tetraMode_ = Mode::TMO;
        fpass_ = 1;
        recreateState();
        voiceChannels_.fill(nullptr);
        if (voiceDecoder_.available()) {
            for (auto& channel : voiceChannels_) {
                channel = voiceDecoder_.createChannelState();
            }
        }
    }

    void TetraDecoder::recreateState() {
        lowerMac_ = std::make_unique<LowerMacLevel>();
        parse_ = std::make_unique<MacLevel>(globalContext_);
        bbBuffer_.assign(30, 0);
        bkn1Buffer_.assign(216, 0);
        bkn2Buffer_.assign(216, 0);
        sb1Buffer_.assign(120, 0);
    }

    int TetraDecoder::process(const Burst& burst, float* audioOut, int audioOutLength) {
        int trafficChannel = 0;

        burstReceived_ = (burst.type != BurstType::None);

        ++timeCounter_;
        if (timeCounter_ > 100) {
            mer_ = (badBurstCounter_ / static_cast<float>(timeCounter_)) * 100.0f;
            timeCounter_ = 0;
            badBurstCounter_ = 0.0f;
        }

        networkTime_.addTimeSlot();

        if (burst.type == BurstType::None) {
            haveErrors_ = true;
            if (tetraMode_ == Mode::TMO) {
                badBurstCounter_ += 1.0f;
            }
            return trafficChannel;
        }

        haveErrors_ = false;
        parse_->resetAACH();
        data_.clear();
        syncInfo_.clear();

        if (burst.type == BurstType::SYNC) {
            phyLevel_.extractSBChannels(burst, sb1Buffer_.data());

            logicChannel_ = lowerMac_->extractLogicChannelFromSB(sb1Buffer_.data(), static_cast<int>(sb1Buffer_.size()));
            badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.5f;
            haveErrors_ = !logicChannel_.crcIsOk;
            averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
            if (logicChannel_.crcIsOk) {
                parse_->syncPDU(logicChannel_, syncInfo_);

                if (syncInfo_.value(GlobalNames::SystemCode) < 8) {
                    tetraMode_ = Mode::TMO;
                    lowerMac_->setScramblerCode(utils::createScramblerCode(
                        syncInfo_.value(GlobalNames::MCC),
                        syncInfo_.value(GlobalNames::MNC),
                        syncInfo_.value(GlobalNames::ColorCode)
                    ));
                    networkTime_.synchronize(
                        syncInfo_.value(GlobalNames::TimeSlot),
                        syncInfo_.value(GlobalNames::Frame),
                        syncInfo_.value(GlobalNames::MultiFrame)
                    );
                }
                else {
                    tetraMode_ = Mode::DMO;
                    if (syncInfo_.value(GlobalNames::SYNC_PDU_type) == 0) {
                        if (syncInfo_.value(GlobalNames::Master_slave_link_flag) == 1 || syncInfo_.value(GlobalNames::Communication_type) == 0) {
                            networkTime_.synchronizeMaster(syncInfo_.value(GlobalNames::TimeSlot), syncInfo_.value(GlobalNames::Frame));
                        }
                        else {
                            networkTime_.synchronizeSlave(syncInfo_.value(GlobalNames::TimeSlot), syncInfo_.value(GlobalNames::Frame));
                        }
                    }
                }
            }

            phyLevel_.extractPhyChannels(tetraMode_, burst, bbBuffer_.data(), bkn1Buffer_.data(), bkn2Buffer_.data());

            if (tetraMode_ == Mode::TMO) {
                logicChannel_ = lowerMac_->extractLogicChannelFromBKN(bkn2Buffer_.data(), static_cast<int>(bkn2Buffer_.size()));
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.5f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
                if (logicChannel_.crcIsOk) {
                    parse_->tmoParseMacPDU(logicChannel_, data_);
                }
            }
            else {
                logicChannel_ = lowerMac_->extractLogicChannelFromBKN2(bkn2Buffer_.data(), static_cast<int>(bkn2Buffer_.size()));
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.5f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
                if (logicChannel_.crcIsOk) {
                    parse_->syncPDUHalfSlot(logicChannel_, syncInfo_);

                    if (syncInfo_.value(GlobalNames::Communication_type) == 0) {
                        if (syncInfo_.contains(GlobalNames::MNC) && syncInfo_.contains(GlobalNames::Source_address)) {
                            lowerMac_->setScramblerCode(utils::createScramblerCode(
                                syncInfo_.value(GlobalNames::MNC),
                                syncInfo_.value(GlobalNames::Source_address)
                            ));
                        }
                    }
                    else if (syncInfo_.value(GlobalNames::Communication_type) == 1) {
                        if (syncInfo_.contains(GlobalNames::Repeater_address) && syncInfo_.contains(GlobalNames::Source_address)) {
                            lowerMac_->setScramblerCode(utils::createScramblerCode(
                                syncInfo_.value(GlobalNames::Repeater_address),
                                syncInfo_.value(GlobalNames::Source_address)
                            ));
                        }
                    }
                }
            }

            emitSyncInfo(syncInfo_);
        }

        if (burst.type != BurstType::SYNC) {
            phyLevel_.extractPhyChannels(tetraMode_, burst, bbBuffer_.data(), bkn1Buffer_.data(), bkn2Buffer_.data());

            if (tetraMode_ == Mode::TMO) {
                logicChannel_ = lowerMac_->extractLogicChannelFromBB(bbBuffer_.data(), static_cast<int>(bbBuffer_.size()));
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.2f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                if (logicChannel_.crcIsOk) {
                    parse_->accessAsignPDU(logicChannel_);
                }
            }

            switch (burst.type) {
            case BurstType::NDB1:
                if (((tetraMode_ == Mode::TMO) && (parse_->downLinkChannelType == ChannelType::Traffic))
                    || ((tetraMode_ == Mode::DMO) && (!networkTime_.frame18()) && (networkTime_.timeSlot() == 1))
                    || ((tetraMode_ == Mode::DMO) && (!networkTime_.frame18Slave()) && (networkTime_.timeSlotSlave() == 1))) {
                    logicChannel_ = lowerMac_->extractVoiceDataFromBKN1BKN2(
                        bkn1Buffer_.data(),
                        bkn2Buffer_.data(),
                        static_cast<int>(bkn1Buffer_.size())
                    );
                    const bool isAudio = decodeAudio(audioOut, logicChannel_.ptr, logicChannel_.length, false, networkTime_.timeSlot(), audioOutLength);
                    trafficChannel = isAudio ? networkTime_.timeSlot() : 0;
                    break;
                }

                logicChannel_ = lowerMac_->extractLogicChannelFromBKN1BKN2(
                    bkn1Buffer_.data(),
                    bkn2Buffer_.data(),
                    static_cast<int>(bkn1Buffer_.size())
                );
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.8f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
                if (logicChannel_.crcIsOk) {
                    if (tetraMode_ == Mode::TMO) {
                        parse_->tmoParseMacPDU(logicChannel_, data_);
                    }
                    else {
                        parse_->dmoParseMacPDU(logicChannel_, data_);
                    }
                }
                break;

            case BurstType::NDB2:
                logicChannel_ = lowerMac_->extractLogicChannelFromBKN(bkn1Buffer_.data(), static_cast<int>(bkn1Buffer_.size()));
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.4f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
                if (logicChannel_.crcIsOk) {
                    if (tetraMode_ == Mode::TMO) {
                        parse_->tmoParseMacPDU(logicChannel_, data_);
                    }
                    else {
                        parse_->dmoParseMacPDU(logicChannel_, data_);
                    }
                }

                if ((parse_->downLinkChannelType == ChannelType::Traffic && !parse_->halfSlotStolen)
                    || ((tetraMode_ == Mode::DMO) && (!networkTime_.frame18()) && (networkTime_.timeSlot() == 1) && (!parse_->halfSlotStolen))
                    || ((tetraMode_ == Mode::DMO) && (!networkTime_.frame18Slave()) && (networkTime_.timeSlotSlave() == 1) && (!parse_->halfSlotStolen))) {
                    logicChannel_ = lowerMac_->extractVoiceDataFromBKN2(bkn2Buffer_.data(), static_cast<int>(bkn2Buffer_.size()));
                    const bool isAudio = decodeAudio(audioOut, logicChannel_.ptr, logicChannel_.length, true, networkTime_.timeSlot(), audioOutLength);
                    trafficChannel = isAudio ? networkTime_.timeSlot() : 0;
                    break;
                }

                logicChannel_ = lowerMac_->extractLogicChannelFromBKN(bkn2Buffer_.data(), static_cast<int>(bkn2Buffer_.size()));
                logicChannel_.timeSlot = networkTime_.timeSlot();
                logicChannel_.frame = networkTime_.frame();

                badBurstCounter_ += logicChannel_.crcIsOk ? 0.0f : 0.4f;
                haveErrors_ = haveErrors_ || !logicChannel_.crcIsOk;
                averageBer_ = averageBer_ * 0.5f + lowerMac_->ber() * 0.5f;
                if (logicChannel_.crcIsOk) {
                    if (tetraMode_ == Mode::TMO) {
                        parse_->tmoParseMacPDU(logicChannel_, data_);
                    }
                    else {
                        parse_->dmoParseMacPDU(logicChannel_, data_);
                    }
                }
                break;

            default:
                break;
            }
        }

        if (!data_.empty()) {
            emitData(data_);
        }

        return trafficChannel;
    }

    void TetraDecoder::emitSyncInfo(const ReceivedData& syncInfo) {
        if (syncInfoReadyHandler_) {
            syncInfoReadyHandler_(syncInfo);
        }
    }

    void TetraDecoder::emitData(const std::vector<ReceivedData>& data) {
        if (dataReadyHandler_) {
            dataReadyHandler_(data);
        }
    }

    bool TetraDecoder::decodeAudio(float* audioBuffer, std::uint8_t* buf, int length, bool stolen, int channel, int audioOutLength) {
        if (audioBuffer == nullptr || buf == nullptr || audioOutLength < AUDIO_OUTPUT_SAMPLES) {
            return false;
        }

        std::fill(audioBuffer, audioBuffer + audioOutLength, 0.0f);
        if (!voiceDecoder_.available() || channel < 1 || channel > 4 || voiceChannels_[static_cast<std::size_t>(channel - 1)] == nullptr) {
            return false;
        }

        voiceDecoder_.channelDecode(fpass_, buf, cdc_.data(), stolen ? 1 : 0);
        fpass_ = 0;

        const bool noErrors = (cdc_[0] == 0) || (cdc_[138] == 0);

        badBurstCounter_ += ((cdc_[0] == 0) || stolen) ? 0.0f : 0.4f;
        badBurstCounter_ += (cdc_[138] == 0) ? 0.0f : 0.4f;

        voiceDecoder_.speechDecode(cdc_.data(), sdc_.data(), voiceChannels_[static_cast<std::size_t>(channel - 1)]);
        shortToFloatPtr(sdc_.data(), audioBuffer, AUDIO_OUTPUT_SAMPLES);

        return noErrors;
    }

    void TetraDecoder::shortToFloatPtr(const short* source, float* dest, int length) const {
        for (int i = 0; i < length; ++i) {
            dest[i] = static_cast<float>(source[i]) * AUDIO_GAIN;
        }
    }
}
