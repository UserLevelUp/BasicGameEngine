#include "pch.h"
#include "CommandHistoryOperation.h"
#include <iostream>

// Constructor
CommandHistoryOperation::CommandHistoryOperation() : symbol_("CH"), currentIndex_(-1) {}

// Implementation of the Symbol method from the IOperate interface
std::string CommandHistoryOperation::Symbol() const {
    return symbol_;
}

// Implementation of the Operate method from the IOperate interface
void CommandHistoryOperation::Operate(std::shared_ptr<OpNode> node) {
    if (!node) return;

    // Example operation: Print the node's name
    std::cout << "Operating on OpNode: " << node->GetName() << std::endl;

    // Perform operations on child nodes
    for (const auto& child : node->GetChildren()) {
        std::cout << "Child Node: " << child->GetName() << std::endl;
    }
}

// Adds a command to the history
void CommandHistoryOperation::AddCommand(const std::string& command) {
    commands_.push_back(command);
    currentIndex_ = commands_.size() - 1;
}

// Undo the last command
bool CommandHistoryOperation::Undo() {
    if (currentIndex_ > 0) {
        --currentIndex_;
        return true;
    }
    return false;
}

// Redo the last undone command
bool CommandHistoryOperation::Redo() {
    if (currentIndex_ < commands_.size() - 1) {
        ++currentIndex_;
        return true;
    }
    return false;
}

// Factory function to create an instance of CommandHistoryOperation
extern "C" COMMANDHISTORYOPERATION_API IOperate* CreateInstance() {
    return new CommandHistoryOperation();
}
