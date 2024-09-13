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
            // Create a node
            auto node = std::make_shared<OpNode>("Node");

            // Add an attribute to the node
            node->AddAttribute("key", "value");

            // Verify that the attribute was added correctly
            Assert::AreEqual(std::string("value"), node->GetValue("key"));
        }
    };


}
