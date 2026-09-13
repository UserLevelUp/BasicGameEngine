#include "pch.h"
#include "CppUnitTest.h"
#include "../CommandHistoryOperation/CommandHistoryOperation.h"  // Adjust path as needed
#include <Windows.h>  // Include Windows API header for LoadLibrary, FreeLibrary
#include <memory>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include "../OpNode/IOperate.h"  // Include IOperate from OpNode project
#include "../CommandHistoryOperation/ArtBoard.h"

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

    if (history.GetNextCollection()) {
        int continuationDepth = CalculateDepth(*history.GetNextCollection());
        if (continuationDepth > maxDepth) {
            maxDepth = continuationDepth;
        }
    }
    return maxDepth;
}

// Namespace declaration should be at file scope
namespace BasicGameEngine_UnitTests
{
    struct ArtBoardFixture {
        CommandHistoryOperation history;
        std::shared_ptr<ArtBoardDocument> document = std::make_shared<ArtBoardDocument>();

        ArtBoardFixture() {
            RegisterArtBoardCommands(history, document);
            Execute("CreateArtBoard", { { "Board", "Board1" } });
        }

        std::shared_ptr<OpNode> Execute(const std::string& type,
            std::initializer_list<std::pair<std::string, std::string>> attributes) {
            auto node = std::make_shared<OpNode>(type);
            node->AddAttribute("CommandType", type);
            for (const auto& attribute : attributes) {
                node->AddAttribute(attribute.first, attribute.second);
            }
            history.AddCommand(node);
            return node;
        }

        std::shared_ptr<OpNode> DrawCircle(const std::string& id = "Circle", const std::string& board = "Board1") {
            return Execute("DrawCircle", { { "Board", board }, { "ShapeId", id }, { "Center", "[0,0]" },
                { "Radius", "50" }, { "Matrix", "[1,0,0,1]" }, { "Color", "0.5,0.2,0.7,1.0" } });
        }

