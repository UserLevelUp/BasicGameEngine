#include "../include/BgeHierarchy.h"
#include "../../OpNode/OpNode.h"
#include "../include/BgeGameModule.h"
#include "../include/BgeAudioPluginOperation.h"
#include "../include/BgeBreakableRockFieldPluginOperation.h"
#include "../include/BgeProjectilePluginOperation.h"
#include "../include/BgeScoreboardPluginOperation.h"
#include "../include/BgeSpriteSheetAnimatorPluginOperation.h"
#include "../include/BgeTitleScreenPluginOperation.h"
#include "../include/BgeVectorShipPluginOperation.h"
#include "../include/BasicGameRoleOperation.h"
#include "../include/DirectX11BouncingBallOperation.h"

#include <algorithm>
#include <string>

// Forward declarations of helper functions defined in BasicGameEngine.cpp
std::string Narrow(const std::wstring& value);
bool CurrentProcessOwnsGameLoop();

// Global BGE plugin registry of supported pieces/capabilities
const std::array<BgePluginDescriptor, 9> kBgePluginRegistry = {{
    { L"bge.2d.arcade", L"capability", L"2D arcade viewport, counters, overlays, replay, authoring envelopes, and executable export command grammar", L"game define | viewport fit-host | counter define | author start | author group | author tool | author save | export enable | export executable | inspect commands", L"--design-size --wrap-x --wrap-y --background --formats --target --name --include", L"bge.event.game.defined bge.event.scene.configured bge.event.replay.surface.enabled bge.event.author.started bge.event.author.saved bge.event.export.requested" },
    { L"bge.piece.vector-ship", L"piece", L"Reusable vector player ship with input, firing, respawn, and hyperspace signature", L"player-ship create", L"--id --shape --lives --input-profile --fire --hyperspace --respawn --invulnerable", L"bge.event.entity.spawned bge.event.input.bound bge.event.player.respawned" },
    { L"bge.piece.breakable-rock-field", L"piece", L"Reusable breakable rock field with split, scoring, wave, and avoid-zone signature", L"rock-field create", L"--sizes --count --split --score --speed-range --wave-counter --avoid", L"bge.event.entity.spawned bge.event.rule.defined bge.event.counter.changed" },
    { L"bge.piece.projectile", L"piece", L"Reusable projectile with owner, speed, lifetime, wrap, and collision tag signature", L"projectile create", L"--id --owner --shape --speed --ttl --wrap --collision-tag", L"bge.event.entity.grammar.extended bge.event.entity.lifecycle.changed" },
    { L"bge.piece.ufo", L"piece", L"Reusable UFO enemy with score, weapon, arrival, aim, and edge-spawn signature", L"ufo create", L"--id --score --weapon --arrival --aim --edge-spawn", L"bge.event.entity.spawned bge.event.rule.fired bge.event.counter.changed" },
    { L"bge.piece.scoreboard", L"piece", L"Reusable vector scoreboard and HUD line bound to counters", L"scoreboard create", L"--counters --anchor --format --font --color --icon-row --scale", L"bge.event.overlay.defined bge.event.counter.observed" },
    { L"bge.piece.title-screen", L"piece", L"Reusable vector-arcade title screen with score legend, credit line, blinking prompt, and start command binding", L"title-screen create", L"--text --subtitle --start --next --font --center --legend --credit --blink", L"bge.event.screen.defined bge.event.command.bound bge.event.overlay.defined" },
    { L"bge.piece.sprite-sheet-animator", L"piece", L"Reusable sprite-sheet animator with sequence binding and actor animation command surface", L"sprite-sheet create | animation-sequence create | actor animation bind", L"--id --image --columns --rows --frame-size --alpha-source --alpha-cutoff --preserve-alpha --no-preserve-alpha --transparent-background --opaque-background --frames --fps --loop --tier-counter --map --show --color", L"bge.event.sprite.sheet.defined bge.event.sprite.sequence.defined bge.event.sprite.binding.defined" },
    { L"bge.piece.audio", L"piece", L"Engine-neutral audio piece with synthesized-tone WAV playback via winmm; any domain (game, business-rules, DICOM alert) can consume it", L"sound define | sound play", L"--id --kind --freq --duration --volume", L"bge.event.sound.defined bge.event.sound.played" },
}};

void AddBgePluginRegistryAttributesToOpNode(const std::shared_ptr<OpNode>& root)
{
    if (!root) {
        return;
    }

    root->SetAttribute("plugin.registry", "enabled");
    root->SetAttribute("plugin.registry.source", "bge.static.command-signatures");
    for (const auto& descriptor : kBgePluginRegistry) {
        std::string key = std::string("plugin.") + Narrow(descriptor.id);
        root->SetAttribute(key, "available");
        root->SetAttribute(key + ".kind", Narrow(descriptor.kind));
        root->SetAttribute(key + ".command", Narrow(descriptor.command));
        root->SetAttribute(key + ".flags", Narrow(descriptor.flags));
        root->SetAttribute(key + ".emits", Narrow(descriptor.emits));
    }
}

void BootstrapRoleOpNode()
{
    std::string role = g_isController ? "bge.controller" : Narrow(g_workerName);
    auto root = std::make_shared<OpNode>(role.empty() ? "bge.worker" : role);
    root->SetAttribute("bge.role", g_isController ? "controller" : "worker");
    root->SetAttribute("bge.worker.name", role);
    root->SetAttribute("runtime.game-loop", CurrentProcessOwnsGameLoop() ? "enabled" : "disabled");
    root->SetAttribute("plugin.opnode", "enabled");
    root->SetAttribute("plugin.game-loop", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.directx11", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.directx12", "available");
    root->SetAttribute("animation.2d.bouncing-ball", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.scene-3d", (role == "bge.scene-3d") ? "enabled" : "available");
    root->SetAttribute("plugin.images", (role == "bge.images") ? "enabled" : "available");
    root->SetAttribute("plugin.sound", (role == "bge.sound") ? "enabled" : "available");
    root->SetAttribute("plugin.sample-game", (role == "bge.sample-game-one" || role == "bge.sample-game-two") ? "enabled" : "available");
    AddBgePluginRegistryAttributesToOpNode(root);
    root->SetAttribute("environment", "basic-3d");
    if (role == "bge.game-loop") {
        root->AddOperation(std::make_shared<DirectX11BouncingBallOperation>());
        root->AddOperation(std::make_shared<BgeAudioPluginOperation>());
        root->AddOperation(std::make_shared<BgeBreakableRockFieldPluginOperation>());
        root->AddOperation(std::make_shared<BgeProjectilePluginOperation>());
        root->AddOperation(std::make_shared<BgeScoreboardPluginOperation>());
        root->AddOperation(std::make_shared<BgeSpriteSheetAnimatorPluginOperation>());
        root->AddOperation(std::make_shared<BgeTitleScreenPluginOperation>());
        root->AddOperation(std::make_shared<BgeVectorShipPluginOperation>());
    }
    root->AddOperation(std::make_shared<BasicGameRoleOperation>());
    root->PerformOperations();
}
