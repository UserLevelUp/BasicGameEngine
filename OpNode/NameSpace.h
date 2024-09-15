#pragma once
// NameSpace.h
#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <string>
#include <memory>

class NameSpace {
public:
    // Constructors
    NameSpace() = default;
    NameSpace(const std::string& prefix, const std::string& suffix, const std::string& uri_prefix, const std::string& uri_suffix);

    // Getters and setters
    const std::string& GetPrefix() const;
    void SetPrefix(const std::string& prefix);

    const std::string& GetSuffix() const;
    void SetSuffix(const std::string& suffix);

    const std::string& GetURIPrefix() const;
    void SetURIPrefix(const std::string& uri_prefix);

    const std::string& GetURISuffix() const;
    void SetURISuffix(const std::string& uri_suffix);

    // Cloning method
    std::shared_ptr<NameSpace> Clone() const;

private:
    std::string prefix_;
    std::string suffix_;
    std::string uri_prefix_;
    std::string uri_suffix_;
};

#endif // NAMESPACE_H
