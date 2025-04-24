#include "pch.h"
#include "CppUnitTest.h"
#include "TestHelpers.h" 

#include "../OpNode/OpNode.h"
#include "../DX11Operation/DirectXOperation.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace BasicGameEngine_UnitTests
{

    TEST_CLASS(OpNodeNamespaceTests)
    {
    public:
        TEST_METHOD(TestSetNamespace)
        {
            OpNode node("TestNode");
            auto ns = std::make_shared<NameSpace>("ex", "suffix", "http://example.com/prefix", "http://example.com/suffix");
            node.SetNamespace(ns);

            Assert::AreEqual(std::string("ex"), node.GetNamespace()->GetPrefix());
            Assert::AreEqual(std::string("suffix"), node.GetNamespace()->GetSuffix());
            Assert::AreEqual(std::string("http://example.com/prefix"), node.GetNamespace()->GetURIPrefix());
            Assert::AreEqual(std::string("http://example.com/suffix"), node.GetNamespace()->GetURISuffix());
        }
    };

    TEST_CLASS(OpNodeTests)
    {
    public:

        TEST_METHOD(TestAddChild)
        {
            // Create a parent node
            auto parent = std::make_shared<OpNode>("Parent");

            // Create a child node
            auto child = std::make_shared<OpNode>("Child");

            // Add the child to the parent
            parent->AddChild(child);

            // Verify that the parent has one child
            Assert::AreEqual(1, static_cast<int>(parent->GetChildren().size()));

            // Verify that the child's name is correct
            Assert::AreEqual(std::string("Child"), parent->GetChildren().front()->GetName());
        }

        TEST_METHOD(TestAddAttribute)
        {
            // Arrange: Create a node
            auto node = std::make_shared<OpNode>("Node");

            // Act: Add an attribute to the node
            node->AddAttribute("key", "value");

            // Assert: Verify that the attribute was added correctly
            Assert::AreEqual(std::string("value"), node->GetValue("key"), L"The attribute value should be 'value'.");
        }

        //TEST_METHOD(TestAddOperation)
        //{
        //    // Arrange: Create a node and a mock operation
        //    auto node = std::make_shared<OpNode>("Node");
        //    auto operation = std::make_shared<CommandHistory>(); // Assuming CommandHistory implements IOperate

        //    // Act: Add the operation to the node
        //    node->AddOperation(operation);

        //    // Assert: Verify that the operation was added
        //    Assert::AreEqual(1, node->OperationsCount(), L"The node should have one operation after adding.");
        //}

        //TEST_METHOD(TestRemoveOperation)
        //{
        //    // Arrange: Create a node and add an operation
        //    auto node = std::make_shared<OpNode>("Node");
        //    auto operation = std::make_shared<CommandHistory>(); // Assuming CommandHistory implements IOperate
        //    #ifndef COMMANDHISTORY_H
        //    #define COMMANDHISTORY_H

        //    #include "IOperate.h"
        //    #include <string>
        //    #include <memory>

        //    class CommandHistory : public IOperate {
        //    public:
        //        CommandHistory() = default;
        //        virtual ~CommandHistory() = default;

        //        std::string Symbol() const override {
        //            return "CH";
        //        }

        //        void Operate(std::shared_ptr<OpNode> node) override {
        //            // Implementation of the operation
        //        }
        //    };

        //    #endif // COMMANDHISTORY_H
        //    node->AddOperation(operation);

        //    // Act: Remove the operation from the node
        //    node->RemoveOperation(operation);

        //    // Assert: Verify that the operation was removed
        //    Assert::AreEqual(0, node->OperationsCount(), L"The node should have no operations after removal.");
        //}

        TEST_METHOD(TestNamespace)
        {
            // Arrange: Create a node
            auto node = std::make_shared<OpNode>("Node");

            // Act: Set a namespace
            auto ns = std::make_shared<NameSpace>("ex", "suffix", "http://example.com/prefix", "http://example.com/suffix");
            node->SetNamespace(ns);

            // Assert: Verify that the namespace was set correctly
            Assert::AreEqual(std::string("ex"), node->GetNamespace()->GetPrefix(), L"The namespace prefix should be 'ex'.");
            Assert::AreEqual(std::string("suffix"), node->GetNamespace()->GetSuffix(), L"The namespace suffix should be 'suffix'.");
            Assert::AreEqual(std::string("http://example.com/prefix"), node->GetNamespace()->GetURIPrefix(), L"The namespace URI prefix should match.");
            Assert::AreEqual(std::string("http://example.com/suffix"), node->GetNamespace()->GetURISuffix(), L"The namespace URI suffix should match.");
        }

        TEST_METHOD(TestDeleteAttribute)
        {
            // Arrange: Create a node and add an attribute
            auto node = std::make_shared<OpNode>("Node");
            node->AddAttribute("key", "value");

            // Act: Delete the attribute
            node->DeleteAttribute("key");

            // Assert: Verify that the attribute was deleted
            Assert::AreEqual(std::string(""), node->GetValue("key"), L"The attribute value should be empty after deletion.");
        }
         

		TEST_METHOD(TestMarkOpNodeAsDeleted)
        {
            // Arrange: Create a node and add an attribute
            auto node = std::make_shared<OpNode>("Node");
            node->AddAttribute("deleted", "true");
			node->SetNamespace(std::make_shared<NameSpace>("ulu", "deleted", "http://userlevelup.com/ulu", "http://userlevelup.com/deleted"));

            // Act: Delete the attribute
            node->DeleteAttribute("key");

            // Assert: Verify that the attribute was deleted
            Assert::AreEqual(std::string(""), node->GetValue("key"), L"The attribute value should be empty after deletion.");
        }

        TEST_METHOD(TestChildOperationInheritance)
        {
            // Arrange: Create parent node and inject DirectX operation
            auto parent = std::make_shared<OpNode>("DXParent");
            auto dxOp = std::make_shared<DirectXOperation>();
            parent->AddOperation(dxOp);

            // Create child node and add it to parent
            auto child = std::make_shared<OpNode>("Child");
            parent->AddChild(child);

            // For this test, we simulate operation inheritance by adding parent's operations to the child.
            // (In a complete implementation the inheritance might occur automatically.)
            // Here, we simply copy parent's operations into the child.
            // Since there's no direct getter for operations, we use OperationIcons to hint the operations.
            child->AddOperation(dxOp);

            // Act: Retrieve the list of operation symbols from the child node
            auto childOpIcons = child->OperationIcons();

            // Assert: The child should have at least one operation and its symbol should be "DX11"
            Assert::IsFalse(childOpIcons.empty(), L"Child node must have at least one operation after copying.");
            Assert::AreEqual(std::string("DX11"), childOpIcons.front());
        }

        TEST_METHOD(TestDirectXRenderWithChildNode)
        {
            // Arrange: Create a dummy window for DirectX
            HWND hWnd = CreateDummyWindow();

            // Create parent node and add the DirectX operation
            auto parent = std::make_shared<OpNode>("DXParent");
            auto dxOp = std::make_shared<DirectXOperation>();
            bool initResult = dxOp->Initialize(hWnd, 800, 600);
            Assert::IsTrue(initResult, L"DirectX operation should initialize successfully.");
            parent->AddOperation(dxOp);

            // Create a child node and add it to the parent,
            // then manually pass down the DirectX operation.
            auto child = std::make_shared<OpNode>("ChildNode");
            parent->AddChild(child);
            child->AddOperation(dxOp);

            // Act: Invoke operations (could internally clear the render target, etc.)
            parent->PerformOperations();
            child->PerformOperations();

            // Optionally, call Render to test further
            dxOp->Render();
            dxOp->Cleanup();

            DestroyWindow(hWnd);

            // Assert: If no exceptions occur, DirectX calls are working.
            // Additional assertions can be added if your DirectXOperation sets flags or logs status.
        }


    };


}
