#include "../../include/op/OpNode.h"
#include "../../include/op/IOperate.h"
#include <algorithm> // For std::remove

// Constructors
OpNode::OpNode() : name_(""), text_(""), tag_("") {}
OpNode::OpNode(const std::string& name) : name_(name), text_(""), tag_("") {}
OpNode::OpNode(const std::string& name, const std::string& tag) : name_(name), text_(""), tag_(tag) {}
OpNode::OpNode(const std::string& name, int img1, int img2) : name_(name), text_(""), tag_("") {}

// Destructor
OpNode::~OpNode() {}

// Add a child node
void OpNode::AddChild(const std::shared_ptr<OpNode>& child) {
    children_.push_back(child);
}

// Remove a child node
void OpNode::RemoveChild(const std::shared_ptr<OpNode>& child) {
    children_.remove(child);
}

// Add an operation to the node
void OpNode::AddOperation(const std::shared_ptr<IOperate>& operation) {
    operations_.push_back(operation);
}

// Remove an operation from the node
void OpNode::RemoveOperation(const std::shared_ptr<IOperate>& operation) {
    operations_.remove_if([&](const std::shared_ptr<IOperate>& op) { return op == operation; });
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

// Additional methods
void OpNode::AddAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value; // Correct map usage
}

std::string OpNode::GetValue(const std::string& key) const {
    auto it = attributes_.find(key);
    if (it != attributes_.end()) {
        return it->second;
    }
    return "";
}

// ... other methods remain the same
