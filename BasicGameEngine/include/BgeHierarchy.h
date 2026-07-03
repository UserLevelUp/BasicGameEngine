#ifndef BGE_HIERARCHY_H
#define BGE_HIERARCHY_H

#include <string>
#include <array>
#include <memory>
#include "../../OpNode/OpNode.h"

// Struct for BGE Plugin Descriptors
struct BgePluginDescriptor {
    const wchar_t* id;
    const wchar_t* kind;
    const wchar_t* summary;
    const wchar_t* command;
    const wchar_t* flags;
    const wchar_t* emits;
};

// Extern variables defined in BasicGameEngine.cpp but referenced in of BgeHierarchy
extern bool g_isController;
extern std::wstring g_workerName;

// Global BGE plugin registry of supported pieces/capabilities
extern const std::array<BgePluginDescriptor, 9> kBgePluginRegistry;

// Hierarchy initialization and discovery
void BootstrapRoleOpNode();
void AddBgePluginRegistryAttributesToOpNode(const std::shared_ptr<OpNode>& root);

#endif // BGE_HIERARCHY_H
