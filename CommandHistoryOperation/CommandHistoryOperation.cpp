#include "pch.h"
#include "CommandHistoryOperation.h"
#include <iostream>
#include <functional>
#include <algorithm>
#include <stdexcept>


CommandHistoryOperation::CommandHistoryOperation(int maxDepth)
    : symbol_("CH"), maxDepth_(maxDepth), cursorPosition_(-1), currentHistory_(nullptr), branchPosition_(-1), currentDepth_(1) {
    if (maxDepth < 1) {
        throw std::invalid_argument("History depth must be at least one.");
    }
    commandEntries_.reserve(CollectionCapacity);
}

std::string CommandHistoryOperation::Symbol() const {
    return symbol_;
}

void CommandHistoryOperation::Operate(std::shared_ptr<OpNode> node) {
    if (!node) return;

    std::cout << "Operating on OpNode: " << node->GetName() << std::endl;
    for (const auto& child : node->GetChildren()) {
        std::cout << "Child Node: " << child->GetName() << std::endl;
    }
}

void CommandHistoryOperation::AddCommand(const std::shared_ptr<OpNode>& commandNode) {
    if (!commandNode) {
        throw std::invalid_argument("Command node must not be null.");
    }

    CommandAction action;
    const auto commandType = commandNode->GetValue("CommandType");
    if (!commandType.empty()) {
        auto factory = commandFactories_.find(commandType);
        if (factory == commandFactories_.end()) {
            throw std::invalid_argument("Unknown command type: " + commandType);
        }
        action = factory->second(commandNode);
        if (!action.execute || !action.undo) {
            throw std::invalid_argument("Executable commands require execute and undo actions.");
        }
    }
    AddPreparedCommand(commandNode, action);
}

void CommandHistoryOperation::RegisterCommand(const std::string& commandType, CommandFactory factory) {
    if (commandType.empty() || !factory) {
        throw std::invalid_argument("Command registration requires a type and factory.");
    }
    commandFactories_[commandType] = std::move(factory);
}

void CommandHistoryOperation::AddPreparedCommand(const std::shared_ptr<OpNode>& commandNode, const CommandAction& action) {
    if (currentHistory_ && cursorPosition_ >= branchPosition_) {
        currentHistory_->AddPreparedCommand(commandNode, action);
    }
    else if (cursorPosition_ + 1 < static_cast<int>(commandEntries_.size()) && maxDepth_ > 1) {
        auto newBranch = std::make_shared<CommandHistoryOperation>(maxDepth_ - 1);
        newBranch->commandFactories_ = commandFactories_;
        newBranch->AddPreparedCommand(commandNode, action);
        branchPosition_ = cursorPosition_;
        commandEntries_[cursorPosition_ + 1].childCommandHistory = newBranch;
        currentHistory_ = newBranch;
    }
    else if (commandEntries_.size() == CollectionCapacity && cursorPosition_ == static_cast<int>(CollectionCapacity) - 1) {
        auto next = nextCollection_ ? nextCollection_ : CreateNewHistoryIfNeeded();
        next->AddPreparedCommand(commandNode, action);
        nextCollection_ = next;
    }
    else {
        if (action.execute) {
            action.execute();
        }
        // At the depth limit, replace only the undone suffix rather than nesting another branch.
        commandEntries_.erase(commandEntries_.begin() + (cursorPosition_ + 1), commandEntries_.end());
        nextCollection_.reset();
        currentHistory_.reset();
        commandEntries_.push_back(CommandEntry{ commandNode, CommandState::Executed, nullptr, action });
        cursorPosition_ = static_cast<int>(commandEntries_.size()) - 1;
    }
    currentDepth_ = CalculateDepth(*this);
}

bool CommandHistoryOperation::Undo() {
    if (currentHistory_ && cursorPosition_ >= branchPosition_ && currentHistory_->Undo()) {
        return true;
    }
    if (!currentHistory_ && nextCollection_ && cursorPosition_ == static_cast<int>(commandEntries_.size()) - 1 && nextCollection_->Undo()) {
        return true;
    }
    if (cursorPosition_ >= 0) {
        auto& entry = commandEntries_[cursorPosition_];
        if (entry.action.undo) {
            entry.action.undo();
        }
        entry.state = CommandState::Undone;
        --cursorPosition_;
        return true;
    }
    return false;
}

