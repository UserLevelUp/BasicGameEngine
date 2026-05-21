#pragma once

#include <fstream>
#include <memory>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class DirectX11BouncingBallOperation : public IOperate {
public:
    DirectX11BouncingBallOperation() = default;
    ~DirectX11BouncingBallOperation() override = default;

    std::string Symbol() const override { return "DirectX11BouncingBallOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        node->SetAttribute("plugin.directx11", "enabled");
        node->SetAttribute("renderer.api", "directx11");
        node->SetAttribute("animation.2d.bouncing-ball", "enabled");

        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[DirectX11BouncingBallOperation] node='" << node->GetName() << "'\n";
        log << "  plugin.directx11=enabled\n";
        log << "  renderer.api=directx11\n";
        log << "  animation.2d.bouncing-ball=enabled\n";
    }
};