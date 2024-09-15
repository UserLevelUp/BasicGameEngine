#include "pch.h"
#include "CppUnitTest.h"

#include "../OpNode/OpNode.h"

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


    };


}
