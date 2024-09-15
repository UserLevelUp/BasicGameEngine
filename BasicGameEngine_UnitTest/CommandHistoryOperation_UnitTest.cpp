#include "pch.h"
#include "CppUnitTest.h"
#include "../CommandHistoryOperation/CommandHistoryOperation.h"  // Adjust path as needed

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace BasicGameEngine_UnitTest
{
    TEST_CLASS(CommandHistoryOperationTests)
    {
    public:

        TEST_METHOD(TestCreateInstance)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>();

            // Act & Assert
            Assert::IsNotNull(commandHistory.get(), L"Instance creation failed.");
            Assert::AreEqual(std::string("CH"), commandHistory->Symbol(), L"Symbol mismatch.");
        }

        TEST_METHOD(TestAddCommand)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>();
            commandHistory->AddCommand("MoveForward");

            // Act
            commandHistory->AddCommand("MoveBackward");

            // Assert
            Assert::IsTrue(commandHistory->Undo(), L"Undo operation failed.");
            Assert::IsTrue(commandHistory->Redo(), L"Redo operation failed.");
        }

        TEST_METHOD(TestOperateOnOpNode)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>();
            auto node = std::make_shared<OpNode>("RootNode");

            // Act
            commandHistory->Operate(node);

            // Assert
            // This test ensures no exceptions are thrown during the operation
            Assert::IsTrue(true, L"Operate executed without exceptions.");
        }
    };
}