        const Shape& ShapeAt(size_t index = 0, const std::string& board = "Board1") const {
            return document->boards.at(board).shapes.At(index);
        }
    };

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
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            auto command = std::make_shared<OpNode>("NonExistentCommand");
            command->AddAttribute("CommandType", "NonExistent");
            Assert::ExpectException<std::invalid_argument>([&] { commandHistory->AddCommand(command); });
            Assert::IsTrue(commandHistory->GetCommandEntries().empty());
            Assert::IsFalse(commandHistory->Undo());
            Assert::IsFalse(commandHistory->Redo());
        }

        TEST_METHOD(TestGenericCommandsRemainPermissive) {
            CommandHistoryOperation history;
            auto command = std::make_shared<OpNode>("UnregisteredGenericName");
            history.AddCommand(command);
            Assert::IsTrue(history.GetCommandEntries().front().node == command);
        }

        TEST_METHOD(TestUndoRedoFirstCommand) {
            CommandHistoryOperation history;
            Assert::IsFalse(history.Undo());
            Assert::IsFalse(history.Redo());
            history.AddCommand(std::make_shared<OpNode>("First"));
            Assert::IsTrue(history.Undo());
            Assert::AreEqual(CommandState::Undone, history.GetCommandEntries()[0].state);
            Assert::IsFalse(history.Undo());
            Assert::IsTrue(history.Redo());
            Assert::AreEqual(CommandState::Executed, history.GetCommandEntries()[0].state);
            Assert::IsFalse(history.Redo());
        }

        TEST_METHOD(TestRegisteredCommandUndoRedoAndValidation) {
            CommandHistoryOperation history;
            int value = 0;
            history.RegisterCommand("Increment", [&](const std::shared_ptr<OpNode>&) {
                return CommandAction{ [&] { ++value; }, [&] { --value; } };
            });
            auto command = std::make_shared<OpNode>("IncrementValue");
            command->AddAttribute("CommandType", "Increment");
            history.AddCommand(command);
            Assert::AreEqual(1, value);
            Assert::IsTrue(history.Undo());
            Assert::AreEqual(0, value);
            Assert::IsTrue(history.Redo());
            Assert::AreEqual(1, value);
            Assert::ExpectException<std::invalid_argument>([&] { history.AddCommand(nullptr); });
            Assert::ExpectException<std::invalid_argument>([&] { history.RegisterCommand("", {}); });
            Assert::ExpectException<std::invalid_argument>([] { CommandHistoryOperation invalid(0); });
            Assert::AreEqual(size_t(1), history.GetCommandEntries().size());
        }

        TEST_METHOD(TestBranchUndoRedoTargetsActiveCommands) {
            CommandHistoryOperation history(2);
            int value = 0;
            history.RegisterCommand("Increment", [&](const std::shared_ptr<OpNode>&) {
                return CommandAction{ [&] { ++value; }, [&] { --value; } };
            });
            auto command = std::make_shared<OpNode>("Increment");
            command->AddAttribute("CommandType", "Increment");
            history.AddCommand(command);
            history.AddCommand(command);
            Assert::IsTrue(history.Undo());
            history.AddCommand(command);
            history.AddCommand(command);
            Assert::AreEqual(3, value);
            Assert::AreEqual(2, CalculateDepth(history));
            Assert::IsTrue(history.Undo());
            Assert::IsTrue(history.Undo());
            Assert::IsTrue(history.Undo());
            Assert::AreEqual(0, value);
            Assert::IsFalse(history.Undo());
            Assert::IsTrue(history.Redo());
            Assert::IsTrue(history.Redo());
            Assert::IsTrue(history.Redo());
            Assert::AreEqual(3, value);
            Assert::IsFalse(history.Redo());
            Assert::AreEqual(CommandState::Undone, history.GetCommandEntries()[1].state);
        }

        TEST_METHOD(TestBranchAfterUndoAllAtDepthLimit) {
            CommandHistoryOperation history(1);
            history.AddCommand(std::make_shared<OpNode>("Old"));
            Assert::IsTrue(history.Undo());
            history.AddCommand(std::make_shared<OpNode>("New"));
            Assert::AreEqual(size_t(1), history.GetCommandEntries().size());
            Assert::AreEqual(std::string("New"), history.GetCommandEntries().front().node->GetName());
            Assert::AreEqual(1, CalculateDepth(history));
            Assert::IsFalse(history.Redo());
            Assert::IsTrue(history.Undo());
        }

        TEST_METHOD(TestCollectionRolloverAndTraversal) {
            CommandHistoryOperation history;
            Assert::IsNull(history.GetNextCollection().get());
            for (int i = 0; i < 10; ++i) {
                history.AddCommand(std::make_shared<OpNode>(std::to_string(i)));
            }
            Assert::AreEqual(size_t(10), history.GetCommandEntries().size());
            Assert::IsNull(history.GetNextCollection().get());
            history.AddCommand(std::make_shared<OpNode>("10"));
            auto second = history.GetNextCollection();
            Assert::IsNotNull(second.get());
            Assert::AreEqual(size_t(10), history.GetCommandEntries().size());
            Assert::AreEqual(size_t(1), second->GetCommandEntries().size());
            Assert::AreEqual(std::string("10"), second->GetCommandEntries()[0].node->GetName());
            for (int i = 11; i < 20; ++i) {
                history.AddCommand(std::make_shared<OpNode>(std::to_string(i)));
            }
            Assert::AreEqual(size_t(10), second->GetCommandEntries().size());
            Assert::IsNull(second->GetNextCollection().get());
            history.AddCommand(std::make_shared<OpNode>("20"));
            Assert::IsNotNull(second->GetNextCollection().get());
            Assert::AreEqual(size_t(1), second->GetNextCollection()->GetCommandEntries().size());
            int visited = 0;
            history.TraverseCommands([&](const std::shared_ptr<OpNode>& node) {
                Assert::AreEqual(std::to_string(visited++), node->GetName());
            });
            Assert::AreEqual(21, visited);
            Assert::AreEqual(1, CalculateDepth(history), L"Continuation collections do not consume branch depth.");
            Assert::AreEqual(1, history.GetCurrentDepth());
        }

        TEST_METHOD(TestCollectionBoundaryUndoRedoAndBranch) {
            CommandHistoryOperation history(2);
            int value = 0;
            history.RegisterCommand("Increment", [&](const std::shared_ptr<OpNode>&) {
                return CommandAction{ [&] { ++value; }, [&] { --value; } };
            });
            auto command = std::make_shared<OpNode>("Increment");
            command->AddAttribute("CommandType", "Increment");
            for (int i = 0; i < 21; ++i) {
                history.AddCommand(command);
            }
            Assert::AreEqual(21, value);
            for (int i = 20; i >= 0; --i) {
                Assert::IsTrue(history.Undo());
                Assert::AreEqual(i, value);
            }
            Assert::IsFalse(history.Undo());
            for (int i = 1; i <= 21; ++i) {
                Assert::IsTrue(history.Redo());
                Assert::AreEqual(i, value);
            }
            Assert::IsFalse(history.Redo());
            history.MoveCursorUp();
            Assert::AreEqual(20, value);
            Assert::IsFalse(history.IsAtLeafNode());
            history.MoveCursorDown();
            Assert::AreEqual(21, value);
            for (int i = 0; i < 12; ++i) {
                Assert::IsTrue(history.Undo());
            }
            history.AddCommand(command);
            Assert::AreEqual(10, value);
            Assert::AreEqual(2, CalculateDepth(history));
            Assert::AreEqual(size_t(10), history.GetCommandEntries().size());
            Assert::AreEqual(size_t(10), history.GetNextCollection()->GetCommandEntries().size());
            Assert::IsFalse(history.Redo(), L"The abandoned continuation must not execute on the new branch.");
            for (int i = 0; i < 10; ++i) {
                Assert::IsTrue(history.Undo());
            }
            Assert::AreEqual(0, value);
            history.MoveCursorToEnd();
            Assert::AreEqual(10, value);
            Assert::IsTrue(history.IsAtLeafNode());
        }

        TEST_METHOD(TestRejectedCommandDoesNotAllocateCollection) {
            CommandHistoryOperation history;
            for (int i = 0; i < 10; ++i) {
                history.AddCommand(std::make_shared<OpNode>("Generic"));
            }
            auto invalid = std::make_shared<OpNode>("Invalid");
            invalid->AddAttribute("CommandType", "Unknown");
            Assert::ExpectException<std::invalid_argument>([&] { history.AddCommand(invalid); });
            Assert::AreEqual(size_t(10), history.GetCommandEntries().size());
            Assert::IsNull(history.GetNextCollection().get());
            Assert::IsTrue(history.Undo());
            Assert::IsTrue(history.Redo());
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
                ancestry += currentNode->GetName() + " -> ";
                currentNode = currentNode->GetParent();
            }

            // Assert: Verify ancestry trace
            Assert::AreEqual(std::string("ChildCommand -> ParentCommand -> RootCommand -> "), ancestry, L"Ancestry does not match expected path to root.");
            Assert::IsNotNull(child.get(), L"Traversal must not overwrite the starting node.");
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
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("TranslationCommand", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "Matrix", "[2,0,0,5;0,3,0,7;0,0,1,0;0,0,0,1]" }, { "TimeComponent", "10s" } });
            const auto transform = context.ShapeAt().transform;
            Assert::AreEqual(10.0, transform.timeSeconds);
            Assert::AreEqual(2.0, transform.matrix[0]);
            auto point = transform.Apply({ 2, 4 });
            Assert::AreEqual(9.0, point.x);
            Assert::AreEqual(19.0, point.y);
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(0.0, context.ShapeAt().transform.timeSeconds);
            Assert::AreEqual(1.0, context.ShapeAt().transform.matrix[0]);
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(10.0, context.ShapeAt().transform.timeSeconds);
            Assert::AreEqual(5.0, context.ShapeAt().transform.matrix[3]);
        }

        TEST_METHOD(TestFutureHandlingOfNonExistentCommands) {
            auto commandNode = std::make_shared<OpNode>("NonExistentCommand");
            commandNode->AddAttribute("CommandType", "NonExistent");
            auto commandHistory = std::make_shared<CommandHistoryOperation>(10);
            commandHistory->AddCommand(std::make_shared<OpNode>("Existing"));
            Assert::IsTrue(commandHistory->Undo());
            Assert::ExpectException<std::invalid_argument>([&] { commandHistory->AddCommand(commandNode); });
            Assert::AreEqual(size_t(1), commandHistory->GetCommandEntries().size());
            Assert::AreEqual(CommandState::Undone, commandHistory->GetCommandEntries()[0].state);
            Assert::IsTrue(commandHistory->Redo(), L"Rejected commands must preserve the redo path.");
        }

        TEST_METHOD(TestCombineStaticAndAnimatedArtBoards) {
            ArtBoardFixture context;
            context.DrawCircle("StaticCircle");
            context.Execute("CreateArtBoard", { { "Board", "Board2" }, { "Animated", "true" } });
            context.DrawCircle("AnimatedCircle", "Board2");
            context.Execute("CombineArtBoards", { { "StaticBoard", "Board1" }, { "AnimatedBoard", "Board2" },
                { "destination_board_name", "CombinedBoard" } });
            const auto combined = context.document->boards.at("CombinedBoard");
            Assert::AreEqual(std::string("CombinedBoard"), combined.name);
            Assert::IsTrue(combined.animated);
            Assert::AreEqual(size_t(2), combined.shapes.Size());
            Assert::AreEqual(std::string("StaticCircle"), combined.shapes.At(0).id);
            Assert::AreEqual(std::string("AnimatedCircle"), combined.shapes.At(1).id);
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board2").shapes.Size());
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(0), context.document->boards.count("CombinedBoard"));
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(2), context.document->boards.at("CombinedBoard").shapes.Size());
        }

        TEST_METHOD(TestAddCircleToArtBoard) {
            ArtBoardFixture context;
            context.DrawCircle();
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            const auto& shape = context.ShapeAt();
            Assert::IsTrue(std::holds_alternative<Circle>(shape.geometry));
            const auto& circle = std::get<Circle>(shape.geometry);
            Assert::AreEqual(50.0, circle.radius);
            Assert::AreEqual(0.0, circle.center.x);
            Assert::AreEqual(0.0, circle.center.y);
            Assert::AreEqual(0.5, shape.color[0]);
            Assert::AreEqual(1.0, shape.color[3]);
            Assert::AreEqual(1.0, shape.transform.matrix[0]);
            Assert::AreEqual(1.0, shape.transform.matrix[5]);
        }


        TEST_METHOD(TestAddLineToArtBoard) {
            ArtBoardFixture context;
            context.Execute("DrawLine", { { "Board", "Board1" }, { "ShapeId", "Line" },
                { "Start", "[0,0]" }, { "End", "[100,100]" } });
            Assert::IsTrue(std::holds_alternative<Line>(context.ShapeAt().geometry));
            const auto& line = std::get<Line>(context.ShapeAt().geometry);
            Assert::AreEqual(0.0, line.start.x);
            Assert::AreEqual(0.0, line.start.y);
            Assert::AreEqual(100.0, line.end.x);
            Assert::AreEqual(100.0, line.end.y);
        }


        TEST_METHOD(TestAddRectangleToArtBoard) {
            ArtBoardFixture context;
            context.Execute("DrawRectangle", { { "Board", "Board1" }, { "ShapeId", "Rectangle" },
                { "TopLeft", "[2,3]" }, { "Dimensions", "[100,50]" } });
            Assert::IsTrue(std::holds_alternative<RectangleShape>(context.ShapeAt().geometry));
            const auto& rectangle = std::get<RectangleShape>(context.ShapeAt().geometry);
            Assert::AreEqual(2.0, rectangle.topLeft.x);
            Assert::AreEqual(3.0, rectangle.topLeft.y);
            Assert::AreEqual(100.0, rectangle.dimensions.x);
            Assert::AreEqual(50.0, rectangle.dimensions.y);
        }


        TEST_METHOD(TestAddAnimationTick) {
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("AddAnimationTick", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "TickNumber", "1" }, { "Position", "[2,3]" },
                { "Matrix", "[1,0,0,5;0,1,0,7;0,0,1,0;0,0,0,1]" }, { "TimeComponent", "0.25s" } });
            Assert::AreEqual(size_t(1), context.ShapeAt().animation.Size());
            const auto tick = context.ShapeAt().animation.At(0);
            Assert::AreEqual(1, tick.number);
            Assert::AreEqual(2.0, tick.position.x);
            Assert::AreEqual(3.0, tick.position.y);
            Assert::AreEqual(0.25, tick.transform.timeSeconds);
            Assert::AreEqual(7.0, context.ShapeAt().PositionAtTick(1).x);
            Assert::AreEqual(10.0, context.ShapeAt().PositionAtTick(1).y);
            Assert::IsTrue(context.document->boards.at("Board1").animated);
            Assert::AreEqual(50.0, std::get<Circle>(context.ShapeAt().geometry).radius);
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(0), context.ShapeAt().animation.Size());
            Assert::IsFalse(context.document->boards.at("Board1").animated);
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(1), context.ShapeAt().animation.Size());
            Assert::AreEqual(7.0, context.ShapeAt().PositionAtTick(1).x);
            Assert::ExpectException<std::out_of_range>([&] { context.ShapeAt().PositionAtTick(2); });
        }


        TEST_METHOD(TestMultipleAnimationSteps) {
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "StartTick", "1" }, { "EndTick", "10" }, { "StartPosition", "[0,0]" },
                { "EndPosition", "[100,100]" }, { "Matrix", "[1,0,0,1]" } });
            Assert::AreEqual(size_t(10), context.ShapeAt().animation.Size());
            for (int i = 0; i < 10; ++i) {
                const auto& tick = context.ShapeAt().animation.At(i);
                Assert::AreEqual(i + 1, tick.number);
                Assert::AreEqual(100.0 * i / 9.0, tick.position.x, 0.000001);
                Assert::AreEqual(100.0 * i / 9.0, tick.position.y, 0.000001);
            }
            Assert::AreEqual(100.0, context.ShapeAt().PositionAtTick(10).x);
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(0), context.ShapeAt().animation.Size());
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(10), context.ShapeAt().animation.Size());
        }

        TEST_METHOD(TestAnimationCollectionsAndSequentialRanges) {
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "StartTick", "1" }, { "EndTick", "10" }, { "StartPosition", "[1,1]" }, { "EndPosition", "[10,10]" } });
            context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "StartTick", "11" }, { "EndTick", "21" }, { "StartPosition", "[11,11]" }, { "EndPosition", "[21,21]" } });
            const auto samples = context.ShapeAt().animation;
            Assert::AreEqual(size_t(21), samples.Size());
            Assert::AreEqual(size_t(3), samples.GetCollections().size());
            Assert::AreEqual(size_t(10), samples.GetCollections().front().size());
            Assert::AreEqual(size_t(1), samples.GetCollections().back().size());
            for (int i = 0; i < 21; ++i) {
                Assert::AreEqual(i + 1, samples.At(i).number);
                Assert::AreEqual(static_cast<double>(i + 1), samples.At(i).position.x, 0.000001);
            }
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(10), context.ShapeAt().animation.Size());
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(21), context.ShapeAt().animation.Size());
        }

        TEST_METHOD(TestInvalidAnimationLeavesStateUnchanged) {
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("AddAnimationTick", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                { "TickNumber", "1" }, { "Position", "[2,3]" } });
            for (const std::string tick : { "-1", "0", "1", "1.5", "2147483648", "2bad" }) {
                Assert::ExpectException<std::invalid_argument>([&] {
                    context.Execute("AddAnimationTick", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                        { "TickNumber", tick }, { "Position", "[0,0]" } });
                });
            }
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                    { "StartTick", "10" }, { "EndTick", "2" }, { "StartPosition", "[0,0]" }, { "EndPosition", "[1,1]" } });
            });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                    { "StartTick", "1" }, { "EndTick", "10" }, { "StartPosition", "[0,0]" }, { "EndPosition", "[1,1]" } });
            });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("AddAnimationTick", { { "Board", "Board1" }, { "ShapeId", "Missing" },
                    { "TickNumber", "2" }, { "Position", "[0,0]" } });
            });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("AddAnimationTick", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                    { "TickNumber", "2" }, { "Position", "[nan,0]" } });
            });
            Assert::AreEqual(size_t(3), context.history.GetCommandEntries().size());
            Assert::AreEqual(size_t(1), context.ShapeAt().animation.Size());
            Assert::AreEqual(2.0, context.ShapeAt().PositionAtTick(1).x);
        }

        TEST_METHOD(TestSingleTickRangeAtIntegerBoundaries) {
            ArtBoardFixture context;
            context.DrawCircle();
            for (const std::string tick : { "0", "2147483647" }) {
                context.Execute("AnimateMultipleSteps", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                    { "StartTick", tick }, { "EndTick", tick }, { "StartPosition", "[2,3]" }, { "EndPosition", "[2,3]" } });
            }
            Assert::AreEqual(size_t(2), context.ShapeAt().animation.Size());
            Assert::AreEqual(0, context.ShapeAt().animation.At(0).number);
            Assert::AreEqual(2147483647, context.ShapeAt().animation.At(1).number);
            Assert::AreEqual(2.0, context.ShapeAt().PositionAtTick(2147483647).x);
        }

        TEST_METHOD(TestCombinedArtBoardsPreserveIndependentAnimation) {
            ArtBoardFixture context;
            context.DrawCircle("Static");
            context.Execute("CreateArtBoard", { { "Board", "Board2" }, { "Animated", "true" } });
            context.DrawCircle("Animated", "Board2");
            context.Execute("AddAnimationTick", { { "Board", "Board2" }, { "ShapeId", "Animated" },
                { "TickNumber", "1" }, { "Position", "[25,30]" }, { "TimeComponent", "2s" } });
            context.Execute("CombineArtBoards", { { "StaticBoard", "Board1" }, { "AnimatedBoard", "Board2" },
                { "destination_board_name", "Combined" } });
            Assert::AreEqual(size_t(0), context.ShapeAt(0, "Combined").animation.Size());
            Assert::AreEqual(25.0, context.ShapeAt(1, "Combined").PositionAtTick(1).x);
            Assert::AreEqual(2.0, context.ShapeAt(1, "Combined").animation.At(0).transform.timeSeconds);
            context.Execute("AddAnimationTick", { { "Board", "Combined" }, { "ShapeId", "Animated" },
                { "TickNumber", "2" }, { "Position", "[50,60]" } });
            Assert::AreEqual(size_t(2), context.ShapeAt(1, "Combined").animation.Size());
            Assert::AreEqual(size_t(1), context.ShapeAt(0, "Board2").animation.Size());
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(1), context.ShapeAt(1, "Combined").animation.Size());
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(50.0, context.ShapeAt(1, "Combined").PositionAtTick(2).x);
        }


        TEST_METHOD(TestUndoDrawingAction) {
            ArtBoardFixture context;
            context.DrawCircle();
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(0), context.document->boards.at("Board1").shapes.Size());
            Assert::AreEqual(CommandState::Undone, context.history.GetCommandEntries()[1].state);
            Assert::IsTrue(context.history.Undo());
            Assert::IsTrue(context.document->boards.empty());
            Assert::IsFalse(context.history.Undo());
        }

        TEST_METHOD(TestRedoDrawingAction) {
            ArtBoardFixture context;
            auto command = context.DrawCircle();
            Assert::IsTrue(context.history.Undo());
            command->SetAttribute("Radius", "999");
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            Assert::AreEqual(50.0, std::get<Circle>(context.ShapeAt().geometry).radius,
                L"Redo must use the recorded command state, not subsequently edited attributes.");
            Assert::AreEqual(CommandState::Executed, context.history.GetCommandEntries()[1].state);
            Assert::IsFalse(context.history.Redo());
        }

        TEST_METHOD(TestShapeCollectionsUseTenItemBatches) {
            ArtBoardFixture context;
            for (int i = 0; i < 11; ++i) {
                context.DrawCircle(std::to_string(i));
            }
            const auto& shapes = context.document->boards.at("Board1").shapes;
            Assert::AreEqual(size_t(11), shapes.Size());
            Assert::AreEqual(size_t(2), shapes.GetCollections().size());
            Assert::AreEqual(size_t(10), shapes.GetCollections().front().size());
            Assert::AreEqual(size_t(1), shapes.GetCollections().back().size());
            Assert::AreEqual(std::string("10"), shapes.At(10).id);
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(10), context.document->boards.at("Board1").shapes.Size());
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(size_t(11), context.document->boards.at("Board1").shapes.Size());
        }

        TEST_METHOD(TestInvalidDrawingCommandsLeaveStateUnchanged) {
            ArtBoardFixture context;
            context.DrawCircle();
            for (const std::string radius : { "-1", "0", "nan", "1oops" }) {
                Assert::ExpectException<std::invalid_argument>([&] {
                    context.Execute("DrawCircle", { { "Board", "Board1" }, { "ShapeId", "Invalid" },
                        { "Center", "[0,0]" }, { "Radius", radius } });
                });
            }
            Assert::ExpectException<std::invalid_argument>([&] { context.DrawCircle(); });
            Assert::ExpectException<std::invalid_argument>([&] { context.DrawCircle("Missing", "UnknownBoard"); });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("DrawRectangle", { { "Board", "Board1" }, { "ShapeId", "Invalid" },
                    { "TopLeft", "[0,0]" }, { "Dimensions", "[100,-1]" } });
            });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("DrawLine", { { "Board", "Board1" }, { "ShapeId", "Invalid" },
                    { "Start", "[0,0]" } });
            });
            for (const std::string matrix : { "[1,2,3]", "[1,0,0,1] garbage" }) {
                Assert::ExpectException<std::invalid_argument>([&] {
                    context.Execute("TranslationCommand", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                        { "Matrix", matrix }, { "TimeComponent", "10s" } });
                });
            }
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("TranslationCommand", { { "Board", "Board1" }, { "ShapeId", "Circle" },
                    { "Matrix", "[1,0,0,1]" }, { "TimeComponent", "-1s" } });
            });
            Assert::AreEqual(size_t(2), context.history.GetCommandEntries().size());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            Assert::AreEqual(50.0, std::get<Circle>(context.ShapeAt().geometry).radius);
            Assert::AreEqual(0.0, context.ShapeAt().transform.timeSeconds);
        }

        TEST_METHOD(TestDrawingBranchRestoresOnlyActiveState) {
            ArtBoardFixture context;
            context.DrawCircle("First");
            context.DrawCircle("Abandoned");
            Assert::IsTrue(context.history.Undo());
            context.DrawCircle("Replacement");
            Assert::AreEqual(std::string("Replacement"), context.ShapeAt(1).id);
            Assert::IsFalse(context.history.Redo());
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            Assert::IsTrue(context.history.Undo());
            Assert::AreEqual(size_t(0), context.document->boards.at("Board1").shapes.Size());
            Assert::IsTrue(context.history.Redo());
            Assert::IsTrue(context.history.Redo());
            Assert::AreEqual(std::string("First"), context.ShapeAt(0).id);
            Assert::AreEqual(std::string("Replacement"), context.ShapeAt(1).id);
        }

        TEST_METHOD(TestInvalidArtBoardCombinationPreservesSources) {
            ArtBoardFixture context;
            context.DrawCircle();
            context.Execute("CreateArtBoard", { { "Board", "Board2" }, { "Animated", "true" } });
            context.DrawCircle("Circle", "Board2");
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("CombineArtBoards", { { "StaticBoard", "Board1" }, { "AnimatedBoard", "Board2" },
                    { "destination_board_name", "Combined" } });
            });
            Assert::ExpectException<std::invalid_argument>([&] {
                context.Execute("CombineArtBoards", { { "StaticBoard", "Board1" }, { "AnimatedBoard", "Missing" },
                    { "destination_board_name", "Combined" } });
            });
            Assert::AreEqual(size_t(2), context.document->boards.size());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board1").shapes.Size());
            Assert::AreEqual(size_t(1), context.document->boards.at("Board2").shapes.Size());
            Assert::AreEqual(size_t(4), context.history.GetCommandEntries().size());
        }


    };
}
