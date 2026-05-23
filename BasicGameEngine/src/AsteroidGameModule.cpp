#include "../include/BgeGameModule.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cwctype>
#include <cwchar>
#include <sstream>
#include <string>
#include <windows.h>

namespace {

constexpr float BGE_ASTEROID_GAME_TURN_DEGREES = 10.0f;
constexpr float BGE_ASTEROID_GAME_THRUST_STEP = 36.0f;
constexpr float BGE_ASTEROID_GAME_REVERSE_THRUST_STEP = 24.0f;
constexpr float BGE_ASTEROID_GAME_MAX_PLAYER_SPEED = 420.0f;
constexpr float BGE_ASTEROID_GAME_BULLET_SPEED = 520.0f;
constexpr float BGE_ASTEROID_GAME_BULLET_LIFETIME_SECONDS = 1.35f;
constexpr float BGE_ASTEROID_GAME_SPLIT_MIN_RADIUS = 28.0f;
constexpr float BGE_ASTEROID_GAME_SPLIT_SCALE = 0.58f;
constexpr int BGE_ASTEROID_GAME_RESERVED_BULLET_SLOTS = 2;
constexpr int BGE_ASTEROID_GAME_STARTING_LIVES = 3;
constexpr float BGE_ASTEROID_GAME_RESPAWN_INVULNERABLE_SECONDS = 1.50f;
constexpr int BGE_ASTEROID_GAME_LARGE_ASTEROID_POINTS = 20;
constexpr int BGE_ASTEROID_GAME_MEDIUM_ASTEROID_POINTS = 50;
constexpr int BGE_ASTEROID_GAME_SMALL_ASTEROID_POINTS = 100;

std::wstring LowerModuleArg(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool TryParseFloatModuleArg(const std::wstring& text, float& value)
{
    wchar_t* end = nullptr;
    value = std::wcstof(text.c_str(), &end);
    return end != text.c_str() && end && *end == L'\0';
}

bool TryParseIntModuleArg(const std::wstring& text, int& value)
{
    wchar_t* end = nullptr;
    long parsed = std::wcstol(text.c_str(), &end, 10);
    if (end == text.c_str() || !end || *end != L'\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool TryParseSlotModuleArg(const std::wstring& text, int& slotIndex)
{
    int parsed = 0;
    if (!TryParseIntModuleArg(text, parsed) || parsed < 1 || parsed > BGE_OBJECT_SLOT_COUNT) {
        return false;
    }
    slotIndex = parsed - 1;
    return true;
}

float VectorLength(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

void NormalizeVectorOrDefault(float x, float y, float& outX, float& outY)
{
    float length = VectorLength(x, y);
    if (length < 1.0f) {
        outX = 1.0f;
        outY = 0.0f;
        return;
    }

    outX = x / length;
    outY = y / length;
}

void ClampVelocity(float& velocityX, float& velocityY, float maxSpeed)
{
    float speed = VectorLength(velocityX, velocityY);
    if (speed > maxSpeed && speed > 0.0f) {
        float scale = maxSpeed / speed;
        velocityX *= scale;
        velocityY *= scale;
    }
}

bool RuntimeReady(const BgeGameRuntime& runtime)
{
    return runtime.objectMutex
        && runtime.objectSlots
        && runtime.animationRunning
        && runtime.activeObjectGroupIndex
        && runtime.mainPlayerGroupIndex
        && runtime.mainPlayerSlot
        && runtime.selectedObjectSlot
        && runtime.objectSelectionActive
        && runtime.rendererStateDirty;
}

int AsteroidGameHitPoints(float radius)
{
    if (radius >= 54.0f) {
        return BGE_ASTEROID_GAME_LARGE_ASTEROID_POINTS;
    }
    if (radius >= 32.0f) {
        return BGE_ASTEROID_GAME_MEDIUM_ASTEROID_POINTS;
    }
    return BGE_ASTEROID_GAME_SMALL_ASTEROID_POINTS;
}

std::wstring AsteroidGameTitleText()
{
    return L"Asteroid Game | Press asteroid game/restart | A/D turn | W/S thrust | Space fire";
}

std::wstring AsteroidGameCommandText()
{
    return L"Asteroid commands: asteroid title | asteroid hud | asteroid status | asteroid commands | asteroid player set/select | asteroid add/remove/count | asteroid fire | asteroid score/lives | restart";
}

class AsteroidGameModule final : public BgeGameModule {
public:
    const wchar_t* Name() const override
    {
        return L"Asteroid Game";
    }

    bool OnStart(BgeGameRuntime& runtime, std::wstring& statusText) override
    {
        if (runtime.ownsGameLoop && !runtime.ownsGameLoop()) {
            statusText = L"Asteroid Game runs in bge.game-loop";
            return false;
        }
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            auto& slots = *runtime.objectSlots;
            for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
                slots[index] = BgeObjectSlotState{};
                bulletLifeSeconds_[index] = 0.0f;
            }

            gameMode_ = true;
            score_ = 0;
            lives_ = BGE_ASTEROID_GAME_STARTING_LIVES;
            respawnInvulnerableSeconds_ = 0.0f;
            gameOver_ = false;
            victory_ = false;
            *runtime.mainPlayerGroupIndex = *runtime.activeObjectGroupIndex;
            *runtime.mainPlayerSlot = 0;
            *runtime.selectedObjectSlot = 0;
            *runtime.objectSelectionActive = true;
            if (runtime.setObjectKeyboardFocusLocked) {
                runtime.setObjectKeyboardFocusLocked();
            }

            ConfigureAsteroidGamePlayerLocked(runtime, 0, viewport.width * 0.50f, viewport.playTop + viewport.playHeight * 0.50f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 1, viewport.width * 0.22f, viewport.playTop + viewport.playHeight * 0.26f, 64.0f, 78.0f, 52.0f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 2, viewport.width * 0.76f, viewport.playTop + viewport.playHeight * 0.30f, 58.0f, -72.0f, 64.0f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 3, viewport.width * 0.54f, viewport.playTop + viewport.playHeight * 0.76f, 52.0f, 54.0f, -86.0f);

            BgeSetCurrentEdgePolicy(BgeEdgePolicy::Wrap);
            *runtime.animationRunning = true;
            if (runtime.refreshSelectedObjectGlobalsLocked) {
                runtime.refreshSelectedObjectGlobalsLocked();
            }
            BgeUpdateCollisionFlags(slots);
            if (runtime.persistActiveObjectGroupLocked) {
                runtime.persistActiveObjectGroupLocked();
            }
            *runtime.rendererStateDirty = true;
            hudText = BuildAsteroidGameHudLocked(runtime);
        }

        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        if (runtime.setHud) {
            runtime.setHud(hudText);
        }
        statusText = L"Asteroid Game: score 0, lives " + std::to_wstring(lives_) + L"; W/Up thrust, S/Down reverse, A/D rotate, Space fire";
        if (runtime.log) {
            runtime.log("[AsteroidGame] start module=AsteroidGameModule mode=asteroid-game asteroids=3 player-slot=1 edge=wrap");
        }
        return true;
    }

    bool OnCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, std::wstring& statusText) override
    {
        if (tokens.empty()) {
            statusText = L"Asteroid Game command missing";
            return false;
        }

        std::wstring command = LowerModuleArg(tokens[0]);
        if (command == L"score") {
            return HandleScoreCommand(runtime, tokens, 1, statusText);
        }
        if (command == L"lives") {
            return HandleLivesCommand(runtime, tokens, 1, statusText);
        }
        if (command == L"fire") {
            return HandleFireCommand(runtime, statusText);
        }
        if (command == L"restart") {
            return OnStart(runtime, statusText);
        }
        if (command == L"asteroid-game" || command == L"asteroids" || command == L"game") {
            return OnStart(runtime, statusText);
        }

        if (command == L"asteroid") {
            std::wstring subcommand = tokens.size() >= 2 ? LowerModuleArg(tokens[1]) : L"status";
            if (subcommand == L"game" || subcommand == L"play" || subcommand == L"start") {
                return OnStart(runtime, statusText);
            }
            if (subcommand == L"status" || subcommand == L"state") {
                return HandleStatusCommand(runtime, statusText);
            }
            if (subcommand == L"hud" || subcommand == L"header") {
                return HandleHudCommand(runtime, statusText);
            }
            if (subcommand == L"title" || subcommand == L"screen") {
                return HandleTitleCommand(runtime, statusText);
            }
            if (subcommand == L"commands" || subcommand == L"help") {
                return HandleCommandsCommand(runtime, statusText);
            }
            if (subcommand == L"restart" || subcommand == L"reset") {
                return OnStart(runtime, statusText);
            }
            if (subcommand == L"score") {
                return HandleScoreCommand(runtime, tokens, 2, statusText);
            }
            if (subcommand == L"lives") {
                return HandleLivesCommand(runtime, tokens, 2, statusText);
            }
            if (subcommand == L"fire") {
                return HandleFireCommand(runtime, statusText);
            }
            if (subcommand == L"player") {
                return HandlePlayerCommand(runtime, tokens, 2, statusText);
            }
            if (subcommand == L"add" || subcommand == L"spawn" || subcommand == L"target") {
                return HandleAsteroidAddCommand(runtime, tokens, 2, statusText);
            }
            if (subcommand == L"remove" || subcommand == L"clear") {
                return HandleAsteroidRemoveCommand(runtime, tokens, 2, statusText);
            }
            if (subcommand == L"count") {
                return HandleAsteroidCountCommand(runtime, statusText);
            }
        }

        statusText = L"Asteroid Game module command not handled";
        return false;
    }

    bool OnKeyDown(BgeGameRuntime& runtime, unsigned int key) override
    {
        if (runtime.ownsGameLoop && !runtime.ownsGameLoop()) {
            return false;
        }

        bool isGameKey = key == L'A' || key == L'D' || key == L'W' || key == L'S' || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_SPACE;
        if (!isGameKey || !RuntimeReady(runtime)) {
            return false;
        }

        bool handled = false;
        bool noPlayerSelected = false;
        int bulletSlot = -1;
        std::wstring statusText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                return false;
            }
            if (gameOver_ || victory_ || !*runtime.animationRunning) {
                handled = true;
                statusText = BuildAsteroidGameStatusLocked(runtime);
            }
            else if (!SelectedAsteroidGamePlayerLocked(runtime)) {
                noPlayerSelected = true;
            }
            else if (key == L'A' || key == VK_LEFT) {
                handled = RotateAsteroidGamePlayerLocked(runtime, -BGE_ASTEROID_GAME_TURN_DEGREES);
                statusText = L"Asteroid Game: rotate left";
            }
            else if (key == L'D' || key == VK_RIGHT) {
                handled = RotateAsteroidGamePlayerLocked(runtime, BGE_ASTEROID_GAME_TURN_DEGREES);
                statusText = L"Asteroid Game: rotate right";
            }
            else if (key == L'W' || key == VK_UP) {
                handled = ThrustAsteroidGamePlayerLocked(runtime, BGE_ASTEROID_GAME_THRUST_STEP);
                statusText = L"Asteroid Game: thrust";
            }
            else if (key == L'S' || key == VK_DOWN) {
                handled = ThrustAsteroidGamePlayerLocked(runtime, -BGE_ASTEROID_GAME_REVERSE_THRUST_STEP);
                statusText = L"Asteroid Game: reverse thrust";
            }
            else if (key == VK_SPACE) {
                handled = FireAsteroidGameProjectileLocked(runtime, bulletSlot);
                statusText = handled ? L"Asteroid Game: fire bullet " + std::to_wstring(bulletSlot + 1) : L"Asteroid Game: no projectile slot";
            }

            if (handled) {
                auto& slots = *runtime.objectSlots;
                if (runtime.refreshSelectedObjectGlobalsLocked) {
                    runtime.refreshSelectedObjectGlobalsLocked();
                }
                BgeUpdateCollisionFlags(slots);
                if (runtime.persistActiveObjectGroupLocked) {
                    runtime.persistActiveObjectGroupLocked();
                }
                *runtime.rendererStateDirty = true;
            }
        }

        if (noPlayerSelected) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Asteroid Game: select player object 1 to fly");
            }
            return true;
        }
        if (handled) {
            if (runtime.syncControls) {
                runtime.syncControls();
            }
            if (runtime.invalidateRenderer) {
                runtime.invalidateRenderer();
            }
            if (runtime.setStatus) {
                runtime.setStatus(statusText);
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-input key=" << key;
                if (bulletSlot >= 0) {
                    message << " bullet-slot=" << (bulletSlot + 1);
                }
                runtime.log(message.str());
            }
            return true;
        }
        return false;
    }

    bool OnTick(BgeGameRuntime& runtime, double deltaMilliseconds) override
    {
        if (!RuntimeReady(runtime)) {
            return false;
        }

        bool dirty = false;
        bool playerLostLife = false;
        bool gameOver = false;
        bool victory = false;
        bool invulnerabilityEnded = false;
        int hits = 0;
        int spawned = 0;
        int score = 0;
        int lives = 0;
        int points = 0;
        int asteroidsRemaining = 0;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_ || !*runtime.animationRunning) {
                return false;
            }

            auto& slots = *runtime.objectSlots;
            float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
            if (respawnInvulnerableSeconds_ > 0.0f) {
                float previousInvulnerability = respawnInvulnerableSeconds_;
                respawnInvulnerableSeconds_ = (std::max)(0.0f, respawnInvulnerableSeconds_ - deltaSeconds);
                if (previousInvulnerability > 0.0f && respawnInvulnerableSeconds_ <= 0.0f) {
                    ApplyAsteroidGamePlayerVisualLocked(runtime);
                    invulnerabilityEnded = true;
                    dirty = true;
                }
            }

            for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
                BgeObjectSlotState& slot = slots[index];
                if (slot.visible && !slot.isDeleted && slot.kind == BgeObjectKind::Bullet) {
                    bulletLifeSeconds_[index] -= deltaSeconds;
                    if (bulletLifeSeconds_[index] <= 0.0f) {
                        HideAsteroidGameSlotLocked(runtime, index);
                        dirty = true;
                    }
                }
            }

            for (int bulletIndex = 0; bulletIndex < BGE_OBJECT_SLOT_COUNT; ++bulletIndex) {
                BgeObjectSlotState& bullet = slots[bulletIndex];
                if (!bullet.visible || bullet.isDeleted || bullet.kind != BgeObjectKind::Bullet) {
                    continue;
                }

                for (int asteroidIndex = 0; asteroidIndex < BGE_OBJECT_SLOT_COUNT; ++asteroidIndex) {
                    BgeObjectSlotState& asteroid = slots[asteroidIndex];
                    if (!asteroid.visible || asteroid.isDeleted || asteroid.kind != BgeObjectKind::Asteroid) {
                        continue;
                    }
                    if (!BgeObjectSlotsOverlap(bullet, asteroid)) {
                        continue;
                    }

                    HideAsteroidGameSlotLocked(runtime, bulletIndex);
                    int hitPoints = AsteroidGameHitPoints(asteroid.radius);
                    spawned += SplitAsteroidGameAsteroidLocked(runtime, asteroidIndex);
                    score_ += hitPoints;
                    points += hitPoints;
                    score = score_;
                    ++hits;
                    dirty = true;
                    break;
                }
            }

            if (!gameOver_ && !victory_ && respawnInvulnerableSeconds_ <= 0.0f) {
                int playerSlot = *runtime.mainPlayerSlot;
                if (AsteroidGamePlayerAliveLocked(runtime, playerSlot)) {
                    for (int asteroidIndex = 0; asteroidIndex < BGE_OBJECT_SLOT_COUNT; ++asteroidIndex) {
                        BgeObjectSlotState& asteroid = slots[asteroidIndex];
                        if (!asteroid.visible || asteroid.isDeleted || asteroid.kind != BgeObjectKind::Asteroid) {
                            continue;
                        }
                        if (!BgeObjectSlotsOverlap(slots[playerSlot], asteroid)) {
                            continue;
                        }

                        lives_ = (std::max)(0, lives_ - 1);
                        lives = lives_;
                        playerLostLife = true;
                        dirty = true;
                        HideAsteroidGameBulletsLocked(runtime);
                        if (lives_ <= 0) {
                            HideAsteroidGameSlotLocked(runtime, playerSlot);
                            gameOver_ = true;
                            gameOver = true;
                            *runtime.animationRunning = false;
                        }
                        else {
                            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                            RespawnAsteroidGamePlayerLocked(runtime, viewport);
                        }
                        break;
                    }
                }
            }

            asteroidsRemaining = CountKindLocked(runtime, BgeObjectKind::Asteroid);
            if (!gameOver_ && !victory_ && asteroidsRemaining == 0) {
                victory_ = true;
                victory = true;
                *runtime.animationRunning = false;
                dirty = true;
            }

            if (dirty) {
                BgeUpdateCollisionFlags(slots);
                if (runtime.refreshSelectedObjectGlobalsLocked) {
                    runtime.refreshSelectedObjectGlobalsLocked();
                }
                if (runtime.persistActiveObjectGroupLocked) {
                    runtime.persistActiveObjectGroupLocked();
                }
                *runtime.rendererStateDirty = true;
                hudText = BuildAsteroidGameHudLocked(runtime);
            }
        }

        if (!hudText.empty() && runtime.setHud) {
            runtime.setHud(hudText);
        }

        if (gameOver) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Game over: score " + std::to_wstring(score_) + L", lives 0; use restart");
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-game-over score=" << score_;
                runtime.log(message.str());
            }
        }
        else if (victory) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Asteroid field cleared: score " + std::to_wstring(score_) + L", lives " + std::to_wstring(lives_));
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-victory score=" << score_ << " lives=" << lives_;
                runtime.log(message.str());
            }
        }
        else if (playerLostLife) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Ship hit: lives " + std::to_wstring(lives) + L", score " + std::to_wstring(score_) + L"; respawning");
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-life-lost lives=" << lives << " score=" << score_;
                runtime.log(message.str());
            }
        }
        else if (hits > 0) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Asteroid hit: +" + std::to_wstring(points) + L", score " + std::to_wstring(score) + L", lives " + std::to_wstring(lives_) + L", asteroids " + std::to_wstring(asteroidsRemaining));
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-hit count=" << hits << " spawned=" << spawned << " points=" << points << " score=" << score;
                runtime.log(message.str());
            }
        }
        else if (invulnerabilityEnded && runtime.setStatus) {
            runtime.setStatus(L"Asteroid Game: score " + std::to_wstring(score_) + L", lives " + std::to_wstring(lives_) + L", asteroids " + std::to_wstring(asteroidsRemaining));
        }
        return dirty;
    }

