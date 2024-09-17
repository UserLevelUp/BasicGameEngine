#include "pch.h"
#include "CppUnitTest.h"
#include "../CommandHistoryOperation/CommandHistoryOperation.h"  // Adjust path as needed
#include <Windows.h>  // Include Windows API header for LoadLibrary, FreeLibrary
#include <memory>
#include <iostream>
#include <filesystem>
#include "../OpNode/IOperate.h"  // Include IOperate from OpNode project

using namespace Microsoft::VisualStudio::CppUnitTestFramework; // Ensure this is included

// Define the CalculateDepth function before using it
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

// Namespace declaration should be at file scope
namespace BasicGameEngine_UnitTests
{
    TEST_CLASS(CommandHistoryOperationTests)
    {
    public:
        TEST_METHOD(TestAddCommandAndUndoRedoWithBranching)
        {
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);  // Max depth of 10

            // Act - Add commands and undo
            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);

            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            commandHistory->Undo();  // Undo last command

            // Add new command after undo, creating a new branch
            auto command3 = std::make_shared<OpNode>("Command3");
            commandHistory->AddCommand(command3);

            // Assert - Verify branching behavior
            Assert::IsNotNull(commandHistory.get(), L"CommandHistoryOperation should not be null after branching.");

            // Check if there is at least one Executed state
            const auto& entries = commandHistory->GetCommandEntries();
            bool foundExecuted = false;
            for (const auto& entry : entries) {
                if (entry.state == CommandState::Executed) {
                    foundExecuted = true;
                    break;
                }
            }
            Assert::IsTrue(foundExecuted, L"At least one Executed command should be present.");
        }

        TEST_METHOD(TestMaxDepthEnforcement)
        {
            auto commandHistory = std::make_shared<CommandHistoryOperation>(2);  // Set max depth to 2

            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);

            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            commandHistory->Undo();  // Undo the last command

            auto command3 = std::make_shared<OpNode>("Command3");
            commandHistory->AddCommand(command3);  // Creates the first branching level

            commandHistory->Undo();  // Undo to create a potential second branching

            auto command4 = std::make_shared<OpNode>("Command4");
            commandHistory->AddCommand(command4);  // Creates the second branching level

            commandHistory->Undo();  // Undo again to potentially exceed max depth

            auto command5 = std::make_shared<OpNode>("Command5");
            commandHistory->AddCommand(command5);  // Attempt to exceed the max depth

            // Assert - Verify that we do not exceed the max depth
            int actualDepth = CalculateDepth(*commandHistory);
            Assert::AreEqual(2, actualDepth, L"The depth of CommandHistoryOperation should not exceed the max depth of 2.");
        }
    };
}
