#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BgeProjectilePluginOperation : public IOperate {
public:
    BgeProjectilePluginOperation() = default;
    ~BgeProjectilePluginOperation() override = default;

    std::string Symbol() const override { return "BgeProjectilePluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.projectile", "enabled");
        node->SetAttribute("plugin.bge.piece.projectile.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.projectile.command", "projectile create");
        node->SetAttribute("plugin.bge.piece.projectile.flags", "--id --owner --shape --speed --ttl --wrap --collision-tag");
        node->SetAttribute("plugin.bge.piece.projectile.emits", "bge.event.entity.grammar.extended bge.event.entity.lifecycle.changed");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeProjectilePluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.projectile=enabled\n";
        log << "  command=projectile create\n";
    }
};