#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BgeTitleScreenPluginOperation : public IOperate {
public:
    BgeTitleScreenPluginOperation() = default;
    ~BgeTitleScreenPluginOperation() override = default;

    std::string Symbol() const override { return "BgeTitleScreenPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.title-screen", "enabled");
        node->SetAttribute("plugin.bge.piece.title-screen.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.title-screen.command", "title-screen create");
        node->SetAttribute("plugin.bge.piece.title-screen.flags", "--text --subtitle --start --next --font --center --legend --credit --blink");
        node->SetAttribute("plugin.bge.piece.title-screen.emits", "bge.event.screen.defined bge.event.command.bound");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeTitleScreenPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.title-screen=enabled\n";
        log << "  command=title-screen create\n";
    }
};