#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BgeBreakableRockFieldPluginOperation : public IOperate {
public:
    BgeBreakableRockFieldPluginOperation() = default;
    ~BgeBreakableRockFieldPluginOperation() override = default;

    std::string Symbol() const override { return "BgeBreakableRockFieldPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.breakable-rock-field", "enabled");
        node->SetAttribute("plugin.bge.piece.breakable-rock-field.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.breakable-rock-field.command", "rock-field create");
        node->SetAttribute("plugin.bge.piece.breakable-rock-field.flags", "--sizes --count --split --score --speed-range --wave-counter --avoid");
        node->SetAttribute("plugin.bge.piece.breakable-rock-field.emits", "bge.event.entity.spawned bge.event.rule.defined bge.event.counter.changed");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeBreakableRockFieldPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.breakable-rock-field=enabled\n";
        log << "  command=rock-field create\n";
    }
};