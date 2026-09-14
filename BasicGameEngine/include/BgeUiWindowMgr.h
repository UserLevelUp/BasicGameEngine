#pragma once
#include "BgeGlassUi.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <cstdint>

struct BgeUiActionViewModel {
    std::string id, label, command, target, group;
    bool enabled = true;
    bool menuItem = false;
};
enum class BgeUiActionState { Rejected, Queued, Succeeded, Failed };
struct BgeUiActionResult {
    BgeUiActionState state = BgeUiActionState::Rejected;
    std::string message;
    BgeUiActionResult() = default;
    BgeUiActionResult(bool ok) : state(ok ? BgeUiActionState::Succeeded : BgeUiActionState::Failed) {}
    BgeUiActionResult(BgeUiActionState value, std::string detail) : state(value), message(std::move(detail)) {}
    operator bool() const { return state == BgeUiActionState::Succeeded; }
    const char* Label() const {
        switch (state) {
        case BgeUiActionState::Queued: return "Queued";
        case BgeUiActionState::Succeeded: return "Completed";
        case BgeUiActionState::Failed: return "Failed";
        default: return "Rejected";
        }
    }
};
struct BgeUiActionRequest {
    uint64_t serial = 0;
    BgeUiActionViewModel action;
};
struct BgeUiTheme {
    uint32_t tint = IM_COL32(15, 24, 38, 220);
    uint32_t text = IM_COL32(255, 255, 255, 255);
    BgeGlassOverlayStyle overlay = BgeGlassOverlayStyle::None;
    bool overrideOverlay = false;
};
class BgeUiThemeMgr {
public:
    BgeUiTheme Snapshot() const { std::lock_guard<std::mutex> lock(mutex_); return theme_; }
    void Set(BgeUiTheme theme) { std::lock_guard<std::mutex> lock(mutex_); theme_ = theme; }
private:
    mutable std::mutex mutex_;
    BgeUiTheme theme_;
};
struct BgeUiWindowViewModel {
    std::string title = "BGE Command UI Spike";
    std::vector<BgeUiActionViewModel> actions;
    std::vector<BgeGlassButtonGroupDescriptor> groups;
    std::vector<BgeGlassMenuItemDescriptor> menus;
    BgeUiTheme theme;
    std::string selectedAction;
    std::string lastActionId;
    BgeUiActionResult lastResult;
};

