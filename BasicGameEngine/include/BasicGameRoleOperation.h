#pragma once

#include <fstream>
#include <string>
#include "../../OpNode/IOperate.h"
#include "../../OpNode/OpNode.h"

class BasicGameRoleOperation : public IOperate {
public:
    BasicGameRoleOperation() = default;
    ~BasicGameRoleOperation() override = default;

    std::string Symbol() const override { return "BasicGameRoleOperation"; }

    void Operate(std::shared_ptr<OpNode> node) override {
        std::ofstream log("BasicGameEngine.log", std::ios::app);
        log << "[BasicGameRoleOperation] node='" << node->GetName() << "'\n";
        for (const auto& [key, value] : node->GetAttributes()) {
            log << "  " << key << "=" << value << "\n";
        }
    }
};