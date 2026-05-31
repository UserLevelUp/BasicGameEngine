#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BgeVectorShipPluginOperation : public IOperate {
public:
    BgeVectorShipPluginOperation() = default;
    ~BgeVectorShipPluginOperation() override = default;

    std::string Symbol() const override { return "BgeVectorShipPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.vector-ship", "enabled");
        node->SetAttribute("plugin.bge.piece.vector-ship.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.vector-ship.command", "player-ship create");
        node->SetAttribute("plugin.bge.piece.vector-ship.flags", "--id --shape --lives --input-profile --fire --hyperspace --respawn --invulnerable");
        node->SetAttribute("plugin.bge.piece.vector-ship.emits", "bge.event.entity.spawned bge.event.input.bound bge.event.player.respawned");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeVectorShipPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.vector-ship=enabled\n";
        log << "  command=player-ship create\n";
    }
};