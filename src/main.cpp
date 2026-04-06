#include "tetra_runtime.hpp"

#include <imgui.h>
#include <module.h>
#include <config.h>
#include <core.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/types.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

SDRPP_MOD_INFO{
    /* Name:            */ "tetra_decoder",
    /* Description:     */ "Native TETRA decoder module for SDR++",
    /* Author:          */ "SDR-Tetra-Plugin contributors",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ -1
};

ConfigManager config;

namespace {
    constexpr double DEFAULT_VFO_BANDWIDTH = 25000.0;
    constexpr double DEFAULT_VFO_SAMPLE_RATE = 36000.0;
    constexpr double MIN_VFO_BANDWIDTH = 12500.0;
    constexpr double MAX_VFO_BANDWIDTH = 50000.0;
    constexpr double MIN_VFO_SAMPLE_RATE = 24000.0;
    constexpr double MAX_VFO_SAMPLE_RATE = 192000.0;
    constexpr double VFO_SNAP_INTERVAL = 6250.0;
    constexpr std::size_t LOG_FOLDER_BUFFER_SIZE = 260;
    constexpr std::size_t LOG_RULE_BUFFER_SIZE = 512;
    constexpr std::size_t LOG_SEPARATOR_BUFFER_SIZE = 64;

    struct GroupNameBuffer {
        std::array<char, 96> text{};
        bool initialized = false;
    };

    class TetraDecoderModule : public ModuleManager::Instance {
    public:
        explicit TetraDecoderModule(std::string name)
            : name_(std::move(name)),
              runtime_(name_) {
            loadConfig();
            gui::menu.registerEntry(name_, menuHandler, this, this);
            enable();
        }

        ~TetraDecoderModule() override {
            disable();
            gui::menu.removeEntry(name_);
        }

        void postInit() override {}

        void enable() override {
            if (enabled_.load()) {
                return;
            }

            createVfo(0.0);
            lastTunedFrequency_.store(0);
            ignoreNextTuneReset_.store(false);
            runtime_.setSettings(settings_);
            runtime_.start();
            iqMonitor_.init(vfo_->output, iqHandler, this);
            iqMonitor_.start();
            enabled_.store(true);
        }

        void disable() override {
            if (!enabled_.load()) {
                return;
            }

            iqMonitor_.stop();
            runtime_.stop();
            destroyVfo();
            lastTunedFrequency_.store(0);
            ignoreNextTuneReset_.store(false);
            enabled_.store(false);
        }

        bool isEnabled() override {
            return enabled_.load();
        }

    private:
        void syncSettingsFromBuffers() {
            settings_.logWriteFolder = logFolderBuffer_.data();
            settings_.logEntryRules = logEntryRulesBuffer_.data();
            settings_.logFileNameRules = logFileRulesBuffer_.data();
            settings_.logSeparator = logSeparatorBuffer_.data();
        }

        void resetDecoderState() {
            runtime_.reset();
            lastTunedFrequency_.store(0);
            ignoreNextTuneReset_.store(false);
        }

        static long long groupKey(int nmi, int gssi) {
            return (static_cast<long long>(nmi) << 32) | static_cast<unsigned int>(gssi);
        }

        static void iqHandler(dsp::complex_t* data, int count, void* ctx) {
            auto* self = static_cast<TetraDecoderModule*>(ctx);
            if (!self->enabled_.load() || self->vfo_ == nullptr || data == nullptr || count <= 0) {
                return;
            }

            const auto tunedFrequency = static_cast<long long>(std::llround(gui::waterfall.getCenterFrequency() + self->vfo_->getOffset()));
            const auto lastTunedFrequency = self->lastTunedFrequency_.load();
            if (lastTunedFrequency != 0 && tunedFrequency != lastTunedFrequency) {
                if (self->ignoreNextTuneReset_.load()) {
                    self->ignoreNextTuneReset_.store(false);
                }
                else {
                    self->runtime_.reset();
                }
            }
            self->lastTunedFrequency_.store(tunedFrequency);
            self->runtime_.setTunedFrequency(tunedFrequency);
            self->runtime_.pushIq(data, count, self->vfoSampleRate_.load());

            const double afcCorrection = self->runtime_.consumePendingAfcCorrection();
            if (std::fabs(afcCorrection) > 0.0) {
                self->ignoreNextTuneReset_.store(true);
                self->vfo_->setOffset(self->vfo_->getOffset() + afcCorrection);
            }
        }

