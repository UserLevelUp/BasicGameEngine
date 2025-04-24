#include "pch.h"

#include "OpNode.h"
#include "IOperate.h"
#include <algorithm> // For std::remove
#include <string>
#include "NameSpace.h"


// Constructors
OpNode::OpNode() : name_(""), text_(""), tag_("") {}
OpNode::OpNode(const std::string& name) : name_(name), text_(""), tag_("") {}
OpNode::OpNode(const std::string & name, const std::any & tag) : name_(name), tag_(tag) {}
OpNode::OpNode(const std::string& name, int img1, int img2) : name_(name), text_(""), tag_("") {}

// Destructor
OpNode::~OpNode() {}

// Add a child node
void OpNode::AddChild(const std::shared_ptr<OpNode>& child) {
    children_.push_back(child);
    child->parent_ = shared_from_this();  // Set the parent when adding a child
}

// Remove a child node
void OpNode::RemoveChild(const std::shared_ptr<OpNode>& child) {
    children_.remove(child);
}

const std::list<std::shared_ptr<OpNode>>& OpNode::GetChildren() const {
    return children_;
}

std::shared_ptr<OpNode> OpNode::GetParent() const {
    return parent_;
}

size_t OpNode::GetChildIndex(const std::shared_ptr<OpNode>& child) const {
    size_t index = 0;
    for (const auto& c : children_) {
        if (c == child) {
            return index;
        }
        ++index;
    }
    return -1;  // Return -1 if the child is not found
}

void OpNode::InsertChildAt(size_t index, const std::shared_ptr<OpNode>& child) {
    auto it = children_.begin();
    std::advance(it, index);
    children_.insert(it, child);
    child->parent_ = shared_from_this();  // Set the parent of the inserted child
}


// Add an operation to the node
void OpNode::AddOperation(const std::shared_ptr<IOperate>& operation) {
    operations_.push_back(operation);
}

void OpNode::AddAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value; // Correct map usage
}

// Remove an operation from the node
void OpNode::RemoveOperation(const std::shared_ptr<IOperate>& operation) {
    operations_.remove_if([&](const std::shared_ptr<IOperate>& op) { return op == operation; });
}

void OpNode::SetAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
}

std::string OpNode::GetAttribute(const std::string& key) const {
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";  // Return empty string if the attribute is not found
}

// Getters and setters for properties
const std::string& OpNode::GetName() const {
    return name_;
}

void OpNode::SetName(const std::string& name) {
    name_ = name;
}

const std::string& OpNode::GetText() const {
    return text_;
}

void OpNode::SetText(const std::string& text) {
    text_ = text;
}

const std::any& OpNode::GetTag() const {
    return tag_;
}

void OpNode::SetTag(const std::any& tag) {
    tag_ = tag;
}

std::string OpNode::GetValue(const std::string& key) const {
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";
}

const std::map<std::string, std::string>& OpNode::GetAttributes() const {
	return attributes_;
}

const std::shared_ptr<NameSpace>& OpNode::GetNamespace() const {
    return namespace_;
}

void OpNode::SetNamespace(const std::shared_ptr<NameSpace>& ns) {
    namespace_ = ns;
}

void OpNode::DeleteAttribute(const std::string& key) {
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        attributes_.erase(it);
    }
}

std::list<std::string> OpNode::OperationIcons() const {
    std::list<std::string> icons;
    for (const auto& op : operations_) {
        if (op) {
            icons.push_back(op->Symbol());
        }
    }
    return icons;
}

void OpNode::PerformOperations() {
    // Iterate through each operation and perform it using this node as context
    for (const auto& op : operations_) {
        if (op) {
            op->Operate(shared_from_this());
        }
    }
}
