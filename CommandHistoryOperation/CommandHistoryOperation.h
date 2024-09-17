#ifndef COMMANDHISTORYOPERATION_H
#define COMMANDHISTORYOPERATION_H

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <map>
#include "../OpNode/IOperate.h"
#include "../OpNode/OpNode.h"

// Define export macros for DLL
#ifdef COMMANDHISTORYOPERATION_EXPORTS
#define COMMANDHISTORYOPERATION_API __declspec(dllexport)
#else
#define COMMANDHISTORYOPERATION_API __declspec(dllimport)
#endif

// Forward declarations
class CommandHistoryOperation;

enum class CommandState {
    Executed,
    Undone,
    Branched  // Indicates that this node is tracking a new branch after an undo
};

struct CommandEntry {
    std::shared_ptr<OpNode> node;
    CommandState state;
    std::shared_ptr<CommandHistoryOperation> childCommandHistory;  // New branch for each undo + new command sequence
};

class COMMANDHISTORYOPERATION_API CommandHistoryOperation : public IOperate, public std::enable_shared_from_this<CommandHistoryOperation> {
public:
    CommandHistoryOperation(int maxDepth = 10);  // Maximum depth for nodes that require tracking
    std::string Symbol() const override;
    void Operate(std::shared_ptr<OpNode> node) override;

    void AddCommand(const std::shared_ptr<OpNode>& commandNode);
    bool Undo();
    bool Redo();

    void CleanUpOldHistory();
    void TraverseCommands(std::function<void(const std::shared_ptr<OpNode>&)> visitor) const;

    // Metadata Management for BasicGameEngine
    void SetAttribute(const std::string& key, const std::string& value);
    const std::map<std::string, std::string>& GetAttributes() const;

    // Cursor management
    void MoveCursorToEnd();
    void MoveCursorUp();
    void MoveCursorDown();
    bool IsAtLeafNode() const;

    // Return a new CommandHistoryOperation marked with attributes
    std::shared_ptr<CommandHistoryOperation> CreateNewHistoryIfNeeded();

    // Get the current depth of the command history tree
    int GetCurrentDepth() const;

    // Corrected: Only one declaration for CalculateDepth
    int CalculateDepth(const CommandHistoryOperation& history) const;

    // Correct the method declaration for accessing entries
    const std::vector<CommandEntry>& GetCommandEntries() const;

private:
    std::string symbol_;
    std::vector<CommandEntry> commandEntries_;  // Tracks commands requiring history tracking
    int maxDepth_;  // Maximum depth for nodes with ICommandHistoryOperation
    int cursorPosition_;  // Track the current position of the cursor within the history
    std::map<std::string, std::string> attributes_;  // Store metadata attributes

    // Pointer to track the current command history position
    std::shared_ptr<CommandHistoryOperation> currentHistory_;
    int currentDepth_;  // Track the current depth level
};

extern "C" COMMANDHISTORYOPERATION_API IOperate* CreateInstance();

#endif // COMMANDHISTORYOPERATION_H
