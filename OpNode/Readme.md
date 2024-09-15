# OpNode

## Overview
OpNode is a lightweight C++ library designed for managing hierarchical, tree-like structures in a basic game engine environment. It serves as a versatile, high-performance data management system that can handle configuration management, undo/redo operations, and other complex workloads, leveraging a flexible acyclic graph or tree node system. OpNode is intended to be simple and generic to start, but it is expected to evolve into a niche solution for developers and customers who need a powerful, yet easy-to-use, graphical interface for rendering content and managing data.

While OpNode itself does not handle interprocess communication, it is designed to integrate seamlessly with a basic game engine that supports such capabilities. The game engine can manage multiple windows running as separate executables, utilizing mutexes and other mechanisms to handle interprocess communication between these windows. This makes OpNode an ideal candidate for applications requiring real-time synchronization and coordination between different components or processes.

### Use Case: Basic Game Engine Integration
OpNode was designed to be integrated into a basic game engine to manage various elements, such as:

Configuration Management: Handle and manage various configurations within the game engine.
Undo/Redo Operations: Efficiently manage state changes for undo/redo functionality, especially for creating and manipulating graphical elements rendered on the screen.
Graphical Elements: Use OpNode as a data structure for managing game elements, menu systems, screen movements, and other dynamic components.
Version Control and Real-Time Collaboration: Enable saving, sharing, and comparing content versions in real time, providing a powerful interface for collaborative editing and content management.
By utilizing OpNode, the game engine can leverage a generic yet powerful system for managing these various elements as nodes in a tree structure. This allows for a flexible and scalable solution that can be extended to meet different application requirements.

## Basic Features
### 1. OpNode
An OpNode represents a single node in a hierarchical structure. Each OpNode can have:

A name (e.g., the name of a graphical element or configuration setting).
A value (e.g., the content or state of the node).
A tag that can store additional metadata.
A namespace with optional prefix and URI, allowing for organized grouping of nodes.
A list of attributes stored as key-value pairs.
A list of child nodes to represent nested elements or configurations.
2. Basic Operations
Adding/Removing Children: Add or remove child nodes dynamically.
Managing Attributes: Add, retrieve, and delete attributes.
Namespace Management: Associate nodes with namespaces to organize them.
Operations Management: Implement custom operations on nodes using the IOperate interface.
Performance Optimization: High performance for handling various workloads, including undo/redo operations and real-time updates to graphical elements.

## Example
### cpp
<code>
#include "OpNode.h"
#include <memory>

int main() {
    // Create a new OpNode with a name
    auto root = std::make_shared<OpNode>("RootNode");

    // Add child nodes
    auto child1 = std::make_shared<OpNode>("Child1");
    auto child2 = std::make_shared<OpNode>("Child2");

    root->AddChild(child1);
    root->AddChild(child2);

    // Add attributes to child1
    child1->AddAttribute("attr1", "value1");
    child1->AddAttribute("attr2", "value2");

    // Set namespace
    auto ns = std::make_shared<NameSpace>();
    ns->Prefix = "ex";
    ns->URI_PREFIX = "http://example.com";
    child1->SetNamespace(ns);

    // Perform operations
    root->PerformOperations();

    return 0;
}
</code>
### Next Steps for Testing
To ensure that OpNode is performant and robust for various game engine use cases, a comprehensive suite of unit tests should be developed. This will verify all fundamental operations before adding more advanced functionality, such as configuration management, undo/redo logic, and graphical element management.

### Basic Unit Tests
Test Node Creation: Verify nodes can be created with appropriate properties.
Test Adding and Removing Children: Ensure that child nodes are added or removed correctly, and counts are accurate.
Test Attribute Management: Verify that attributes can be added, retrieved, and deleted accurately.
Test Namespace Management: Ensure namespaces are correctly assigned and can be queried.
Test Operations Management: Confirm operations behave as expected, especially in the absence or presence of IOperate implementations.
Performance Tests: Ensure that OpNode operations are performant, particularly under heavy workloads or in scenarios requiring rapid updates.
Advanced Functionality to Consider for Future Testing
Configuration Management: Test saving and loading configurations using Serialize and Deserialize.
Undo/Redo Logic: Develop tests for undo/redo functionality to ensure state changes can be reverted.
Inter-Node Operations: Validate that nodes interact correctly based on their namespaces and operations.
Integration with Game Engine: Test the integration of OpNode with the game engine, ensuring mutex handling, interprocess communication, and real-time updates work as expected.
Steps for Creating Unit Tests
Set up MSTest for Unit Testing:

Use Visual Studio's MSTest framework to create a test project for OpNode.
Include your OpNode library in the test project.
### Implement Basic Tests:

Create tests for all fundamental functionalities as outlined above.
Use assertions to verify expected outcomes (Assert::AreEqual, Assert::IsTrue, etc.).
### Run and Validate Tests:

Run all tests locally to ensure they pass.
Set up a CI/CD pipeline (e.g., GitHub Actions) to automatically run tests on every commit or pull request.
### Expand Test Coverage:

As new features are added to the OpNode library, expand the test suite to cover them.
Include edge cases and performance scenarios.

## Conclusion
The OpNode library provides a robust and flexible foundation for managing hierarchical data structures in a game engine environment. With its focus on high performance, configuration management, and easy integration, OpNode is ideal for developers seeking a simple yet powerful solution for handling complex graphical interfaces, game elements, and real-time collaboration.

This README.md serves as a guide for understanding the purpose and use of OpNode, as well as the path forward for testing and further development. By building a solid testing foundation, you can confidently extend the library's capabilities to meet the evolving needs of your game engine and its users.