bool CommandHistoryOperation::Redo() {
    if (currentHistory_ && cursorPosition_ >= branchPosition_) {
        return currentHistory_->Redo();
    }
    if (cursorPosition_ < static_cast<int>(commandEntries_.size()) - 1) {
        auto& entry = commandEntries_[cursorPosition_ + 1];
        if (entry.action.execute) {
            entry.action.execute();
        }
        entry.state = CommandState::Executed;
        ++cursorPosition_;
        return true;
    }
    return !currentHistory_ && nextCollection_ && nextCollection_->Redo();
}

void CommandHistoryOperation::MoveCursorToEnd() {
    while (Redo()) {}
}

void CommandHistoryOperation::MoveCursorUp() {
    Undo();
}

void CommandHistoryOperation::MoveCursorDown() {
    Redo();
}

bool CommandHistoryOperation::IsAtLeafNode() const {
    if (currentHistory_) {
        return cursorPosition_ >= branchPosition_ && currentHistory_->IsAtLeafNode();
    }
    return cursorPosition_ == static_cast<int>(commandEntries_.size()) - 1
        && (!nextCollection_ || nextCollection_->IsAtLeafNode());
}

void CommandHistoryOperation::CleanUpOldHistory() {
    if (currentDepth_ > maxDepth_) {
        // Limit the depth of CommandHistory by cleaning up old histories
        commandEntries_.erase(commandEntries_.begin());
        currentDepth_--;
        std::cout << "currentDepth is decreemented: " << currentDepth_;
    }
}

std::shared_ptr<CommandHistoryOperation> CommandHistoryOperation::CreateNewHistoryIfNeeded() {
    if (commandEntries_.size() == CollectionCapacity) {
        if (nextCollection_) {
            return nextCollection_;
        }
        auto newSiblingHistory = std::make_shared<CommandHistoryOperation>(maxDepth_);
        newSiblingHistory->commandFactories_ = commandFactories_;
        newSiblingHistory->SetAttribute("NewHistory", "True");
        newSiblingHistory->SetAttribute("Justification", "CollectionCapacityReached");
        return newSiblingHistory;  // Return the new instance
    }
    return nullptr;
}

int CommandHistoryOperation::GetCurrentDepth() const {
    return currentDepth_;
}

void CommandHistoryOperation::SetAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
}

const std::map<std::string, std::string>& CommandHistoryOperation::GetAttributes() const {
    return attributes_;
}

const std::vector<CommandEntry>& CommandHistoryOperation::GetCommandEntries() const {
    return commandEntries_;
}

const std::shared_ptr<CommandHistoryOperation>& CommandHistoryOperation::GetNextCollection() const {
    return nextCollection_;
}

void CommandHistoryOperation::TraverseCommands(std::function<void(const std::shared_ptr<OpNode>&)> visitor) const {
    for (const auto& entry : commandEntries_) {
        visitor(entry.node);
        if (entry.childCommandHistory) {
            entry.childCommandHistory->TraverseCommands(visitor);
        }
    }
    if (nextCollection_) {
        nextCollection_->TraverseCommands(visitor);
    }
}

int CommandHistoryOperation::CalculateDepth(const CommandHistoryOperation& history) const {
    // Base depth is 1 for the current level
    int maxDepth = 1;

    const auto& entries = history.GetCommandEntries();
    for (const auto& entry : entries) {
        if (entry.childCommandHistory) {
            // Recursively calculate the depth of each child history
            int childDepth = 1 + CalculateDepth(*entry.childCommandHistory);
            if (childDepth > maxDepth) {
                maxDepth = childDepth;
            }
        }
    }
            
    if (history.GetNextCollection()) {
        maxDepth = (std::max)(maxDepth, CalculateDepth(*history.GetNextCollection()));
    }
    return maxDepth;
}

void CommandHistoryOperation::MarkCommandAsDeleted(const std::shared_ptr<OpNode>& commandNode) {
    if (!commandNode) {
        std::cerr << "Error: Invalid command node." << std::endl;
        return;
    }

    // Set an attribute on the command node to mark it as deleted
    commandNode->SetAttribute("ulu:deleted", "true");
    std::cout << "Command '" << commandNode->GetName() << "' marked as deleted." << std::endl;
}


extern "C" COMMANDHISTORYOPERATION_API IOperate* CreateInstance() {
    return new CommandHistoryOperation();
}