        static void menuHandler(void* ctx) {
            auto* self = static_cast<TetraDecoderModule*>(ctx);
            const auto snapshot = self->runtime_.snapshot();

            if (!self->enabled_.load()) {
                style::beginDisabled();
            }

            ImGui::TextUnformatted("Receiver");
            ImGui::Separator();
            ImGui::Text("VFO bandwidth: %.0f Hz", self->vfoBandwidth_);
            ImGui::Text("VFO sample rate: %.0f sps", self->vfoSampleRate_.load());

            int bandwidth = static_cast<int>(self->vfoBandwidth_);
            if (ImGui::InputInt(("Bandwidth##tetra_bw_" + self->name_).c_str(), &bandwidth, 2500, 12500)) {
                self->vfoBandwidth_ = std::clamp<double>(static_cast<double>(bandwidth), MIN_VFO_BANDWIDTH, MAX_VFO_BANDWIDTH);
                self->applyVfoSettings();
                self->resetDecoderState();
                self->saveConfig();
            }

            int sampleRate = static_cast<int>(self->vfoSampleRate_.load());
            if (ImGui::InputInt(("Sample Rate##tetra_sr_" + self->name_).c_str(), &sampleRate, 6000, 24000)) {
                self->vfoSampleRate_.store(std::clamp<double>(static_cast<double>(sampleRate), MIN_VFO_SAMPLE_RATE, MAX_VFO_SAMPLE_RATE));
                self->applyVfoSettings();
                self->resetDecoderState();
                self->saveConfig();
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Decoder");
            ImGui::Separator();

            if (ImGui::Checkbox(("Auto Play##tetra_auto_" + self->name_).c_str(), &self->settings_.autoPlay)) {
                self->runtime_.setAutoPlay(self->settings_.autoPlay);
                self->saveConfig();
            }
            if (ImGui::Checkbox(("Ignore Encrypted Speech##tetra_ign_" + self->name_).c_str(), &self->settings_.ignoreEncryptedSpeech)) {
                self->runtime_.setSettings(self->settings_);
                self->saveConfig();
            }
            if (ImGui::Checkbox(("Enable UDP Output##tetra_udp_" + self->name_).c_str(), &self->settings_.udpEnabled)) {
                self->runtime_.setSettings(self->settings_);
                self->saveConfig();
            }
            if (self->settings_.udpEnabled) {
                int udpPort = self->settings_.udpPort;
                if (ImGui::InputInt(("UDP Port##tetra_udp_port_" + self->name_).c_str(), &udpPort, 1, 100)) {
                    self->settings_.udpPort = std::clamp(udpPort, 1, 65535);
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
            }
            if (ImGui::Checkbox(("Disable AFC##tetra_afc_" + self->name_).c_str(), &self->settings_.afcDisabled)) {
                self->runtime_.setSettings(self->settings_);
                self->saveConfig();
            }

            int blockedLevel = self->settings_.blockedLevel;
            if (ImGui::InputInt(("Blocked Level##tetra_block_" + self->name_).c_str(), &blockedLevel, 1, 10)) {
                self->settings_.blockedLevel = blockedLevel;
                self->runtime_.setSettings(self->settings_);
                self->saveConfig();
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Audio");
            ImGui::Separator();
            int selectedChannel = snapshot.selectedChannel;
            if (ImGui::RadioButton(("Mute##tetra_mute_" + self->name_).c_str(), selectedChannel == 0)) {
                self->runtime_.setSelectedChannel(0);
            }
            ImGui::SameLine();
            for (int channel = 1; channel <= 4; ++channel) {
                if (ImGui::RadioButton((std::string("CH") + std::to_string(channel) + "##tetra_ch_" + self->name_).c_str(), selectedChannel == channel)) {
                    self->runtime_.setSelectedChannel(channel);
                }
                if (channel < 4) {
                    ImGui::SameLine();
                }
            }
            self->runtime_.showAudioVolumeSlider("##_tetra_audio_vol_", ImGui::GetContentRegionAvail().x);

            ImGui::Spacing();
            ImGui::TextUnformatted("Status");
            ImGui::Separator();
            ImGui::Text("Mode: %s", snapshot.tetraMode == tetra::Mode::DMO ? "DMO" : "TMO");
            ImGui::Text("Burst: %s", snapshot.burstReceived ? "Receiving" : "Idle");
            ImGui::Text("BER: %.2f%%", snapshot.ber);
            ImGui::Text("MER: %.2f%%", snapshot.mer);
            ImGui::Text("FER: %.0f Hz", snapshot.frequencyErrorHz);
            ImGui::Text("Dropped IQ blocks: %d", snapshot.lostBuffers);
            ImGui::Text("MCC/MNC: %d / %d", snapshot.currentCellMcc, snapshot.currentCellMnc);
            ImGui::Text("Color / LA: %d / %d", snapshot.currentCellCc, snapshot.currentCellLa);
            ImGui::Text("Carrier: current %d / main %d", snapshot.currentCellCarrier, snapshot.mainCellCarrier);
            ImGui::Text("Main Frequency: %.6f MHz", static_cast<double>(snapshot.mainCellFrequency) / 1000000.0);
            if (snapshot.mainCellFrequency != 0) {
                if (ImGui::Button(("Tune Main Carrier##tetra_tune_main_" + self->name_).c_str())) {
                    sigpath::sourceManager.tune(static_cast<double>(snapshot.mainCellFrequency));
                    self->resetDecoderState();
                }
                ImGui::SameLine();
            }
            if (ImGui::Button(("Reset Decoder##tetra_reset_" + self->name_).c_str())) {
                self->resetDecoderState();
            }
            if (!snapshot.voiceAvailable) {
                ImGui::TextWrapped("Voice decoder unavailable: %s", snapshot.voiceError.c_str());
            }

            if (ImGui::CollapsingHeader("Burst Monitor")) {
                if (!snapshot.burstAngles.empty()) {
                    ImGui::PlotLines(
                        ("##tetra_burst_plot_" + self->name_).c_str(),
                        snapshot.burstAngles.data(),
                        static_cast<int>(snapshot.burstAngles.size()),
                        0,
                        nullptr,
                        -3.2f,
                        3.2f,
                        ImVec2(ImGui::GetContentRegionAvail().x, 120.0f)
                    );
                }
                else {
                    ImGui::TextUnformatted("No burst data yet.");
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Channels", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginTable(("tetra_channels_" + self->name_).c_str(), 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("CH");
                    ImGui::TableSetupColumn("Active");
                    ImGui::TableSetupColumn("From");
                    ImGui::TableSetupColumn("To");
                    ImGui::TableSetupColumn("Group");
                    ImGui::TableSetupColumn("Prio");
                    ImGui::TableSetupColumn("Voice");
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < 4; ++i) {
                        const auto& load = snapshot.currentLoads[static_cast<std::size_t>(i)];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", i + 1);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(snapshot.channelActive[static_cast<std::size_t>(i)] ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", load.from);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", load.to);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextUnformatted(load.groupName.c_str());
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%d", load.groupPriority);
                        ImGui::TableSetColumnIndex(6);
                        ImGui::TextUnformatted(load.isClear ? "Clear" : "Encrypted");
                    }
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Calls")) {
                if (ImGui::BeginTable(("tetra_calls_" + self->name_).c_str(), 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Call");
                    ImGui::TableSetupColumn("From");
                    ImGui::TableSetupColumn("To");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Slot");
                    ImGui::TableSetupColumn("Carrier");
                    ImGui::TableHeadersRow();

                    for (const auto& call : snapshot.currentCalls) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", call.callID);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", call.from);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", call.to);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", call.type);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%d", call.assignedSlot);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%d", call.carrier);
                    }
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Neighbours")) {
                if (snapshot.neighbourData.empty()) {
                    ImGui::TextUnformatted("No neighbour cell information.");
                }
                else if (ImGui::BeginTable(("tetra_neighbours_" + self->name_).c_str(), 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Cell");
                    ImGui::TableSetupColumn("Carrier");
                    ImGui::TableSetupColumn("MCC");
                    ImGui::TableSetupColumn("MNC");
                    ImGui::TableSetupColumn("LA");
                    ImGui::TableSetupColumn("Service");
                    ImGui::TableHeadersRow();

                    for (const auto& neighbour : snapshot.neighbourData) {
                        int cell = 0;
                        int carrier = 0;
                        int mcc = 0;
                        int mnc = 0;
                        int la = 0;
                        int service = 0;
                        neighbour.tryGetValue(tetra::GlobalNames::Cell_identifier, cell);
                        neighbour.tryGetValue(tetra::GlobalNames::Main_carrier_number, carrier);
                        if (carrier == 0) {
                            neighbour.tryGetValue(tetra::GlobalNames::Carrier_number, carrier);
                        }
                        neighbour.tryGetValue(tetra::GlobalNames::Neighbour_MCC, mcc);
                        neighbour.tryGetValue(tetra::GlobalNames::Neighbour_MNC, mnc);
                        neighbour.tryGetValue(tetra::GlobalNames::Neighbour_LA, la);
                        neighbour.tryGetValue(tetra::GlobalNames::Neighbour_cell_service_level, service);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", cell);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", carrier);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", mcc);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", mnc);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%d", la);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%d", service);
                    }
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Recent Events")) {
                if (snapshot.recentEvents.empty()) {
                    ImGui::TextUnformatted("No CMCE/SDS events yet.");
                }
                else {
                    const int start = std::max(0, static_cast<int>(snapshot.recentEvents.size()) - 20);
                    for (int i = static_cast<int>(snapshot.recentEvents.size()) - 1; i >= start; --i) {
                        ImGui::TextWrapped("%s", snapshot.recentEvents[static_cast<std::size_t>(i)].c_str());
                    }
                }
            }

