#include "pch.h"  // Include precompiled header

#include "OpNodeMgr.h"
#include "OpNode.h"
#include "IOperate.h"
#include <fstream>  // For file handling
#include <iostream>

// Constructor
OpNodeMgr::OpNodeMgr() {
    // Initialization logic if any
}

// Destructor
OpNodeMgr::~OpNodeMgr() {
    // Cleanup logic if any
}

// Add a root node
void OpNodeMgr::AddRootNode(std::shared_ptr<OpNode> root) {
    root_nodes_.push_back(root);
}

// Remove a root node
void OpNodeMgr::RemoveRootNode(std::shared_ptr<OpNode> root) {
    root_nodes_.remove(root);
}

// Execute all operations
void OpNodeMgr::ExecuteAllOperations() {
    for (auto& root : root_nodes_) {
        root->PerformOperations();  // Assuming PerformOperations will execute the node's operations
    }
}

// Find a node by key
std::shared_ptr<OpNode> OpNodeMgr::FindNodeByKey(const std::string& key) const {
    for (const auto& root : root_nodes_) {
        auto foundNode = FindNodeByKeyRecursive(root, key);
        if (foundNode) return foundNode;
    }
    return nullptr;  // Return nullptr if not found
}

// Helper for recursive search
std::shared_ptr<OpNode> OpNodeMgr::FindNodeByKeyRecursive(const std::shared_ptr<OpNode>& current, const std::string& key) const {
    if (current->GetName() == key) {
        return current;
    }

    for (const auto& child : current->GetChildren()) {
        auto foundNode = FindNodeByKeyRecursive(child, key);
        if (foundNode) return foundNode;
    }

    return nullptr;  // Return nullptr if not found
}

// Save configuration
void OpNodeMgr::SaveConfig(const std::string& filepath) {
    // Logic to serialize and save the configuration
    std::ofstream file(filepath);
    if (file.is_open()) {
        // Example serialization logic
        for (const auto& root : root_nodes_) {
            file << root->GetName() << "\n";  // Just an example, actual serialization will be more complex
        }
        file.close();
    }
    else {
        std::cerr << "Unable to open file for saving configuration: " << filepath << std::endl;
    }
}

// Load configuration
void OpNodeMgr::LoadConfig(const std::string& filepath) {
    // Logic to load and deserialize the configuration
    std::ifstream file(filepath);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            // Example deserialization logic
            auto node = std::make_shared<OpNode>(line);  // Assuming line represents a node name
            AddRootNode(node);
        }
        file.close();
    }
    else {
        std::cerr << "Unable to open file for loading configuration: " << filepath << std::endl;
    }
}