// One manager may own one item or many. Snapshots keep renderer iteration stable.
class BgeUiWindowMgr {
public:
    BgeUiWindowViewModel Snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto model = model_;
        model.theme = theme_->Snapshot();
        for (auto& group : model.groups) for (auto& button : group.buttons) {
            button.glassTint = model.theme.tint; button.textColor = model.theme.text;
            if (model.theme.overrideOverlay) button.overlay = model.theme.overlay;
        }
        return model;
    }
    void SetTitle(std::string title) { std::lock_guard<std::mutex> lock(mutex_); model_.title = std::move(title); }
    bool Select(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actions_.find(id);
        if (it == actions_.end() || !it->second.enabled) return false;
        model_.selectedAction = id; return true;
    }
    void SetTheme(std::shared_ptr<BgeUiThemeMgr> theme) {
        if (!theme) return;
        std::lock_guard<std::mutex> lock(mutex_); theme_ = std::move(theme);
    }
    bool Register(BgeUiActionViewModel action) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (action.id.empty() || action.command.empty() || action.target.empty() || actions_.count(action.id)) return false;
        order_.push_back(action.id);
        const auto id = action.id;
        actions_.emplace(id, std::move(action));
        Refresh();
        return true;
    }
    bool Rebind(const std::string& id, const std::string& command, const std::string& target) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actions_.find(id);
        if (it == actions_.end() || command.empty() || target.empty()) return false;
        it->second.command = command; it->second.target = target;
        Refresh(); return true;
    }
    bool Move(const std::string& id, const std::string& group) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actions_.find(id);
        if (it == actions_.end()) return false;
        it->second.group = group; Refresh(); return true;
    }
    bool Remove(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!actions_.erase(id)) return false;
        if (model_.selectedAction == id) model_.selectedAction.clear();
        order_.erase(std::remove(order_.begin(), order_.end(), id), order_.end());
        Refresh(); return true;
    }
    bool SetEnabled(const std::string& id, bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actions_.find(id);
        if (it == actions_.end()) return false;
        it->second.enabled = enabled;
        if (!enabled && model_.selectedAction == id) model_.selectedAction.clear();
        Refresh(); return true;
    }
    bool Resolve(const std::string& id, BgeUiActionViewModel& action) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actions_.find(id);
        if (it == actions_.end() || !it->second.enabled) return false;
        action = it->second; return true;
    }
    bool PrepareAction(const std::string& id, BgeUiActionRequest& request) {
        std::lock_guard<std::mutex> lock(mutex_);
        request = {};
        ++requestSerial_;
        model_.lastActionId = id;
        const auto it = actions_.find(id);
        if (it == actions_.end() || !it->second.enabled) {
            model_.lastResult = {BgeUiActionState::Rejected, "Action missing or disabled"};
            return false;
        }
        request = {requestSerial_, it->second};
        model_.lastResult = {BgeUiActionState::Queued, "Waiting for command result"};
        return true;
    }
    void CompleteAction(const BgeUiActionRequest& request, BgeUiActionResult result) {
        std::lock_guard<std::mutex> lock(mutex_);
        // An older completion must not overwrite the most recently submitted action.
        if (request.serial != 0 && request.serial == requestSerial_) model_.lastResult = std::move(result);
    }
    template<class Dispatch> BgeUiActionResult Activate(const std::string& id, Dispatch dispatch) {
        BgeUiActionRequest request;
        if (!PrepareAction(id, request)) return {BgeUiActionState::Rejected, "Action missing or disabled"};
        BgeUiActionResult result = dispatch(request.action);
        CompleteAction(request, result);
        return result;
    }
    static std::shared_ptr<BgeUiWindowMgr> CreateWorkbench() {
        auto manager = std::make_shared<BgeUiWindowMgr>();
        manager->Register({"button.resolution-status", "Resolution status", "resolution status", "local", ""});
        manager->Register({"dropdown.status", "resolution status", "resolution status", "local", ""});
        manager->Register({"dropdown.plugins", "plugin commands", "plugin commands", "local", ""});
        manager->Register({"sprite.import", "Enable Animator", "plugin import bge.piece.sprite-sheet-animator", "local", "group.sprites"});
        manager->Register({"sprite.manager", "Sprite Manager", "sprite-sheet window", "local", "group.sprites"});
        manager->Register({"res.1080", "1080p FHD", "resolution 1920 1080", "local", "group.display"});
        manager->Register({"res.720", "720p HD", "resolution 1280 720", "local", "group.display"});
        manager->Register({"sprite.sheets", "List Sheets", "sprite-sheet status", "local", "group.tools"});
        manager->Register({"sprite.sequences", "List Sequences", "animation-sequence status", "local", "group.tools"});
        manager->menus_ = {
            {"file", "File", "", {{"button.resolution-status", "Resolution status", "", {}}}},
            {"sprites", "Sprites", "", {{"sprite.import", "Enable Animator", "", {}}, {"sprite.manager", "Sprite Manager", "", {}}}},
            {"display", "Display", "", {{"res.1080", "1080p FHD", "", {}}, {"res.720", "720p HD", "", {}}}},
            {"tools", "Tools", "", {{"sprite.sheets", "List Sheets", "", {}}, {"sprite.sequences", "List Sequences", "", {}}}}
        };
        manager->Refresh();
        return manager;
    }
private:
    void Refresh() {
        model_.actions.clear(); model_.groups.clear(); model_.menus = menus_;
        for (const auto& id : order_) {
            const auto& action = actions_.at(id);
            model_.actions.push_back(action);
            if (action.menuItem) {
                auto menu = std::find_if(model_.menus.begin(), model_.menus.end(), [&](const auto& value) { return value.id == action.group; });
                if (menu == model_.menus.end()) {
                    model_.menus.push_back({action.group, action.group, "", {}});
                    menu = std::prev(model_.menus.end());
                }
                menu->children.push_back({action.id, action.label, "", {}});
                continue;
            }
            if (action.group.empty()) continue;
            auto it = std::find_if(model_.groups.begin(), model_.groups.end(),
                [&](const auto& group) { return group.id == action.group; });
            if (it == model_.groups.end()) {
                BgeGlassButtonGroupDescriptor group;
                group.id = action.group; group.label = action.group;
                if (action.group == "group.sprites") group.label = "Sprite Animator";
                if (action.group == "group.display") group.label = "Display & Resolution (4-Pane Window Overlay)";
                if (action.group == "group.tools") group.label = "Sprite Catalog";
                model_.groups.push_back(group); it = std::prev(model_.groups.end());
            }
            BgeGlassButtonDescriptor button;
            button.id = action.id; button.label = action.label; button.group = action.group;
            button.enabled = action.enabled;
            // Commands stay in the manager; render models carry stable activation IDs.
            if (action.group == "group.display") button.overlay = BgeGlassOverlayStyle::WindowFourPane;
            if (action.group == "group.tools") button.overlay = BgeGlassOverlayStyle::SpecularSheen;
            it->buttons.push_back(button);
        }
    }
    mutable std::mutex mutex_;
    uint64_t requestSerial_ = 0;
    std::map<std::string, BgeUiActionViewModel> actions_;
    std::vector<std::string> order_;
    std::vector<BgeGlassMenuItemDescriptor> menus_;
    BgeUiWindowViewModel model_;
    std::shared_ptr<BgeUiThemeMgr> theme_ = std::make_shared<BgeUiThemeMgr>();
};
