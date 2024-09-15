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

    // Execute all operations in all root nodes
    void ExecuteAllOperations();

    // Find a node by key
    std::shared_ptr<OpNode> FindNodeByKey(const std::string& key) const;

    // Configuration management (serialization/deserialization)
    void SaveConfig(const std::string& filepath);
    void LoadConfig(const std::string& filepath);

    // Namespace management
    void AssignNamespaceToNode(std::shared_ptr<OpNode> node, const std::shared_ptr<NameSpace>& ns);
    void PropagateNamespaceFromNode(std::shared_ptr<OpNode> node);

    // Operation management
    void ApplyOperationToNode(std::shared_ptr<IOperate> operation, std::shared_ptr<OpNode> node);
    void EnforceOperationsRecursively(std::shared_ptr<OpNode> node);

private:
    // Helper for recursive search
    std::shared_ptr<OpNode> FindNodeByKeyRecursive(const std::shared_ptr<OpNode>& current, const std::string& key) const;

    // List of root nodes managed by the manager
    std::list<std::shared_ptr<OpNode>> root_nodes_;
};

#endif // OPNODEMGR_H
