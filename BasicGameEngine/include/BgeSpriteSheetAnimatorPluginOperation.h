#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

// Engine-neutral sprite-sheet animator piece.
// Slice 2 scope: registry/signature surface and command skeleton wiring.

class BgeSpriteSheetAnimatorPluginOperation : public IOperate {
public:
    BgeSpriteSheetAnimatorPluginOperation() = default;
    ~BgeSpriteSheetAnimatorPluginOperation() override = default;

    std::string Symbol() const override { return "BgeSpriteSheetAnimatorPluginOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.bge.piece.sprite-sheet-animator", "enabled");
        node->SetAttribute("plugin.bge.piece.sprite-sheet-animator.operation", Symbol());
        node->SetAttribute("plugin.bge.piece.sprite-sheet-animator.command", "sprite-sheet create | animation-sequence create | actor animation bind");
        node->SetAttribute("plugin.bge.piece.sprite-sheet-animator.flags", "--id --image --columns --rows --frame-size --alpha-source --alpha-cutoff --preserve-alpha --no-preserve-alpha --transparent-background --opaque-background --frames --fps --loop --tier-counter --map --show --color");
        node->SetAttribute("plugin.bge.piece.sprite-sheet-animator.emits", "bge.event.sprite.sheet.defined bge.event.sprite.sequence.defined bge.event.sprite.binding.defined");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BgeSpriteSheetAnimatorPluginOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.bge.piece.sprite-sheet-animator=enabled\n";
        log << "  command=sprite-sheet create | animation-sequence create | actor animation bind\n";
    }
};
