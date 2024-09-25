#include "pch.h"
#include "CppUnitTest.h"
#include "../CommandHistoryOperation/CommandHistoryOperation.h"  // Adjust path as needed
#include <Windows.h>  // Include Windows API header for LoadLibrary, FreeLibrary
#include <memory>
#include <iostream>
#include <filesystem>
#include "../OpNode/IOperate.h"  // Include IOperate from OpNode project

using namespace Microsoft::VisualStudio::CppUnitTestFramework; // Ensure this is included

namespace Microsoft::VisualStudio::CppUnitTestFramework
{
    // Specialization for CommandState enum
    template<>
    std::wstring ToString<CommandState>(const CommandState& state)
    {
        switch (state) {
        case CommandState::Executed:
            return L"Executed";
        case CommandState::Undone:
            return L"Undone";
        case CommandState::Branched:
            return L"Branched";
        default:
            return L"Unknown";
        }
    }
}

// Define the CalculateDepth function before using it
static int CalculateDepth(const CommandHistoryOperation& history) {
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
            // add traits
            #define TEST_CATEGORY(categoryName) \
                Logger::WriteMessage("Category: " categoryName)

            TEST_CATEGORY(L"Undo/Redo");
            // Arrange
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

        TEST_METHOD(TestAddCommands)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);  // Max depth of 10

            // Act
            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);

            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            const auto& entries = commandHistory->GetCommandEntries();

            // Assert
            Assert::AreEqual(size_t(2), entries.size(), L"Command history should have 2 commands.");
            Assert::AreEqual(std::string("Command1"), entries[0].node->GetName(), L"The first command should be 'Command1'.");
            Assert::AreEqual(std::string("Command2"), entries[1].node->GetName(), L"The second command should be 'Command2'.");
        }

        TEST_METHOD(TestUndoCommand)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);

            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);
            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            // Act
            commandHistory->Undo();

            const auto& entries = commandHistory->GetCommandEntries();

            // Assert
            Assert::AreEqual(CommandState::Undone, entries[1].state, L"The second command should be 'Undone'.");
            Assert::AreEqual(CommandState::Executed, entries[0].state, L"The first command should remain 'Executed'.");
        }

        TEST_METHOD(TestRedoCommand)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);

            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);
            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            commandHistory->Undo();  // Undo the last command

            // Act
            commandHistory->Redo();  // Redo the undone command

            const auto& entries = commandHistory->GetCommandEntries();

            // Assert
            Assert::AreEqual(CommandState::Executed, entries[1].state, L"The second command should be 'Executed' after redo.");
        }

        TEST_METHOD(TestMaxDepthEnforcementSimple)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(2);  // Set max depth to 2

            // Act
            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);
            auto command2 = std::make_shared<OpNode>("Command2");
            commandHistory->AddCommand(command2);

            // Trigger depth handling
            auto command3 = std::make_shared<OpNode>("Command3");
            commandHistory->AddCommand(command3);

            // Assert
            Assert::IsTrue(commandHistory->GetCurrentDepth() <= 2, L"The current depth should not exceed the maximum depth of 2.");
        }

        TEST_METHOD(TestSoftDeletion)
        {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);

            auto command1 = std::make_shared<OpNode>("Command1");
            commandHistory->AddCommand(command1);
            commandHistory->MarkCommandAsDeleted(command1);

            const auto& entries = commandHistory->GetCommandEntries();

            // Assert
            Assert::AreEqual(std::string("true"), entries[0].node->GetAttribute("ulu:deleted"), L"Command1 should be marked as deleted.");
        }

        TEST_METHOD(TestNonExistentCommand) {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);

            // Act: Try adding a non-existent command
            try {
                auto nonExistentCommand = std::make_shared<OpNode>("NonExistentCommand");
                commandHistory->AddCommand(nonExistentCommand);  // Expect this to fail
                Assert::Fail(L"Expected an exception for non-existent command, but it was not thrown.");
            }
            catch (const std::exception& e) {
                // Assert: Expecting exception to be thrown
                Assert::IsTrue(true, L"Non-existent command correctly raised an exception.");
            }
        }

        TEST_METHOD(TestParentNamesToRoot) {
            // Arrange: Create a series of commands, each as a child of the previous
            auto root = std::make_shared<OpNode>("RootCommand");
            auto parent = std::make_shared<OpNode>("ParentCommand");
            auto child = std::make_shared<OpNode>("ChildCommand");

            // Set parent-child relationships
            root->AddChild(parent);
            parent->AddChild(child);

            // Act: Traverse up from child to root
            auto currentNode = child;
            std::string ancestry;
            while (currentNode) {
                ancestry = currentNode->GetName() + " -> " + ancestry;
                if (currentNode->GetChildren().empty()) {
                    currentNode = nullptr;  // End at root
                }
                else {
                    currentNode = currentNode->GetChildren().front();  // Traverse up
                }
            }

            // Assert: Verify ancestry trace
            Assert::AreEqual(std::string("ChildCommand -> ParentCommand -> RootCommand -> "), ancestry, L"Ancestry does not match expected path to root.");
        }

        TEST_METHOD(TestTransformationCommandWithMatrixAndColor) {
            // Arrange
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);

            // Create a transformation matrix with time, rotation, translation, and scale
            auto transformationMatrix = std::vector<std::vector<float>>{
                { 1, 0, 0, 0 },  // Rotation matrix placeholder
                { 0, 1, 0, 0 },  // Translation matrix placeholder
                { 0, 0, 1, 0 },  // Scale matrix placeholder
                { 0, 0, 0, 1 }   // Time component placeholder (could be a timestamp or duration)
            };

            // Define a color vector (e.g., RGBA format)
            auto colorVector = std::vector<float>{ 0.5, 0.2, 0.7, 1.0 };  // Example: Purple color with full opacity

            // Act: Add a transformation command with the matrix and color
            auto commandNode = std::make_shared<OpNode>("TransformationCommand");
            commandNode->AddAttribute("Matrix", "1,0,0,0;0,1,0,0;0,0,1,0;0,0,0,1");  // Matrix as a string for simplicity
            commandNode->AddAttribute("Color", "0.5,0.2,0.7,1.0");  // Color vector as a string

            commandHistory->AddCommand(commandNode);

            // Assert: Verify the matrix and color are stored correctly
            Assert::AreEqual(std::string("1,0,0,0;0,1,0,0;0,0,1,0;0,0,0,1"), commandNode->GetValue("Matrix"), L"Matrix attribute mismatch.");
            Assert::AreEqual(std::string("0.5,0.2,0.7,1.0"), commandNode->GetValue("Color"), L"Color attribute mismatch.");
        }

        TEST_METHOD(TestFutureTranslationMatrixWithTime) {
            // Arrange: Create a future placeholder command with metadata
            auto commandNode = std::make_shared<OpNode>("TranslationCommand");

            // Placeholder attributes for the test
            commandNode->AddAttribute("FutureFeature", "Translation with Time Component");
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 0; 0, 1, 0, 0; 0, 0, 1, 0; 0, 0, 0, 1]");
            commandNode->AddAttribute("TimeComponent", "10s");

            // Act: Add command node to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Force the test to fail with a message explaining the future functionality
            Assert::Fail(L"Placeholder test for future translation matrix with time component. Implement this when feature is ready.");
        }

        TEST_METHOD(TestFutureHandlingOfNonExistentCommands) {
            // Arrange: Create a future placeholder command for a non-existent operation
            auto commandNode = std::make_shared<OpNode>("NonExistentCommand");

            // Placeholder attributes for the test
            commandNode->AddAttribute("FutureFeature", "Handling non-existent commands gracefully");
            commandNode->AddAttribute("CommandType", "NonExistent");

            // Act: Add command node to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Force the test to fail with a message explaining the future functionality
            Assert::Fail(L"Placeholder test for handling non-existent commands. Implement this when feature is ready.");
        }

        TEST_METHOD(TestCombineStaticAndAnimatedArtBoards) {
            // Arrange: Create a future placeholder for combining static and animated art boards
            auto commandNode = std::make_shared<OpNode>("CombineArtBoards");

            // Placeholder attributes for combining static and animated art boards
            commandNode->AddAttribute("StaticBoard", "Board1");
            commandNode->AddAttribute("AnimatedBoard", "Board2");
            commandNode->AddAttribute("destination_board_name", "CombinedBoard");  // The name of the final combined board

            // Act: Add the combine art boards action to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for combining static and animated art boards with a destination board name. Implement this feature in future.");
        }

        TEST_METHOD(TestAddCircleToArtBoard) {
            // Arrange: Create a future placeholder command for drawing a circle
            auto commandNode = std::make_shared<OpNode>("DrawCircle");

            // Placeholder attributes for the circle
            commandNode->AddAttribute("ShapeType", "Circle");
            commandNode->AddAttribute("Center", "[0,0]");  // Center coordinates
            commandNode->AddAttribute("Radius", "50");  // Radius in pixels
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 1]");  // Identity matrix for now

            // Act: Add command node to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure, indicating future implementation
            Assert::Fail(L"Placeholder test for adding a circle to the art board. Implement this feature in future.");
        }


        TEST_METHOD(TestAddLineToArtBoard) {
            // Arrange: Create a future placeholder command for drawing a line
            auto commandNode = std::make_shared<OpNode>("DrawLine");

            // Placeholder attributes for the line
            commandNode->AddAttribute("ShapeType", "Line");
            commandNode->AddAttribute("Start", "[0,0]");  // Starting point
            commandNode->AddAttribute("End", "[100,100]");  // End point
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 1]");  // Identity matrix for now

            // Act: Add command node to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for adding a line to the art board. Implement this feature in future.");
        }


        TEST_METHOD(TestAddRectangleToArtBoard) {
            // Arrange: Create a future placeholder command for drawing a rectangle
            auto commandNode = std::make_shared<OpNode>("DrawRectangle");

            // Placeholder attributes for the rectangle
            commandNode->AddAttribute("ShapeType", "Rectangle");
            commandNode->AddAttribute("TopLeft", "[0,0]");
            commandNode->AddAttribute("Dimensions", "[100, 50]");  // Width and height
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 1]");  // Identity matrix for now

            // Act: Add command node to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for adding a rectangle to the art board. Implement this feature in future.");
        }


        TEST_METHOD(TestAddAnimationTick) {
            // Arrange: Create a future placeholder command for adding an animation tick
            auto commandNode = std::make_shared<OpNode>("AddAnimationTick");

            // Placeholder attributes for the animation tick
            commandNode->AddAttribute("TickNumber", "1");  // First tick
            commandNode->AddAttribute("Shape", "Circle");  // Animating a circle
            commandNode->AddAttribute("Position", "[0, 0]");  // Starting position
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 1]");  // Identity matrix for now

            // Act: Add the animation tick to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for adding an animation tick. Implement this feature in future.");
        }


        TEST_METHOD(TestMultipleAnimationSteps) {
            // Arrange: Create a future placeholder command for animating multiple steps
            auto commandNode = std::make_shared<OpNode>("AnimateMultipleSteps");

            // Placeholder attributes for multiple animation steps
            commandNode->AddAttribute("Shape", "Circle");
            commandNode->AddAttribute("StartTick", "1");
            commandNode->AddAttribute("EndTick", "10");  // Animation from tick 1 to 10
            commandNode->AddAttribute("StartPosition", "[0, 0]");
            commandNode->AddAttribute("EndPosition", "[100, 100]");  // Moving the shape
            commandNode->AddAttribute("Matrix", "[1, 0, 0, 1]");  // Identity matrix for now

            // Act: Add the animation steps to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for animating multiple steps. Implement this feature in future.");
        }


        TEST_METHOD(TestUndoDrawingAction) {
            // Arrange: Create a future placeholder command for undoing a drawing action
            auto commandNode = std::make_shared<OpNode>("UndoDrawing");

            // Placeholder attributes for undoing a drawing
            commandNode->AddAttribute("ShapeType", "Circle");
            commandNode->AddAttribute("Action", "Undo");

            // Act: Add the undo action to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for undoing a drawing action. Implement this feature in future.");
        }

        TEST_METHOD(TestRedoDrawingAction) {

            #define TEST_CATEGORY(categoryName) \
                Logger::WriteMessage("Category: " categoryName)

            TEST_CATEGORY(L"Undo/Redo");
            // Arrange: Create a future placeholder command for redoing a drawing action
            auto commandNode = std::make_shared<OpNode>("RedoDrawing");

            // Placeholder attributes for redoing a drawing
            commandNode->AddAttribute("ShapeType", "Circle");
            commandNode->AddAttribute("Action", "Redo");

            // Act: Add the redo action to CommandHistory
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(commandNode);

            // Assert: Placeholder failure
            Assert::Fail(L"Placeholder test for redoing a drawing action. Implement this feature in future.");
        }


    };
}
