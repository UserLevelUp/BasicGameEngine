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