            if (ImGui::CollapsingHeader("Groups")) {
                if (ImGui::BeginTable(("tetra_groups_" + self->name_).c_str(), 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("GSSI");
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Priority");
                    ImGui::TableHeadersRow();

                    for (const auto& group : snapshot.activeGroups) {
                        const auto key = groupKey(group.nmi, group.gssi);
                        auto& buffer = self->groupNameBuffers_[key];
                        if (!buffer.initialized) {
                            std::snprintf(buffer.text.data(), buffer.text.size(), "%s", group.name.c_str());
                            buffer.initialized = true;
                        }

                        ImGui::PushID(static_cast<int>(key & 0x7fffffff));
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", group.gssi);
                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::InputText("##name", buffer.text.data(), buffer.text.size())) {
                            self->runtime_.setGroupMetadata(group.gssi, buffer.text.data(), group.priority);
                            self->saveConfig();
                        }
                        ImGui::TableSetColumnIndex(2);
                        int priority = group.priority;
                        if (ImGui::InputInt("##priority", &priority, 1, 5)) {
                            self->runtime_.setGroupMetadata(group.gssi, buffer.text.data(), priority);
                            self->saveConfig();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Logging")) {
                if (ImGui::Checkbox(("Enable Log##tetra_log_" + self->name_).c_str(), &self->settings_.logEnabled)) {
                    self->syncSettingsFromBuffers();
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
                if (ImGui::InputText(("Folder##tetra_log_folder_" + self->name_).c_str(), self->logFolderBuffer_.data(), self->logFolderBuffer_.size())) {
                    self->syncSettingsFromBuffers();
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
                if (ImGui::InputText(("File Rules##tetra_log_file_rules_" + self->name_).c_str(), self->logFileRulesBuffer_.data(), self->logFileRulesBuffer_.size())) {
                    self->syncSettingsFromBuffers();
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
                if (ImGui::InputText(("Entry Rules##tetra_log_entry_rules_" + self->name_).c_str(), self->logEntryRulesBuffer_.data(), self->logEntryRulesBuffer_.size())) {
                    self->syncSettingsFromBuffers();
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
                if (ImGui::InputText(("Separator##tetra_log_sep_" + self->name_).c_str(), self->logSeparatorBuffer_.data(), self->logSeparatorBuffer_.size())) {
                    self->syncSettingsFromBuffers();
                    self->runtime_.setSettings(self->settings_);
                    self->saveConfig();
                }
                ImGui::TextWrapped("Preview Entry: %s", self->runtime_.previewLogEntry().c_str());
                ImGui::TextWrapped("Preview Path: %s", self->runtime_.previewLogPath().c_str());
            }

            if (!self->enabled_.load()) {
                style::endDisabled();
            }
        }

        void loadConfig() {
            config.acquire();
            auto& cfg = config.conf[name_];
            vfoBandwidth_ = cfg.contains("bandwidth") ? std::clamp<double>(cfg["bandwidth"].get<double>(), MIN_VFO_BANDWIDTH, MAX_VFO_BANDWIDTH) : DEFAULT_VFO_BANDWIDTH;
            vfoSampleRate_.store(cfg.contains("sampleRate") ? std::clamp<double>(cfg["sampleRate"].get<double>(), MIN_VFO_SAMPLE_RATE, MAX_VFO_SAMPLE_RATE) : DEFAULT_VFO_SAMPLE_RATE);

            settings_.autoPlay = cfg.contains("autoPlay") ? cfg["autoPlay"].get<bool>() : true;
            settings_.ignoreEncryptedSpeech = cfg.contains("ignoreEncryptedSpeech") ? cfg["ignoreEncryptedSpeech"].get<bool>() : false;
            settings_.udpEnabled = cfg.contains("udpEnabled") ? cfg["udpEnabled"].get<bool>() : false;
            settings_.udpPort = cfg.contains("udpPort") ? std::clamp<int>(cfg["udpPort"].get<int>(), 1, 65535) : 20025;
            settings_.afcDisabled = cfg.contains("afcDisabled") ? cfg["afcDisabled"].get<bool>() : false;
            settings_.blockedLevel = cfg.contains("blockedLevel") ? cfg["blockedLevel"].get<int>() : 0;
            settings_.logEnabled = cfg.contains("logEnabled") ? cfg["logEnabled"].get<bool>() : false;
            settings_.logWriteFolder = cfg.contains("logWriteFolder") ? cfg["logWriteFolder"].get<std::string>() : (core::args["root"].s() + "/tetra_logs");
            settings_.logEntryRules = cfg.contains("logEntryRules") ? cfg["logEntryRules"].get<std::string>() : settings_.logEntryRules;
            settings_.logFileNameRules = cfg.contains("logFileNameRules") ? cfg["logFileNameRules"].get<std::string>() : settings_.logFileNameRules;
            settings_.logSeparator = cfg.contains("logSeparator") ? cfg["logSeparator"].get<std::string>() : settings_.logSeparator;

            std::vector<tetra::KnownGroup> knownGroups;
            if (cfg.contains("knownGroups")) {
                for (const auto& item : cfg["knownGroups"]) {
                    knownGroups.push_back(tetra::KnownGroup{
                        item.value("nmi", 0),
                        item.value("gssi", 0),
                        item.value("name", std::string{}),
                        item.value("priority", 0)
                    });
                }
            }
            config.release();

            std::snprintf(logFolderBuffer_.data(), logFolderBuffer_.size(), "%s", settings_.logWriteFolder.c_str());
            std::snprintf(logEntryRulesBuffer_.data(), logEntryRulesBuffer_.size(), "%s", settings_.logEntryRules.c_str());
            std::snprintf(logFileRulesBuffer_.data(), logFileRulesBuffer_.size(), "%s", settings_.logFileNameRules.c_str());
            std::snprintf(logSeparatorBuffer_.data(), logSeparatorBuffer_.size(), "%s", settings_.logSeparator.c_str());
            runtime_.setSettings(settings_);
            runtime_.importKnownGroups(knownGroups);
        }

        void saveConfig() {
            syncSettingsFromBuffers();

            config.acquire();
            auto& cfg = config.conf[name_];
            cfg["bandwidth"] = vfoBandwidth_;
            cfg["sampleRate"] = vfoSampleRate_.load();
            cfg["autoPlay"] = settings_.autoPlay;
            cfg["ignoreEncryptedSpeech"] = settings_.ignoreEncryptedSpeech;
            cfg["udpEnabled"] = settings_.udpEnabled;
            cfg["udpPort"] = settings_.udpPort;
            cfg["afcDisabled"] = settings_.afcDisabled;
            cfg["blockedLevel"] = settings_.blockedLevel;
            cfg["logEnabled"] = settings_.logEnabled;
            cfg["logWriteFolder"] = settings_.logWriteFolder;
            cfg["logEntryRules"] = settings_.logEntryRules;
            cfg["logFileNameRules"] = settings_.logFileNameRules;
            cfg["logSeparator"] = settings_.logSeparator;
            cfg["knownGroups"] = json::array();

            for (const auto& group : runtime_.exportKnownGroups()) {
                cfg["knownGroups"].push_back({
                    { "nmi", group.nmi },
                    { "gssi", group.gssi },
                    { "name", group.name },
                    { "priority", group.priority }
                });
            }

            config.release(true);
        }

        void createVfo(double offset) {
            if (vfo_ != nullptr) {
                return;
            }
            vfo_ = sigpath::vfoManager.createVFO(
                name_,
                ImGui::WaterfallVFO::REF_CENTER,
                offset,
                vfoBandwidth_,
                vfoSampleRate_.load(),
                MIN_VFO_BANDWIDTH,
                MAX_VFO_BANDWIDTH,
                false
            );
            vfo_->setSnapInterval(VFO_SNAP_INTERVAL);
        }

        void destroyVfo() {
            if (vfo_ == nullptr) {
                return;
            }
            sigpath::vfoManager.deleteVFO(vfo_);
            vfo_ = nullptr;
        }

        void applyVfoSettings() {
            if (vfo_ == nullptr) {
                return;
            }
            vfo_->setBandwidth(vfoBandwidth_);
            vfo_->setSampleRate(vfoSampleRate_.load(), vfoBandwidth_);
        }

        std::string name_;
        std::atomic<bool> enabled_{ false };
        tetra::TetraSettings settings_{};
        double vfoBandwidth_ = DEFAULT_VFO_BANDWIDTH;
        std::atomic<double> vfoSampleRate_{ DEFAULT_VFO_SAMPLE_RATE };
        tetra::TetraRuntime runtime_;
        VFOManager::VFO* vfo_ = nullptr;
        dsp::sink::Handler<dsp::complex_t> iqMonitor_;
        std::array<char, LOG_FOLDER_BUFFER_SIZE> logFolderBuffer_{};
        std::array<char, LOG_RULE_BUFFER_SIZE> logEntryRulesBuffer_{};
        std::array<char, LOG_RULE_BUFFER_SIZE> logFileRulesBuffer_{};
        std::array<char, LOG_SEPARATOR_BUFFER_SIZE> logSeparatorBuffer_{};
        std::map<long long, GroupNameBuffer> groupNameBuffers_;
        std::atomic<long long> lastTunedFrequency_{ 0 };
        std::atomic<bool> ignoreNextTuneReset_{ false };
    };
}

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/tetra_decoder_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new TetraDecoderModule(std::move(name));
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete static_cast<TetraDecoderModule*>(instance);
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
