#pragma once

#include "generated_enums.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tetra {
    constexpr std::size_t toIndex(GlobalNames name) {
        return static_cast<std::size_t>(name);
    }

    constexpr std::size_t GLOBAL_NAME_COUNT = toIndex(GlobalNames::End);

    struct LogicChannel {
        std::uint8_t* ptr = nullptr;
        int length = 0;
        bool crcIsOk = false;
        int timeSlot = 0;
        int frame = 0;
    };

    enum class BurstType {
        None,
        NDB1,
        NDB2,
        SYNC,
        WaitBurst
    };

    enum class Mode {
        TMO,
        DMO
    };

    struct Burst {
        Mode mode = Mode::TMO;
        BurstType type = BurstType::None;
        std::uint8_t* ptr = nullptr;
        int length = 0;
    };

    struct Rule {
        GlobalNames globalName;
        int length;
        RulesType type;
        int ext1;
        int ext2;
        int ext3;

        Rule(
            GlobalNames globalName,
            int length,
            RulesType type = RulesType::Direct,
            int ext1 = 0,
            int ext2 = 0,
            int ext3 = 0
        ) : globalName(globalName), length(length), type(type), ext1(ext1), ext2(ext2), ext3(ext3) {}
    };

    struct GroupsEntry {
        std::string name;
        int priority = 0;
    };

    struct NetworkEntry {
        std::map<int, GroupsEntry> knowGroups;
    };

    struct CurrentLoad {
        int to = 0;
        int from = 0;
        int type = 0;
        std::string groupName;
        int groupPriority = 0;
        bool isClear = false;
        int callId = 0;
    };

    struct CallsEntry {
        int carrier = 0;
        int callID = 0;
        int type = 0;
        int from = 0;
        int to = 0;
        int isClear = 0;
        int duplex = 0;
        int watchDog = 0;
        int assignedSlot = 0;
    };

    struct ReceivedData {
        std::array<int, GLOBAL_NAME_COUNT> data{};

        ReceivedData();

        void clear();
        bool tryGetValue(GlobalNames name, int& value) const;
        bool contains(GlobalNames name) const;
        int value(GlobalNames name) const;
        void setValue(GlobalNames name, int value);
        void add(GlobalNames name, int value);
    };

    namespace utils {
        std::uint8_t bitsToByte(const std::uint8_t* bitsBuffer, int offset, int length);
        std::uint8_t bitsToChar(const std::uint8_t* bitsBuffer, int offset, int length);
        std::uint32_t bitsToUInt32(const std::uint8_t* bitsBuffer, int offset, int length);
        int bitsToInt32(const std::uint8_t* bitsBuffer, int offset, int length);
        std::uint64_t bitsToULong(const std::uint8_t* bitsBuffer, int offset, int length);
        std::int64_t bitsToLong(const std::uint8_t* bitsBuffer, int offset, int length);
        std::string bitsToString(const std::uint8_t* bitsBuffer, int offset, int length);
        std::string bitsToString(const std::int8_t* bitsBuffer, int offset, int length);
        void byteToBits(std::uint8_t data, std::uint8_t* bitsBuffer, int bufferOffset);
        void intToBits(int data, std::uint8_t* bitsBuffer, int bufferOffset);
        void uintToBits(std::uint32_t data, std::uint8_t* bitsBuffer, int firstBitOffset);
        std::uint32_t createScramblerCode(int mcc, int mnc, int colour);
        std::uint32_t createScramblerCode(int mnc, int sourceAddress);
    }

    class NetworkTime {
    public:
        int timeSlot() const { return tn_; }
        int timeSlotSlave() const { return tnSlave_; }
        int frame() const { return fn_; }
        int frameSlave() const { return fnSlave_; }
        int superFrame() const { return mn_; }
        bool isSynchronized() const { return isSynchronized_; }
        bool timeForBSCH() const { return timeBSCH_; }
        bool timeForBNCH() const { return timeBNCH_; }
        bool frame18() const { return fn18_; }
        bool frame18Slave() const { return fn18Slave_; }

        void synchronize(int tn, int fn, int mn);
        void synchronizeMaster(int tn, int fn);
        void synchronizeSlave(int tn, int fn);
        void addTimeSlot();

    private:
        void timeChecker();
        void calculateSlaveTime();

        int tn_ = 1;
        int fn_ = 1;
        int mn_ = 1;
        bool isSynchronized_ = false;
        bool timeBNCH_ = false;
        bool fn18_ = false;
        bool timeBSCH_ = false;
        bool fn18Slave_ = false;
        int tnSlave_ = 0;
        int fnSlave_ = 0;
    };

    class GlobalContext {
    public:
        bool ignoreEncryptedSpeech = false;
        std::vector<ReceivedData> neighbourList;

        int parseParams(const LogicChannel& channelData, int offset, const std::vector<Rule>& rules, ReceivedData& result);
        long long frequencyCalc(bool isFull, int carrier, int band = 0, int offset = 0);
        int carrierCalc(long long frequency);

    private:
        int currentBand_ = 0;
        int currentOffset_ = 0;
    };
}
