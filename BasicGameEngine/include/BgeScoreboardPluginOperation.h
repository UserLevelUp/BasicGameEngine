#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BgeScoreboardPluginOperation : public IOperate {
public:
    BgeScoreboardPluginOperation() = default;
    ~BgeScoreboardPluginOperation() override = default;

    std::string Symbol() const override { return "BgeScoreboardPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.scoreboard", "enabled");
        node->SetAttribute("plugin.bge.piece.scoreboard.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.scoreboard.command", "scoreboard create");
        node->SetAttribute("plugin.bge.piece.scoreboard.flags", "--counters --anchor --format --font --color --icon-row --scale");
        node->SetAttribute("plugin.bge.piece.scoreboard.emits", "bge.event.overlay.defined bge.event.counter.observed");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeScoreboardPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.scoreboard=enabled\n";
        log << "  command=scoreboard create\n";
    }
};