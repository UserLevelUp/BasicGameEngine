# OpNode

## Overview
OpNode is a lightweight C++ library designed for managing hierarchical, tree-like structures in a basic game engine environment. It serves as a versatile, high-performance data management system that can handle configuration management, undo/redo operations, and other complex workloads, leveraging a flexible acyclic graph or tree node system. OpNode is intended to be simple and generic to start, but it is expected to evolve into a niche solution for developers and customers who need a powerful, yet easy-to-use, graphical interface for rendering content and managing data.

While OpNode itself does not handle interprocess communication, it is designed to integrate seamlessly with a basic game engine that supports such capabilities. The game engine can manage multiple windows running as separate executables, utilizing mutexes and other mechanisms to handle interprocess communication between these windows. This makes OpNode an ideal candidate for applications requiring real-time synchronization and coordination between different components or processes.

Notice that OpNode is a sub folder in the Basic Game Engine: https://github.com/UserLevelUp/BasicGameEngine/tree/master/OpNode.

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

#### Most of these base features have already been added to OpNode system.

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

Namespace Management
The NameSpace class is pivotal in managing various aspects of node metadata within the OpNode system. Here's a detailed explanation of how prefixes and suffixes are utilized:

Prefixes
Purpose: Prefixes are used to indicate capabilities, modes, schema, or status related to the node and its parent. This includes tracking operational modes or states within the BasicGameEngine.
Usage: Multiple prefixes can be assigned, separated by commas, to a node. This helps in categorizing and managing different operational or state-related aspects.
cpp
Copy code
// Example of setting multiple prefixes
auto ns = std::make_shared<NameSpace>("CapabilityA,ModeB", "Active", "http://example.com/prefix", "http://example.com/suffix");
node->SetNamespace(ns);
Suffixes
Purpose: Suffixes are used in conjunction with undo/redo logic to track the state of a node, such as marking a node as deleted or active. They are integral to the CommandHistory mechanism for maintaining and reverting state changes.
Usage: Suffixes help in managing the history of changes, such as adding a suffix to mark a node as deleted and removing it to restore the node.
cpp
Copy code
// Example of setting suffix for undo/redo
auto ns = std::make_shared<NameSpace>("Prefix", "Deleted", "http://example.com/prefix", "http://example.com/suffix");
node->SetNamespace(ns);
NameSpace Class
Here�s a summary of the NameSpace class and its functionalities:

cpp
Copy code
#include "pch.h"
#include "NameSpace.h"

// Constructor
NameSpace::NameSpace(const std::string& prefix, const std::string& suffix, const std::string& uri_prefix, const std::string& uri_suffix)
    : prefix_(prefix), suffix_(suffix), uri_prefix_(uri_prefix), uri_suffix_(uri_suffix) {}

// Getters and Setters
const std::string& NameSpace::GetPrefix() const {
    return prefix_;
}

void NameSpace::SetPrefix(const std::string& prefix) {
    prefix_ = prefix;
}

const std::string& NameSpace::GetSuffix() const {
    return suffix_;
}

void NameSpace::SetSuffix(const std::string& suffix) {
    suffix_ = suffix;
}

const std::string& NameSpace::GetURIPrefix() const {
    return uri_prefix_;
}

void NameSpace::SetURIPrefix(const std::string& uri_prefix) {
    uri_prefix_ = uri_prefix;
}

const std::string& NameSpace::GetURISuffix() const {
    return uri_suffix_;
}

void NameSpace::SetURISuffix(const std::string& uri_suffix) {
    uri_suffix_ = uri_suffix;
}

// Cloning method
std::shared_ptr<NameSpace> NameSpace::Clone() const {
    return std::make_shared<NameSpace>(prefix_, suffix_, uri_prefix_, uri_suffix_);
}
Constructor: Initializes the NameSpace with prefix, suffix, URI prefix, and URI suffix.
Getters/Setters: Methods to access and modify the prefix, suffix, URI prefix, and URI suffix.
Clone Method: Creates a copy of the current NameSpace instance.
Processing Logic
Selective Processing: Nodes are processed based on their relationship with nodes that implement the IOperate interface. Only relevant parent nodes and their ancestors are processed when a child node is added.

Brute Force Processing:

Depth-First Search: The system performs a depth-first search from the master node to identify all relevant leaf nodes.
Processing Queue: Identified leaf nodes are queued and processed sequentially.
Integration with BasicGameEngine
Status Monitoring: The BasicGameEngine tracks changes to nodes and updates its status to indicate when content is ready for saving.
Process Execution: Nodes are processed based on their operational requirements and the state of the IOperate interface.
By clearly defining the roles of prefixes and suffixes, and documenting the NameSpace class functionalities, you ensure that users of the OpNode library can effectively utilize these features in their applications. If you need further assistance with documentation or implementation, feel free to ask!

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

## Future Development

### Prefixes

Prefixes will be used primarily for identifying IOperator operations for a node within the OpNode system library.  An example might be for the parent of an OpNode would have an Add IOperate interface implemented, adn it would allow for thhe adding all the text or tag values of each of its child nodes.  All operations of the parent level are performed on the children and the final result orstate or both are updated to the parent having that operation upon completion.  Operations are assumed to only be appplied to all of its child nodes when processing any node with the IOperate interface.

### Suffixes
Suffixes defined at the namespace level can be added and used with basic CRUD operations, especially marking elements and attributes as deleted, active if there could be a question of it being active, or modes.   Undo and redo will work with the CommandHistory IOperate implementation which is always created for each MasterNode for a BasicGameEngine as a new OpNode child off the main master node to keep track of the commands used during the creation of a game in BGE.

### Testing Future Development

Add tests first and most tests should fail starting out, and the project management story will identify tets that should pass.  Anything added to backlog must first have a test which will most likely fail first and a  comment of the backlog story identifier.  Later, the story and its associated failed test should pass when that task is completed.  This will make development simpler and easier to follow.  Any complex stroy should be grouped as a logic set of failed tests that should pass once everything is working as expected.

## Other Notes:

Also for future development would like to also mark nodes with an Error prefix for that OpNode. Any prefixes should be comma delimited since there is only one field for a prefix and a prefixuri, so to have multiple they need to be comma delimited in order they come or are detected like an error.  When the error goes away, it would remove that from the prefix for the namespace.  If the namespace doesn't exist it should create the namespace.  Saving the OpNode depends on the app using the OpNode library.  The basic game engine should have a status like ready for saving when content in the OpNode changes.  Items within the OpNode system should only be processed when a child item is added for a specific parent that has an IOperate interface implementation.   Only the parent and its parent that contain IOperate should be processed, nothing else.  It processes up.  The only other time is when a brute force process all nodes will happen.  Then it starts with the master node, uses depth first search to find any leaf parent nodes that have any child nodes and then adds them to some kind of process queue.  Once all have been found, then it processes all those leaf nodes until all are completed.  I'll add this to the readme.


## Conclusion
The OpNode library provides a robust and flexible foundation for managing hierarchical data structures in a game engine environment. With its focus on high performance, configuration management, and easy integration, OpNode is ideal for developers seeking a simple yet powerful solution for handling complex graphical interfaces, game elements, and real-time collaboration.

This README.md serves as a guide for understanding the purpose and use of OpNode, as well as the path forward for testing and further development. By building a solid testing foundation, you can confidently extend the library's capabilities to meet the evolving needs of your game engine and its users.