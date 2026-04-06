#pragma once

#include <cstdint>
#include <string>

namespace tetra {
    class VoiceDecoderLibrary {
    public:
        VoiceDecoderLibrary();
        ~VoiceDecoderLibrary();

        VoiceDecoderLibrary(const VoiceDecoderLibrary&) = delete;
        VoiceDecoderLibrary& operator=(const VoiceDecoderLibrary&) = delete;

        bool available() const { return available_; }
        const std::string& error() const { return error_; }

        void* createChannelState() const;
        int channelDecode(int firstPass, std::uint8_t* input, short* output, int halfSlotStolen) const;
        int speechDecode(short* input, short* output, void* channelState) const;

    private:
        void unload();

        using decode_init_t = void* (*)();
        using cdec_t = int (*)(int, std::uint8_t*, short*, int);
        using sdec_t = int (*)(short*, short*, void*);

        void* libraryHandle_ = nullptr;
        decode_init_t decodeInit_ = nullptr;
        cdec_t cdec_ = nullptr;
        sdec_t sdec_ = nullptr;
        bool available_ = false;
        std::string error_;
    };
}
