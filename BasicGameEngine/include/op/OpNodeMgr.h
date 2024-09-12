#ifndef OPNODEMGR_H
#define OPNODEMGR_H

#include "OpNode.h"
#include <list>
#include <memory>
#include <string>

// OpNode Manager class
class OpNodeMgr {
public:
    // Constructor
    OpNodeMgr();

    // Destructor
    virtual ~OpNodeMgr();

    // Manage root nodes
    void AddRootNode(std::shared_ptr<OpNode> root);
    void RemoveRootNode(std::shared_ptr<OpNode> root);

    // Execute all operations
    void ExecuteAllOperations();

    // Find a node by key
    std::shared_ptr<OpNode> FindNodeByKey(const std::string& key) const;

    // Configuration management (serialization/deserialization)
    void SaveConfig(const std::string& filepath);
    void LoadConfig(const std::string& filepath);

private:
    // Helper for recursive search
    std::shared_ptr<OpNode> FindNodeByKeyRecursive(const std::shared_ptr<OpNode>& current, const std::string& key) const;

    std::list<std::shared_ptr<OpNode>> root_nodes_;
};

#endif // OPNODEMGR_H
