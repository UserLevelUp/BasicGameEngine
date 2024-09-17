#include "pch.h"
#include "CommandHistoryOperation.h"
#include <iostream>
#include <functional>


CommandHistoryOperation::CommandHistoryOperation(int maxDepth)
    : symbol_("CH"), maxDepth_(maxDepth), cursorPosition_(0), currentHistory_(nullptr), currentDepth_(0) {}

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
    // Ensure the command node is valid
    if (!commandNode) {
        std::cerr << "Error: Invalid command node provided." << std::endl;
        return;
    }

    // Check if we're appending or inserting a new command
    bool isAppending = (cursorPosition_ == static_cast<int>(commandEntries_.size()) - 1) || commandEntries_.empty();

    if (!isAppending) {
        // Check index validity before accessing
        if (cursorPosition_ < 0 || cursorPosition_ >= static_cast<int>(commandEntries_.size())) {
            std::cerr << "Error: cursorPosition_ is out of bounds!" << std::endl;
            return;
        }

        // Inserting a command: create a new CommandHistoryOperation node
        auto newBranch = std::make_shared<CommandHistoryOperation>(maxDepth_);
        newBranch->AddCommand(commandNode);  // Add the command to the new branch

        // Link the new branch as a child of the current history at the correct position
        commandEntries_[cursorPosition_].childCommandHistory = newBranch;

        // Update current history and depth
        currentHistory_ = newBranch;
        currentDepth_++;
        std::cout << "currentDepth is incremented: " << currentDepth_;

        // Move the cursor to the new leaf node
        MoveCursorToEnd();
    }
    else {
        // Appending a command: simply add it to the current leaf
        commandEntries_.emplace_back(CommandEntry{ commandNode, CommandState::Executed, nullptr });
        cursorPosition_ = static_cast<int>(commandEntries_.size()) - 1;  // Update the cursor to the end

        std::cout << "Appended a command. Current command count: " << commandEntries_.size()
            << ", cursorPosition_: " << cursorPosition_ << std::endl;
    }

    // Enforce depth constraints
    if (currentDepth_ > maxDepth_) {
        auto newHistory = CreateNewHistoryIfNeeded();
        if (newHistory) {
            newHistory->SetAttribute("NewHistory", "True");
            newHistory->SetAttribute("Justification", "MaxDepthReached");
            commandEntries_.back().childCommandHistory = newHistory;
            currentHistory_ = newHistory;
            currentDepth_ = 1;  // Reset depth for the new branch
        }
    }
}

bool CommandHistoryOperation::Undo() {
    if (cursorPosition_ > 0) {
        commandEntries_[cursorPosition_].state = CommandState::Undone;
        std::cout << "Undoing command: " << commandEntries_[cursorPosition_].node->GetName() << std::endl;
        MoveCursorUp();
        return true;
    }
    return false;
}

bool CommandHistoryOperation::Redo() {
    if (cursorPosition_ < static_cast<int>(commandEntries_.size()) - 1) {
        MoveCursorDown();
        commandEntries_[cursorPosition_].state = CommandState::Executed;
        std::cout << "Redoing command: " << commandEntries_[cursorPosition_].node->GetName() << std::endl;
        return true;
    }
    return false;
}

void CommandHistoryOperation::MoveCursorToEnd() {
    cursorPosition_ = static_cast<int>(commandEntries_.size()) - 1;
}

void CommandHistoryOperation::MoveCursorUp() {
    if (cursorPosition_ > 0) {
        cursorPosition_--;
        std::cout << "cursorPosition is decremented and currentDepth is " << currentDepth_;
    }
    else {
        std::cerr << "Warning: Attempted to move cursor up but cursorPosition_ is already at the beginning." << std::endl;
    }
}

void CommandHistoryOperation::MoveCursorDown() {
    if (cursorPosition_ < static_cast<int>(commandEntries_.size()) - 1) {
        cursorPosition_++;
        std::cout << "cursorPosition is incremented and currentDepth is " << currentDepth_;
    }
}

bool CommandHistoryOperation::IsAtLeafNode() const {
    return (cursorPosition_ == static_cast<int>(commandEntries_.size()) - 1);
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
    if (currentDepth_ > maxDepth_) {
        auto newSiblingHistory = std::make_shared<CommandHistoryOperation>(maxDepth_);
        newSiblingHistory->SetAttribute("NewHistory", "True");
        newSiblingHistory->SetAttribute("Justification", "MaxDepthReached");
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

void CommandHistoryOperation::TraverseCommands(std::function<void(const std::shared_ptr<OpNode>&)> visitor) const {
    for (const auto& entry : commandEntries_) {
        visitor(entry.node);
        if (entry.childCommandHistory) {
            entry.childCommandHistory->TraverseCommands(visitor);
        }
    }
}

int CalculateDepth(const CommandHistoryOperation& history) {
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

    return maxDepth;
}

extern "C" COMMANDHISTORYOPERATION_API IOperate* CreateInstance() {
    return new CommandHistoryOperation();
}