#ifndef COMMANDHISTORYOPERATION_H
#define COMMANDHISTORYOPERATION_H

#include "..\OpNode\IOperate.h"  // Ensure correct include path to IOperate
#include "..\OpNode\OpNode.h"  // Ensure correct include path to IOperate
#include <vector>
#include <memory>
#include <string>

// Define the export macro for Windows DLLs
#ifdef COMMANDHISTORYOPERATION_EXPORTS
#define COMMANDHISTORYOPERATION_API __declspec(dllexport)
#else
#define COMMANDHISTORYOPERATION_API __declspec(dllimport)
#endif

// CommandHistoryOperation class that implements the IOperate interface
class COMMANDHISTORYOPERATION_API CommandHistoryOperation : public IOperate {
public:
    CommandHistoryOperation();

    // Implementation of the Symbol method from the IOperate interface
    std::string Symbol() const override;

    // Implementation of the Operate method from the IOperate interface
    void Operate(std::shared_ptr<OpNode> node) override;

    // Adds a command to the history
    void AddCommand(const std::string& command);

    // Undo the last command
    bool Undo();

    // Redo the last undone command
    bool Redo();

private:
    std::string symbol_;                    // Symbol representing the operation type
    std::vector<std::string> commands_;     // History of commands
    int currentIndex_;                      // Current index in the command history
};

// Factory function to create an instance of CommandHistoryOperation
extern "C" COMMANDHISTORYOPERATION_API IOperate* CreateInstance();

#endif // COMMANDHISTORYOPERATION_H
