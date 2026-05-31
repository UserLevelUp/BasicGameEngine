#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

// trace-id: lane.asteroids-arcade-fidelity.audio-plugin-skeleton
// parent:   lane.asteroids-arcade-fidelity
//
// Engine-neutral audio piece plugin. Provides `sound define` and
// `sound play` recipe commands. A DICOM alert plugin, a business-rules
// audible-fire-signal plugin, or a hardware-debug heartbeat plugin
// could all consume the same primitive via `sound play <id>`.

class BgeAudioPluginOperation : public IOperate {
public:
    BgeAudioPluginOperation() = default;
    ~BgeAudioPluginOperation() override = default;

    std::string Symbol() const override { return "BgeAudioPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.audio", "enabled");
        node->SetAttribute("plugin.bge.piece.audio.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.audio.command", "sound define | sound play");
        node->SetAttribute("plugin.bge.piece.audio.flags", "--id --kind --freq --duration --volume");
        node->SetAttribute("plugin.bge.piece.audio.emits", "bge.event.sound.defined bge.event.sound.played");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeAudioPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.audio=enabled\n";
        log << "  command=sound define | sound play\n";
    }
};
