#include "tetra_voice.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace tetra {
    VoiceDecoderLibrary::VoiceDecoderLibrary() {
#if defined(_WIN32)
        libraryHandle_ = reinterpret_cast<void*>(LoadLibraryA("tetraVoiceDec.dll"));
        if (libraryHandle_ == nullptr) {
            libraryHandle_ = reinterpret_cast<void*>(LoadLibraryA("tetraVoiceDec"));
        }
        if (libraryHandle_ != nullptr) {
            decodeInit_ = reinterpret_cast<decode_init_t>(GetProcAddress(reinterpret_cast<HMODULE>(libraryHandle_), "tetra_decode_init"));
            cdec_ = reinterpret_cast<cdec_t>(GetProcAddress(reinterpret_cast<HMODULE>(libraryHandle_), "tetra_cdec"));
            sdec_ = reinterpret_cast<sdec_t>(GetProcAddress(reinterpret_cast<HMODULE>(libraryHandle_), "tetra_sdec"));
        }
#else
        libraryHandle_ = dlopen("libtetraVoiceDec.so", RTLD_LAZY);
        if (libraryHandle_ == nullptr) {
            libraryHandle_ = dlopen("tetraVoiceDec.so", RTLD_LAZY);
        }
        if (libraryHandle_ != nullptr) {
            decodeInit_ = reinterpret_cast<decode_init_t>(dlsym(libraryHandle_, "tetra_decode_init"));
            cdec_ = reinterpret_cast<cdec_t>(dlsym(libraryHandle_, "tetra_cdec"));
            sdec_ = reinterpret_cast<sdec_t>(dlsym(libraryHandle_, "tetra_sdec"));
        }
#endif

        available_ = libraryHandle_ != nullptr && decodeInit_ != nullptr && cdec_ != nullptr && sdec_ != nullptr;
        if (!available_) {
            error_ = "tetraVoiceDec library not found or incomplete";
            unload();
        }
    }

    VoiceDecoderLibrary::~VoiceDecoderLibrary() {
        unload();
    }

    void* VoiceDecoderLibrary::createChannelState() const {
        if (!available_) {
            return nullptr;
        }
        return decodeInit_();
    }

    int VoiceDecoderLibrary::channelDecode(int firstPass, std::uint8_t* input, short* output, int halfSlotStolen) const {
        if (!available_ || input == nullptr || output == nullptr) {
            return -1;
        }
        return cdec_(firstPass, input, output, halfSlotStolen);
    }

    int VoiceDecoderLibrary::speechDecode(short* input, short* output, void* channelState) const {
        if (!available_ || input == nullptr || output == nullptr || channelState == nullptr) {
            return -1;
        }
        return sdec_(input, output, channelState);
    }

    void VoiceDecoderLibrary::unload() {
#if defined(_WIN32)
        if (libraryHandle_ != nullptr) {
            FreeLibrary(reinterpret_cast<HMODULE>(libraryHandle_));
        }
#else
        if (libraryHandle_ != nullptr) {
            dlclose(libraryHandle_);
        }
#endif
        libraryHandle_ = nullptr;
        decodeInit_ = nullptr;
        cdec_ = nullptr;
        sdec_ = nullptr;
        available_ = false;
    }
}
