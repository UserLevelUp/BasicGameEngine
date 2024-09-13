// OpNode.h
#ifndef OPNODE_H
#define OPNODE_H

#include <string>
#include <list>
#include <memory>
#include <map>
#include <any>
#include "NameSpace.h" // Include the namespace header

class IOperate; // Forward declaration

class OpNode : public std::enable_shared_from_this<OpNode> {
public:
    OpNode();
    OpNode(const std::string& name);
    OpNode(const std::string& name, const std::string& tag);
    OpNode(const std::string& name, int img1, int img2);
    virtual ~OpNode();

    void AddChild(const std::shared_ptr<OpNode>& child);
    void RemoveChild(const std::shared_ptr<OpNode>& child);
    void AddOperation(const std::shared_ptr<IOperate>& operation);
    void RemoveOperation(const std::shared_ptr<IOperate>& operation);

    // Attribute management
    std::list<std::string> GetKeys() const;
    std::string GetValue(const std::string& key) const;
    void AddAttribute(const std::string& key, const std::string& value);
    void DeleteAttribute(const std::string& key);
    void DetectComplexNodeName();

    // Operation management
    void ClearOperations();
    int OperationsCount() const;
    std::list<std::string> OperationIcons() const;
    void PerformOperations();

    // Serialization
    void Serialize();
    void Deserialize();

    // Utility functions
    std::string GetXmlName();
    std::shared_ptr<OpNode> TreeNode2OpNode(const std::shared_ptr<OpNode>& treeNode);
    static std::shared_ptr<OpNode> OpNode2TreeNode(const std::shared_ptr<OpNode>& opNode);
    std::list<std::shared_ptr<OpNode>> Find(const std::string& searchText, int index);

    // Getters and setters for properties
    const std::string& GetName() const;
    void SetName(const std::string& name);
    const std::string& GetText() const;
    void SetText(const std::string& text);
    const std::any& GetTag() const;
    void SetTag(const std::any& tag);

    // Getter for children
    const std::list<std::shared_ptr<OpNode>>& GetChildren() const;

    // Namespace management
    const std::shared_ptr<NameSpace>& GetNamespace() const;
    void SetNamespace(const std::shared_ptr<NameSpace>& ns);

private:
    std::string name_;
    std::string text_;
    std::any tag_;
    std::map<std::string, std::string> attributes_;
    std::list<std::shared_ptr<IOperate>> operations_;
    std::list<std::shared_ptr<OpNode>> children_;
    std::shared_ptr<NameSpace> namespace_; // Use shared_ptr for namespace
    std::string errorString_;
};

#endif // OPNODE_H