private:
    int CountKindLocked(const BgeGameRuntime& runtime, BgeObjectKind kind) const
    {
        int count = 0;
        for (const auto& slot : *runtime.objectSlots) {
            if (slot.visible && !slot.isDeleted && slot.kind == kind) {
                ++count;
            }
        }
        return count;
    }

    std::wstring BuildAsteroidGameStatusLocked(const BgeGameRuntime& runtime) const
    {
        int asteroidCount = CountKindLocked(runtime, BgeObjectKind::Asteroid);
        int bulletCount = CountKindLocked(runtime, BgeObjectKind::Bullet);
        std::wstring state = L"stopped";
        if (gameOver_) {
            state = L"game over";
        }
        else if (victory_) {
            state = L"cleared";
        }
        else if (respawnInvulnerableSeconds_ > 0.0f) {
            state = L"respawning";
        }
        else if (*runtime.animationRunning) {
            state = L"running";
        }

        std::wstringstream status;
        status << L"Asteroid Game: score " << score_
               << L", lives " << lives_
               << L", asteroids " << asteroidCount
               << L", bullets " << bulletCount
               << L", " << state;
        return status.str();
    }

    std::wstring BuildAsteroidGameStateTextLocked(const BgeGameRuntime& runtime) const
    {
        if (gameOver_) {
            return L"GAME OVER";
        }
        if (victory_) {
            return L"CLEARED";
        }
        if (respawnInvulnerableSeconds_ > 0.0f) {
            return L"RESPAWN";
        }
        if (*runtime.animationRunning) {
            return L"RUNNING";
        }
        return L"READY";
    }

    std::wstring BuildAsteroidGameHudLocked(const BgeGameRuntime& runtime) const
    {
        std::wstringstream hud;
        hud << L"ASTEROID GAME"
            << L" | SCORE " << score_
            << L" | LIVES " << lives_
            << L" | AST " << CountKindLocked(runtime, BgeObjectKind::Asteroid)
            << L" | BUL " << CountKindLocked(runtime, BgeObjectKind::Bullet)
            << L" | " << BuildAsteroidGameStateTextLocked(runtime);
        if (gameOver_) {
            hud << L" | restart";
        }
        else if (victory_) {
            hud << L" | field cleared | restart";
        }
        else {
            hud << L" | A/D turn W/S thrust Space fire";
        }
        return hud.str();
    }

    std::wstring BuildAsteroidGameScreenTextLocked(const BgeGameRuntime& runtime) const
    {
        if (gameOver_) {
            return L"Game Over | final score " + std::to_wstring(score_) + L" | restart to play again";
        }
        if (victory_) {
            return L"Asteroid Field Cleared | score " + std::to_wstring(score_) + L" | restart for a new field";
        }
        if (!gameMode_) {
            return AsteroidGameTitleText();
        }
        return BuildAsteroidGameHudLocked(runtime);
    }

    void PublishAsteroidGameHudLocked(BgeGameRuntime& runtime) const
    {
        if (runtime.setHud) {
            runtime.setHud(BuildAsteroidGameHudLocked(runtime));
        }
    }

    bool HandleStatusCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::lock_guard<std::mutex> lock(*runtime.objectMutex);
        statusText = BuildAsteroidGameStatusLocked(runtime);
        PublishAsteroidGameHudLocked(runtime);
        return true;
    }

    bool HandleHudCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::lock_guard<std::mutex> lock(*runtime.objectMutex);
        statusText = BuildAsteroidGameHudLocked(runtime);
        PublishAsteroidGameHudLocked(runtime);
        return true;
    }

    bool HandleTitleCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = AsteroidGameTitleText();
            if (runtime.setHud) {
                runtime.setHud(statusText);
            }
            return true;
        }

        std::lock_guard<std::mutex> lock(*runtime.objectMutex);
        statusText = BuildAsteroidGameScreenTextLocked(runtime);
        if (runtime.setHud) {
            runtime.setHud(statusText);
        }
        return true;
    }

    bool HandleCommandsCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        statusText = AsteroidGameCommandText();
        if (runtime.setHud) {
            runtime.setHud(statusText);
        }
        return true;
    }

    bool HandleScoreCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, size_t operationIndex, std::wstring& statusText)
    {
        UNREFERENCED_PARAMETER(runtime);
        std::wstring operation = operationIndex < tokens.size() ? LowerModuleArg(tokens[operationIndex]) : L"status";
        if (operation == L"status" || operation == L"show" || operation == L"current") {
            statusText = L"Asteroid Game score: " + std::to_wstring(score_);
            PublishAsteroidGameHud(runtime);
            return true;
        }
        if (operation == L"reset" || operation == L"clear") {
            score_ = 0;
            statusText = L"Asteroid Game score reset";
            PublishAsteroidGameHud(runtime);
            return true;
        }

        int points = 0;
        if (operation == L"set") {
            if (operationIndex + 1 >= tokens.size() || !TryParseIntModuleArg(tokens[operationIndex + 1], points)) {
                statusText = L"Use: score set <points>";
                return false;
            }
            score_ = (std::max)(0, points);
            statusText = L"Asteroid Game score: " + std::to_wstring(score_);
            PublishAsteroidGameHud(runtime);
            return true;
        }
        if (operation == L"add") {
            if (operationIndex + 1 >= tokens.size() || !TryParseIntModuleArg(tokens[operationIndex + 1], points)) {
                statusText = L"Use: score add <points>";
                return false;
            }
            score_ = (std::max)(0, score_ + points);
            statusText = L"Asteroid Game score: " + std::to_wstring(score_);
            PublishAsteroidGameHud(runtime);
            return true;
        }

        statusText = L"Use: score status|reset|set <points>|add <points>";
        return false;
    }

    bool HandleLivesCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, size_t operationIndex, std::wstring& statusText)
    {
        UNREFERENCED_PARAMETER(runtime);
        std::wstring operation = operationIndex < tokens.size() ? LowerModuleArg(tokens[operationIndex]) : L"status";
        if (operation == L"status" || operation == L"show" || operation == L"current") {
            statusText = L"Asteroid Game lives: " + std::to_wstring(lives_);
            PublishAsteroidGameHud(runtime);
            return true;
        }
        if (operation == L"reset") {
            lives_ = BGE_ASTEROID_GAME_STARTING_LIVES;
            statusText = L"Asteroid Game lives: " + std::to_wstring(lives_);
            PublishAsteroidGameHud(runtime);
            return true;
        }

        int count = 0;
        if (operation == L"set") {
            if (operationIndex + 1 >= tokens.size() || !TryParseIntModuleArg(tokens[operationIndex + 1], count)) {
                statusText = L"Use: lives set <count>";
                return false;
            }
            lives_ = (std::max)(0, count);
            statusText = L"Asteroid Game lives: " + std::to_wstring(lives_);
            PublishAsteroidGameHud(runtime);
            return true;
        }
        if (operation == L"add") {
            if (operationIndex + 1 >= tokens.size() || !TryParseIntModuleArg(tokens[operationIndex + 1], count)) {
                statusText = L"Use: lives add <count>";
                return false;
            }
            lives_ = (std::max)(0, lives_ + count);
            statusText = L"Asteroid Game lives: " + std::to_wstring(lives_);
            PublishAsteroidGameHud(runtime);
            return true;
        }
        if (operation == L"lose" || operation == L"lost" || operation == L"remove") {
            count = 1;
            if (operationIndex + 1 < tokens.size() && !TryParseIntModuleArg(tokens[operationIndex + 1], count)) {
                statusText = L"Use: lives lose [count]";
                return false;
            }
            lives_ = (std::max)(0, lives_ - count);
            statusText = L"Asteroid Game lives: " + std::to_wstring(lives_);
            PublishAsteroidGameHud(runtime);
            return true;
        }

        statusText = L"Use: lives status|reset|set <count>|add <count>|lose [count]";
        return false;
    }

    bool HandleFireCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        int bulletSlot = -1;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                statusText = L"Use: asteroid game before fire";
                return false;
            }
            if (gameOver_ || victory_ || !*runtime.animationRunning) {
                statusText = BuildAsteroidGameStatusLocked(runtime) + L"; use restart";
                return false;
            }
            if (!FireAsteroidGameProjectileLocked(runtime, bulletSlot)) {
                statusText = L"Asteroid Game: no projectile slot";
                return false;
            }
            BgeUpdateCollisionFlags(*runtime.objectSlots);
            if (runtime.persistActiveObjectGroupLocked) {
                runtime.persistActiveObjectGroupLocked();
            }
            *runtime.rendererStateDirty = true;
        }

        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        PublishAsteroidGameHud(runtime);
        statusText = L"Asteroid Game: fire bullet " + std::to_wstring(bulletSlot + 1);
        if (runtime.log) {
            std::ostringstream message;
            message << "[AsteroidGame] module-command fire bullet-slot=" << (bulletSlot + 1);
            runtime.log(message.str());
        }
        return true;
    }

    bool HandlePlayerCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, size_t operationIndex, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::wstring operation = operationIndex < tokens.size() ? LowerModuleArg(tokens[operationIndex]) : L"status";
        if (operation == L"status" || operation == L"show" || operation == L"current") {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            statusText = *runtime.mainPlayerSlot >= 0
                ? L"Asteroid Game player: object " + std::to_wstring(*runtime.mainPlayerSlot + 1)
                : L"Asteroid Game player: none";
            return true;
        }

        if (operation == L"select" || operation == L"focus") {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (*runtime.mainPlayerSlot < 0 || *runtime.mainPlayerSlot >= BGE_OBJECT_SLOT_COUNT) {
                statusText = L"Asteroid Game player: none";
                return false;
            }
            *runtime.selectedObjectSlot = *runtime.mainPlayerSlot;
            *runtime.objectSelectionActive = true;
            if (runtime.refreshSelectedObjectGlobalsLocked) {
                runtime.refreshSelectedObjectGlobalsLocked();
            }
            if (runtime.persistActiveObjectGroupLocked) {
                runtime.persistActiveObjectGroupLocked();
            }
            statusText = L"Asteroid Game player selected: object " + std::to_wstring(*runtime.mainPlayerSlot + 1);
            return true;
        }

        if (operation == L"set") {
            int slotIndex = 0;
            if (operationIndex + 1 < tokens.size() && !TryParseSlotModuleArg(tokens[operationIndex + 1], slotIndex)) {
                statusText = L"Use: asteroid player set [1-10]";
                return false;
            }

            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
            {
                std::lock_guard<std::mutex> lock(*runtime.objectMutex);
                gameMode_ = true;
                gameOver_ = false;
                victory_ = false;
                respawnInvulnerableSeconds_ = BGE_ASTEROID_GAME_RESPAWN_INVULNERABLE_SECONDS;
                *runtime.animationRunning = true;
                *runtime.mainPlayerGroupIndex = *runtime.activeObjectGroupIndex;
                *runtime.mainPlayerSlot = slotIndex;
                *runtime.selectedObjectSlot = slotIndex;
                *runtime.objectSelectionActive = true;
                ConfigureAsteroidGamePlayerLocked(runtime, slotIndex, viewport.width * 0.50f, viewport.playTop + viewport.playHeight * 0.50f);
                ApplyAsteroidGamePlayerVisualLocked(runtime);
                if (runtime.setObjectKeyboardFocusLocked) {
                    runtime.setObjectKeyboardFocusLocked();
                }
                if (runtime.refreshSelectedObjectGlobalsLocked) {
                    runtime.refreshSelectedObjectGlobalsLocked();
                }
                if (runtime.persistActiveObjectGroupLocked) {
                    runtime.persistActiveObjectGroupLocked();
                }
                *runtime.rendererStateDirty = true;
            }
            if (runtime.syncControls) {
                runtime.syncControls();
            }
            if (runtime.invalidateRenderer) {
                runtime.invalidateRenderer();
            }
            PublishAsteroidGameHud(runtime);
            statusText = L"Asteroid Game player set: object " + std::to_wstring(slotIndex + 1);
            return true;
        }

        statusText = L"Use: asteroid player status|select|set [1-10]";
        return false;
    }

    bool HandleAsteroidAddCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, size_t firstArgIndex, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        size_t argIndex = firstArgIndex;
        int slotIndex = -1;
        if (argIndex < tokens.size() && TryParseSlotModuleArg(tokens[argIndex], slotIndex)) {
            ++argIndex;
        }

        BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
        float x = viewport.width * 0.30f;
        float y = viewport.playTop + viewport.playHeight * 0.32f;
        float radius = 56.0f;
        float velocityX = 72.0f;
        float velocityY = 48.0f;
        if (argIndex < tokens.size()) {
            if (argIndex + 4 >= tokens.size()
                || !TryParseFloatModuleArg(tokens[argIndex], x)
                || !TryParseFloatModuleArg(tokens[argIndex + 1], y)
                || !TryParseFloatModuleArg(tokens[argIndex + 2], radius)
                || !TryParseFloatModuleArg(tokens[argIndex + 3], velocityX)
                || !TryParseFloatModuleArg(tokens[argIndex + 4], velocityY)) {
                statusText = L"Use: asteroid add [slot] [x y radius vx vy]";
                return false;
            }
        }

        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            gameMode_ = true;
            victory_ = false;
            if (lives_ > 0) {
                gameOver_ = false;
                *runtime.animationRunning = true;
            }
            if (slotIndex < 0) {
                slotIndex = FindReusableAsteroidGameAsteroidSlotLocked(runtime);
            }
            if (slotIndex < 0) {
                statusText = L"Asteroid Game: no asteroid slot";
                return false;
            }
            ConfigureAsteroidGameAsteroidLocked(runtime, slotIndex, x, y, (std::max)(8.0f, radius), velocityX, velocityY);
            BgeUpdateCollisionFlags(*runtime.objectSlots);
            if (runtime.refreshSelectedObjectGlobalsLocked) {
                runtime.refreshSelectedObjectGlobalsLocked();
            }
            if (runtime.persistActiveObjectGroupLocked) {
                runtime.persistActiveObjectGroupLocked();
            }
            *runtime.rendererStateDirty = true;
        }
        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        PublishAsteroidGameHud(runtime);
        statusText = L"Asteroid Game asteroid set: object " + std::to_wstring(slotIndex + 1);
        return true;
    }

    bool HandleAsteroidRemoveCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, size_t firstArgIndex, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }
        int slotIndex = -1;
        if (firstArgIndex >= tokens.size() || !TryParseSlotModuleArg(tokens[firstArgIndex], slotIndex)) {
            statusText = L"Use: asteroid remove <1-10>";
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            HideAsteroidGameSlotLocked(runtime, slotIndex);
            BgeUpdateCollisionFlags(*runtime.objectSlots);
            if (runtime.refreshSelectedObjectGlobalsLocked) {
                runtime.refreshSelectedObjectGlobalsLocked();
            }
            if (runtime.persistActiveObjectGroupLocked) {
                runtime.persistActiveObjectGroupLocked();
            }
            *runtime.rendererStateDirty = true;
        }
        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        PublishAsteroidGameHud(runtime);
        statusText = L"Asteroid Game object cleared: " + std::to_wstring(slotIndex + 1);
        return true;
    }

    bool HandleAsteroidCountCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::lock_guard<std::mutex> lock(*runtime.objectMutex);
        statusText = L"Asteroid Game asteroids: " + std::to_wstring(CountKindLocked(runtime, BgeObjectKind::Asteroid));
        PublishAsteroidGameHudLocked(runtime);
        return true;
    }

    void PublishAsteroidGameHud(BgeGameRuntime& runtime) const
    {
        if (!runtime.setHud || !RuntimeReady(runtime)) {
            return;
        }
        std::lock_guard<std::mutex> lock(*runtime.objectMutex);
        runtime.setHud(BuildAsteroidGameHudLocked(runtime));
    }

    void HideAsteroidGameSlotLocked(BgeGameRuntime& runtime, int slotIndex)
    {
        if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
            return;
        }

        BgeObjectSlotState& slot = (*runtime.objectSlots)[slotIndex];
        slot.visible = false;
        slot.deleteMarked = false;
        slot.isDeleted = false;
        slot.collisionDetected = false;
        slot.kind = BgeObjectKind::Generic;
        bulletLifeSeconds_[slotIndex] = 0.0f;
    }

    void HideAsteroidGameBulletsLocked(BgeGameRuntime& runtime)
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (slots[index].kind == BgeObjectKind::Bullet) {
                HideAsteroidGameSlotLocked(runtime, index);
            }
        }
    }

    bool IsAsteroidGameBulletReserveSlotLocked(BgeGameRuntime& runtime, int slotIndex) const
    {
        return slotIndex >= (BGE_OBJECT_SLOT_COUNT - BGE_ASTEROID_GAME_RESERVED_BULLET_SLOTS)
            && slotIndex < BGE_OBJECT_SLOT_COUNT
            && !(slotIndex == *runtime.mainPlayerSlot && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex);
    }

    int FindReusableAsteroidGameBulletSlotLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (!IsAsteroidGameBulletReserveSlotLocked(runtime, index)) {
                continue;
            }
            if (!slots[index].visible || slots[index].isDeleted) {
                return index;
            }
        }

        int oldestBulletSlot = -1;
        float shortestLife = 1000000.0f;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (IsAsteroidGameBulletReserveSlotLocked(runtime, index)
                && slots[index].kind == BgeObjectKind::Bullet
                && bulletLifeSeconds_[index] < shortestLife) {
                shortestLife = bulletLifeSeconds_[index];
                oldestBulletSlot = index;
            }
        }
        if (oldestBulletSlot >= 0) {
            return oldestBulletSlot;
        }

        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (index == *runtime.mainPlayerSlot && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex) {
                continue;
            }
            if (!slots[index].visible || slots[index].isDeleted) {
                return index;
            }
        }

        return -1;
    }

    int FindReusableAsteroidGameAsteroidSlotLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (index == *runtime.mainPlayerSlot && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex) {
                continue;
            }
            if (IsAsteroidGameBulletReserveSlotLocked(runtime, index)) {
                continue;
            }
            if (!slots[index].visible || slots[index].isDeleted) {
                return index;
            }
        }

        return -1;
    }

    void ConfigureAsteroidGameAsteroidLocked(BgeGameRuntime& runtime, int slotIndex, float x, float y, float radius, float velocityX, float velocityY)
    {
        if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
            return;
        }

        BgeObjectSlotState& slot = (*runtime.objectSlots)[slotIndex];
        slot = BgeObjectSlotState{};
        slot.visible = true;
        slot.x = x;
        slot.y = y;
        slot.radius = radius;
        slot.velocityX = velocityX;
        slot.velocityY = velocityY;
        slot.colorR = 0.66f;
        slot.colorG = 0.58f;
        slot.colorB = 0.48f;
        slot.colorA = 0.88f;
        slot.shape = BgeObjectShape::Asteroid;
        slot.kind = BgeObjectKind::Asteroid;
        bulletLifeSeconds_[slotIndex] = 0.0f;
    }

    void ConfigureAsteroidGamePlayerLocked(BgeGameRuntime& runtime, int slotIndex, float x, float y)
    {
        BgeObjectSlotState& slot = (*runtime.objectSlots)[slotIndex];
        slot = BgeObjectSlotState{};
        slot.visible = true;
        slot.x = x;
        slot.y = y;
        slot.radius = 18.0f;
        slot.velocityX = 120.0f;
        slot.velocityY = 0.0f;
        slot.colorR = 0.10f;
        slot.colorG = 0.82f;
        slot.colorB = 0.95f;
        slot.colorA = 1.0f;
        slot.shape = BgeObjectShape::Ball;
        slot.kind = BgeObjectKind::Player;
        bulletLifeSeconds_[slotIndex] = 0.0f;
    }

    bool AsteroidGamePlayerAliveLocked(BgeGameRuntime& runtime, int slotIndex) const
    {
        auto& slots = *runtime.objectSlots;
        return gameMode_
            && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex
            && slotIndex >= 0
            && slotIndex < BGE_OBJECT_SLOT_COUNT
            && slots[slotIndex].visible
            && !slots[slotIndex].isDeleted
            && slots[slotIndex].kind == BgeObjectKind::Player;
    }

    void ApplyAsteroidGamePlayerVisualLocked(BgeGameRuntime& runtime)
    {
        int playerSlot = *runtime.mainPlayerSlot;
        if (!AsteroidGamePlayerAliveLocked(runtime, playerSlot)) {
            return;
        }
        (*runtime.objectSlots)[playerSlot].colorA = respawnInvulnerableSeconds_ > 0.0f ? 0.48f : 1.0f;
    }

    void RespawnAsteroidGamePlayerLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        int playerSlot = *runtime.mainPlayerSlot;
        if (playerSlot < 0 || playerSlot >= BGE_OBJECT_SLOT_COUNT) {
            return;
        }
        ConfigureAsteroidGamePlayerLocked(runtime, playerSlot, viewport.width * 0.50f, viewport.playTop + viewport.playHeight * 0.50f);
        respawnInvulnerableSeconds_ = BGE_ASTEROID_GAME_RESPAWN_INVULNERABLE_SECONDS;
        *runtime.selectedObjectSlot = playerSlot;
        *runtime.objectSelectionActive = true;
        ApplyAsteroidGamePlayerVisualLocked(runtime);
    }

    bool SelectedAsteroidGamePlayerLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        return gameMode_
            && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex
            && *runtime.mainPlayerSlot >= 0
            && *runtime.mainPlayerSlot < BGE_OBJECT_SLOT_COUNT
            && *runtime.selectedObjectSlot == *runtime.mainPlayerSlot
            && slots[*runtime.mainPlayerSlot].visible
            && !slots[*runtime.mainPlayerSlot].isDeleted
            && slots[*runtime.mainPlayerSlot].kind == BgeObjectKind::Player;
    }

    bool RotateAsteroidGamePlayerLocked(BgeGameRuntime& runtime, float deltaDegrees)
    {
        if (!SelectedAsteroidGamePlayerLocked(runtime)) {
            return false;
        }

        BgeObjectSlotState& player = (*runtime.objectSlots)[*runtime.mainPlayerSlot];
        float currentSpeed = VectorLength(player.velocityX, player.velocityY);
        if (currentSpeed < 1.0f) {
            currentSpeed = 1.0f;
            player.velocityX = currentSpeed;
            player.velocityY = 0.0f;
        }

        float radians = std::atan2(player.velocityY, player.velocityX) + deltaDegrees * 3.14159265358979323846f / 180.0f;
        player.velocityX = std::cos(radians) * currentSpeed;
        player.velocityY = std::sin(radians) * currentSpeed;
        return true;
    }

    bool ThrustAsteroidGamePlayerLocked(BgeGameRuntime& runtime, float thrustStep)
    {
        if (!SelectedAsteroidGamePlayerLocked(runtime)) {
            return false;
        }

        BgeObjectSlotState& player = (*runtime.objectSlots)[*runtime.mainPlayerSlot];
        float directionX = 1.0f;
        float directionY = 0.0f;
        NormalizeVectorOrDefault(player.velocityX, player.velocityY, directionX, directionY);
        player.velocityX += directionX * thrustStep;
        player.velocityY += directionY * thrustStep;
        ClampVelocity(player.velocityX, player.velocityY, BGE_ASTEROID_GAME_MAX_PLAYER_SPEED);
        return true;
    }

    bool FireAsteroidGameProjectileLocked(BgeGameRuntime& runtime, int& bulletSlot)
    {
        bulletSlot = -1;
        if (!SelectedAsteroidGamePlayerLocked(runtime)) {
            return false;
        }

        int targetSlot = FindReusableAsteroidGameBulletSlotLocked(runtime);
        if (targetSlot < 0) {
            return false;
        }

        auto& slots = *runtime.objectSlots;
        const BgeObjectSlotState& player = slots[*runtime.mainPlayerSlot];
        float directionX = 1.0f;
        float directionY = 0.0f;
        NormalizeVectorOrDefault(player.velocityX, player.velocityY, directionX, directionY);

        BgeObjectSlotState& bullet = slots[targetSlot];
        bullet = BgeObjectSlotState{};
        bullet.visible = true;
        bullet.x = player.x + directionX * (player.radius + 10.0f);
        bullet.y = player.y + directionY * (player.radius + 10.0f);
        bullet.radius = 5.0f;
        bullet.velocityX = player.velocityX + directionX * BGE_ASTEROID_GAME_BULLET_SPEED;
        bullet.velocityY = player.velocityY + directionY * BGE_ASTEROID_GAME_BULLET_SPEED;
        bullet.colorR = 1.0f;
        bullet.colorG = 0.92f;
        bullet.colorB = 0.18f;
        bullet.colorA = 1.0f;
        bullet.shape = BgeObjectShape::Ball;
        bullet.kind = BgeObjectKind::Bullet;
        bulletLifeSeconds_[targetSlot] = BGE_ASTEROID_GAME_BULLET_LIFETIME_SECONDS;
        bulletSlot = targetSlot;
        return true;
    }

    int SpawnSplitAsteroidLocked(BgeGameRuntime& runtime, const BgeObjectSlotState& source, float radius, float angleOffsetRadians)
    {
        int slotIndex = FindReusableAsteroidGameAsteroidSlotLocked(runtime);
        if (slotIndex < 0) {
            return -1;
        }

        float sourceSpeed = VectorLength(source.velocityX, source.velocityY);
        if (sourceSpeed < 70.0f) {
            sourceSpeed = 105.0f;
        }
        float baseAngle = std::atan2(source.velocityY, source.velocityX);
        if (VectorLength(source.velocityX, source.velocityY) < 1.0f) {
            baseAngle = angleOffsetRadians;
        }
        float splitAngle = baseAngle + angleOffsetRadians;

        BgeObjectSlotState& split = (*runtime.objectSlots)[slotIndex];
        split = source;
        split.visible = true;
        split.deleteMarked = false;
        split.isDeleted = false;
        split.collisionDetected = false;
        split.radius = radius;
        split.x = source.x + std::cos(splitAngle) * radius * 0.55f;
        split.y = source.y + std::sin(splitAngle) * radius * 0.55f;
        split.velocityX = std::cos(splitAngle) * sourceSpeed * 1.12f;
        split.velocityY = std::sin(splitAngle) * sourceSpeed * 1.12f;
        split.colorA = (std::max)(0.58f, source.colorA);
        split.shape = BgeObjectShape::Asteroid;
        split.kind = BgeObjectKind::Asteroid;
        bulletLifeSeconds_[slotIndex] = 0.0f;
        return slotIndex;
    }

    int SplitAsteroidGameAsteroidLocked(BgeGameRuntime& runtime, int asteroidSlot)
    {
        if (asteroidSlot < 0 || asteroidSlot >= BGE_OBJECT_SLOT_COUNT) {
            return 0;
        }

        BgeObjectSlotState source = (*runtime.objectSlots)[asteroidSlot];
        HideAsteroidGameSlotLocked(runtime, asteroidSlot);
        if (source.radius < BGE_ASTEROID_GAME_SPLIT_MIN_RADIUS) {
            return 0;
        }

        int spawned = 0;
        float childRadius = (std::max)(14.0f, source.radius * BGE_ASTEROID_GAME_SPLIT_SCALE);
        if (SpawnSplitAsteroidLocked(runtime, source, childRadius, -0.72f) >= 0) {
            ++spawned;
        }
        if (SpawnSplitAsteroidLocked(runtime, source, childRadius, 0.72f) >= 0) {
            ++spawned;
        }
        return spawned;
    }

    bool gameMode_ = false;
    int score_ = 0;
    int lives_ = BGE_ASTEROID_GAME_STARTING_LIVES;
    float respawnInvulnerableSeconds_ = 0.0f;
    bool gameOver_ = false;
    bool victory_ = false;
    std::array<float, BGE_OBJECT_SLOT_COUNT> bulletLifeSeconds_{};
};

} // namespace

BgeGameModule& BgeAsteroidGameModule()
{
    static AsteroidGameModule module;
    return module;
}