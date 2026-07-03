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
// Dedicated UFO slot, sitting just below the bullet reserve. Without this,
// asteroid splits would fill slots 1..N-bulletReserve-1 and starve the UFO
// finder, so a UFO would silently fail to spawn for an entire game.
constexpr int BGE_ASTEROID_GAME_UFO_RESERVED_SLOT = BGE_OBJECT_SLOT_COUNT - BGE_ASTEROID_GAME_RESERVED_BULLET_SLOTS - 1;
constexpr int BGE_ASTEROID_GAME_STARTING_LIVES = 3;
constexpr float BGE_ASTEROID_GAME_RESPAWN_INVULNERABLE_SECONDS = 1.50f;
constexpr float BGE_ASTEROID_GAME_HYPERSPACE_INVULNERABLE_SECONDS = 0.85f;
constexpr int BGE_ASTEROID_GAME_LARGE_ASTEROID_POINTS = 20;
constexpr int BGE_ASTEROID_GAME_MEDIUM_ASTEROID_POINTS = 50;
constexpr int BGE_ASTEROID_GAME_SMALL_ASTEROID_POINTS = 100;
constexpr float BGE_ASTEROID_GAME_UFO_RADIUS = 20.0f;
constexpr float BGE_ASTEROID_GAME_UFO_SPEED = 170.0f;
constexpr float BGE_ASTEROID_GAME_UFO_MIN_ARRIVAL_SECONDS = 7.0f;
constexpr float BGE_ASTEROID_GAME_UFO_MAX_ARRIVAL_SECONDS = 18.0f;
constexpr int BGE_ASTEROID_GAME_UFO_POINTS = 200;

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

float RandomFloatInRange(float minimum, float maximum)
{
    if (maximum <= minimum) {
        return minimum;
    }
    float unit = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minimum + (maximum - minimum) * unit;
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
    return L"Asteroid Game | asteroid game/restart | A/D turn | W/S thrust | Space fire | H hyperspace | P pause";
}

std::wstring AsteroidGameCommandText()
{
    return L"Asteroid commands: asteroid title | asteroid hud | asteroid status | asteroid commands | asteroid pause/resume | asteroid hyperspace | asteroid player set/select | asteroid add/remove/count | asteroid fire | asteroid score/lives | restart";
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
            ufoSlotIndex_ = -1;
            ufoViewMode_ = 0;
            trialGroupFocus_ = BgeTrialGroup::Ufo;
            ResetAsteroidGameUfoArrivalTimerLocked();
            gameOver_ = false;
            victory_ = false;
            *runtime.mainPlayerGroupIndex = *runtime.activeObjectGroupIndex;
            *runtime.mainPlayerSlot = 0;
            *runtime.selectedObjectSlot = 0;
            *runtime.objectSelectionActive = false;
            if (runtime.setObjectKeyboardFocusLocked) {
                runtime.setObjectKeyboardFocusLocked();
            }

            ConfigureAsteroidGamePlayerLocked(runtime, 0, viewport.width * 0.50f, viewport.playTop + viewport.playHeight * 0.50f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 1, viewport.width * 0.22f, viewport.playTop + viewport.playHeight * 0.26f, 64.0f, 78.0f, 52.0f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 2, viewport.width * 0.76f, viewport.playTop + viewport.playHeight * 0.30f, 58.0f, -72.0f, 64.0f);
            ConfigureAsteroidGameAsteroidLocked(runtime, 3, viewport.width * 0.54f, viewport.playTop + viewport.playHeight * 0.76f, 52.0f, 54.0f, -86.0f);
            std::wstring ufoStatus;
            ApplyAsteroidGameUfoViewModeLocked(runtime, ufoViewMode_, viewport, ufoStatus);

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
        statusText = L"Asteroid Game: score 0, lives " + std::to_wstring(lives_) + L"; " + BuildAsteroidGameTrialStatusTextLocked();
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
        if (command == L"pause") {
            return HandlePauseCommand(runtime, statusText);
        }
        if (command == L"resume") {
            return HandleResumeCommand(runtime, statusText);
        }
        if (command == L"hyperspace" || command == L"hyper") {
            return HandleHyperspaceCommand(runtime, statusText);
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
            if (subcommand == L"pause") {
                return HandlePauseCommand(runtime, statusText);
            }
            if (subcommand == L"resume") {
                return HandleResumeCommand(runtime, statusText);
            }
            if (subcommand == L"hyperspace" || subcommand == L"hyper") {
                return HandleHyperspaceCommand(runtime, statusText);
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

        int digitMode = BgeDigitModeIndexFromKey(key);
        bool isTrialGroupKey = key == VK_PRIOR || key == VK_NEXT;
        bool isGameKey = digitMode >= 0 || isTrialGroupKey || key == L'A' || key == L'a' || key == L'D' || key == L'd' || key == L'W' || key == L'w' || key == L'S' || key == L's' || key == L'H' || key == L'h' || key == L'P' || key == L'p' || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_SPACE;
        if (!isGameKey || !RuntimeReady(runtime)) {
            return false;
        }

        bool handled = false;
        bool noPlayerSelected = false;
        int bulletSlot = -1;
        std::wstring statusText;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                return false;
            }
            if (key == L'P' && !gameOver_ && !victory_) {
                *runtime.animationRunning = !*runtime.animationRunning;
                handled = true;
                statusText = *runtime.animationRunning ? L"Asteroid Game resumed" : L"Asteroid Game paused";
            }
            else if (gameOver_ || victory_ || !*runtime.animationRunning) {
                handled = true;
                statusText = BuildAsteroidGameStatusLocked(runtime);
            }
            else if (isTrialGroupKey) {
                trialGroupFocus_ = BgeCycleTrialGroup(trialGroupFocus_, key == VK_PRIOR ? -1 : 1);
                handled = true;
                statusText = BuildAsteroidGameTrialStatusTextLocked();
            }
            else if (digitMode >= 0) {
                if (trialGroupFocus_ == BgeTrialGroup::Ufo) {
                    BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                    handled = ApplyAsteroidGameUfoViewModeLocked(runtime, digitMode, viewport, statusText);
                }
                else {
                    handled = ApplyAsteroidGamePlayerIconVisibilityModeLocked(runtime, digitMode, statusText);
                }
            }
            else if (!AsteroidGamePlayerAliveLocked(runtime, *runtime.mainPlayerSlot)) {
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
            else if (key == L'H') {
                handled = HyperspaceAsteroidGamePlayerLocked(runtime);
                statusText = handled ? L"Asteroid Game: hyperspace" : L"Asteroid Game: hyperspace unavailable";
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
                hudText = BuildAsteroidGameHudLocked(runtime);
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
            if (!hudText.empty() && runtime.setHud) {
                runtime.setHud(hudText);
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
        int ufoHits = 0;
        int asteroidsRemaining = 0;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_ || !*runtime.animationRunning) {
                return false;
            }

            auto& slots = *runtime.objectSlots;
            float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
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

            if (!gameOver_ && !victory_) {
                int activeUfoSlot = ActiveAsteroidGameUfoSlotLocked(runtime);
                if (activeUfoSlot >= 0) {
                    const BgeObjectSlotState& ufo = slots[activeUfoSlot];
                    float margin = (std::max)(40.0f, ufo.radius * 3.0f);
                    if (ufo.x < -margin || ufo.x > viewport.width + margin || ufo.y < viewport.playTop - margin || ufo.y > viewport.playTop + viewport.playHeight + margin) {
                        HideAsteroidGameSlotLocked(runtime, activeUfoSlot);
                        ResetAsteroidGameUfoArrivalTimerLocked();
                        dirty = true;
                    }
                }
                else {
                    ufoArrivalSeconds_ = (std::max)(0.0f, ufoArrivalSeconds_ - deltaSeconds);
                    if (ufoArrivalSeconds_ <= 0.0f) {
                        if (SpawnAsteroidGameUfoLocked(runtime, viewport)) {
                            dirty = true;
                        }
                        ResetAsteroidGameUfoArrivalTimerLocked();
                    }
                }
            }

            for (int bulletIndex = 0; bulletIndex < BGE_OBJECT_SLOT_COUNT; ++bulletIndex) {
                BgeObjectSlotState& bullet = slots[bulletIndex];
                if (!bullet.visible || bullet.isDeleted || bullet.kind != BgeObjectKind::Bullet) {
                    continue;
                }

                for (int ufoIndex = 0; ufoIndex < BGE_OBJECT_SLOT_COUNT; ++ufoIndex) {
                    BgeObjectSlotState& ufo = slots[ufoIndex];
                    if (!ufo.visible || ufo.isDeleted || ufo.kind != BgeObjectKind::Ufo) {
                        continue;
                    }
                    if (!BgeObjectSlotsOverlap(bullet, ufo)) {
                        continue;
                    }

                    HideAsteroidGameSlotLocked(runtime, bulletIndex);
                    HideAsteroidGameSlotLocked(runtime, ufoIndex);
                    ResetAsteroidGameUfoArrivalTimerLocked();
                    score_ += BGE_ASTEROID_GAME_UFO_POINTS;
                    points += BGE_ASTEROID_GAME_UFO_POINTS;
                    score = score_;
                    ++ufoHits;
                    dirty = true;
                    break;
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
                        if (!asteroid.visible || asteroid.isDeleted || (asteroid.kind != BgeObjectKind::Asteroid && asteroid.kind != BgeObjectKind::Ufo)) {
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
                        if (asteroid.kind == BgeObjectKind::Ufo) {
                            HideAsteroidGameSlotLocked(runtime, asteroidIndex);
                            ResetAsteroidGameUfoArrivalTimerLocked();
                        }
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
        else if (ufoHits > 0) {
            if (runtime.setStatus) {
                runtime.setStatus(L"UFO hit: +" + std::to_wstring(points) + L", score " + std::to_wstring(score) + L", lives " + std::to_wstring(lives_));
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-ufo-hit count=" << ufoHits << " points=" << points << " score=" << score;
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
        int ufoCount = CountKindLocked(runtime, BgeObjectKind::Ufo);
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
               << L", ufo " << ufoCount
               << L", " << state
               << L", " << BuildAsteroidGameTrialStatusTextLocked();
        return status.str();
    }

    std::wstring BuildAsteroidGameTrialStatusTextLocked() const
    {
        std::wstring status = L"test group ";
        status += BgeTrialGroupName(trialGroupFocus_);
        status += L" | PageUp/PageDown | 0-9 ";
        if (trialGroupFocus_ == BgeTrialGroup::Ufo) {
            status += L"UFO view " + std::to_wstring(BgeNormalizeUfoViewModeIndex(ufoViewMode_))
                + L" " + BgeUfoViewModeName(ufoViewMode_);
        }
        else {
            status += L"ship view " + std::to_wstring(BgeNormalizePlayerIconVisibilityModeIndex(playerIconVisibilityMode_))
                + L" " + BgePlayerIconVisibilityModeName(playerIconVisibilityMode_);
        }
        return status;
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
        if (gameMode_) {
            return L"PAUSED";
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
            << L" | UFO " << CountKindLocked(runtime, BgeObjectKind::Ufo)
            << L" | BUL " << CountKindLocked(runtime, BgeObjectKind::Bullet)
            << L" | TEST " << BgeTrialGroupName(trialGroupFocus_)
            << L" | " << BuildAsteroidGameStateTextLocked(runtime);
        if (gameOver_) {
            hud << L" | restart";
        }
        else if (victory_) {
            hud << L" | field cleared | restart";
        }
        else if (gameMode_ && !*runtime.animationRunning) {
            hud << L" | paused | resume/restart";
        }
        else {
            hud << L" | A/D turn W/S thrust Space fire H hyperspace P pause";
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

    bool HandlePauseCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                statusText = L"Use: asteroid game before pause";
                return false;
            }
            if (gameOver_ || victory_) {
                statusText = BuildAsteroidGameStatusLocked(runtime);
                hudText = BuildAsteroidGameHudLocked(runtime);
            }
            else {
                *runtime.animationRunning = false;
                statusText = L"Asteroid Game paused";
                hudText = BuildAsteroidGameHudLocked(runtime);
            }
        }
        if (runtime.setHud) {
            runtime.setHud(hudText);
        }
        return true;
    }

    bool HandleResumeCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                statusText = L"Use: asteroid game before resume";
                return false;
            }
            if (gameOver_ || victory_) {
                statusText = BuildAsteroidGameStatusLocked(runtime) + L"; use restart";
                hudText = BuildAsteroidGameHudLocked(runtime);
            }
            else {
                *runtime.animationRunning = true;
                statusText = L"Asteroid Game resumed";
                hudText = BuildAsteroidGameHudLocked(runtime);
            }
        }
        if (runtime.setHud) {
            runtime.setHud(hudText);
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        return true;
    }

    bool HandleHyperspaceCommand(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!RuntimeReady(runtime)) {
            statusText = L"Asteroid Game runtime unavailable";
            return false;
        }

        bool moved = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_) {
                statusText = L"Use: asteroid game before hyperspace";
                return false;
            }
            if (gameOver_ || victory_ || !*runtime.animationRunning) {
                statusText = BuildAsteroidGameStatusLocked(runtime) + L"; use resume/restart";
                PublishAsteroidGameHudLocked(runtime);
                return false;
            }
            moved = HyperspaceAsteroidGamePlayerLocked(runtime);
            if (!moved) {
                statusText = L"Asteroid Game: hyperspace unavailable";
                return false;
            }
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
        statusText = L"Asteroid Game: hyperspace";
        if (runtime.log) {
            runtime.log("[AsteroidGame] module-command hyperspace");
        }
        return moved;
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
            *runtime.objectSelectionActive = false;
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
                *runtime.objectSelectionActive = false;
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
        bool wasUfo = slot.kind == BgeObjectKind::Ufo;
        slot.visible = false;
        slot.deleteMarked = false;
        slot.isDeleted = false;
        slot.collisionDetected = false;
        slot.kind = BgeObjectKind::Generic;
        bulletLifeSeconds_[slotIndex] = 0.0f;
        if (wasUfo && ufoSlotIndex_ == slotIndex) {
            ufoSlotIndex_ = -1;
        }
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

    // The UFO gets a dedicated slot so asteroid debris cannot starve it.
    // Bullets, asteroids, and split children must all skip it.
    bool IsAsteroidGameUfoReserveSlotLocked(BgeGameRuntime& runtime, int slotIndex) const
    {
        return slotIndex == BGE_ASTEROID_GAME_UFO_RESERVED_SLOT
            && !(slotIndex == *runtime.mainPlayerSlot && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex);
    }

    int FindReusableAsteroidGameBulletSlotLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            if (!IsAsteroidGameBulletReserveSlotLocked(runtime, index)) {
                continue;
            }
            if (IsAsteroidGameUfoReserveSlotLocked(runtime, index)) {
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
            if (IsAsteroidGameUfoReserveSlotLocked(runtime, index)) {
                continue;
            }
            if (!slots[index].visible || slots[index].isDeleted) {
                return index;
            }
        }

        return -1;
    }

    void ResetAsteroidGameUfoArrivalTimerLocked()
    {
        ufoArrivalSeconds_ = RandomFloatInRange(BGE_ASTEROID_GAME_UFO_MIN_ARRIVAL_SECONDS, BGE_ASTEROID_GAME_UFO_MAX_ARRIVAL_SECONDS);
    }

    int ActiveAsteroidGameUfoSlotLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            const BgeObjectSlotState& slot = slots[index];
            if (slot.visible && !slot.isDeleted && slot.kind == BgeObjectKind::Ufo) {
                return index;
            }
        }
        return -1;
    }

    int FindReusableAsteroidGameUfoSlotLocked(BgeGameRuntime& runtime) const
    {
        auto& slots = *runtime.objectSlots;
        // Prefer the dedicated UFO reserve slot. It cannot be a bullet,
        // an asteroid, or the player, so if it is free the UFO can always
        // spawn even when the field is crowded with split asteroid debris.
        int reserved = BGE_ASTEROID_GAME_UFO_RESERVED_SLOT;
        if (reserved >= 0 && reserved < BGE_OBJECT_SLOT_COUNT
            && !(reserved == *runtime.mainPlayerSlot && *runtime.mainPlayerGroupIndex == *runtime.activeObjectGroupIndex)
            && (!slots[reserved].visible || slots[reserved].isDeleted)) {
            return reserved;
        }
        // Fallback: scan the rest, still skipping bullet reserve.
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

    bool SpawnAsteroidGameUfoLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        if (ActiveAsteroidGameUfoSlotLocked(runtime) >= 0) {
            return false;
        }
        int slotIndex = FindReusableAsteroidGameUfoSlotLocked(runtime);
        if (slotIndex < 0) {
            return false;
        }

        bool fromLeft = RandomFloatInRange(0.0f, 1.0f) < 0.5f;
        float direction = fromLeft ? 1.0f : -1.0f;
        float radius = BGE_ASTEROID_GAME_UFO_RADIUS;
        float minY = viewport.playTop + radius * 1.8f;
        float maxY = (std::max)(minY, viewport.playTop + viewport.playHeight - radius * 1.8f);

        BgeObjectSlotState& slot = (*runtime.objectSlots)[slotIndex];
        slot = BgeObjectSlotState{};
        slot.visible = true;
        slot.x = fromLeft ? -radius * 2.5f : viewport.width + radius * 2.5f;
        slot.y = RandomFloatInRange(minY, maxY);
        slot.radius = radius;
        slot.velocityX = direction * BGE_ASTEROID_GAME_UFO_SPEED;
        slot.velocityY = RandomFloatInRange(-38.0f, 38.0f);
        slot.headingX = direction;
        slot.headingY = 0.0f;
        slot.colorR = 0.84f;
        slot.colorG = 0.96f;
        slot.colorB = 1.0f;
        slot.colorA = 1.0f;
        slot.shape = BgeObjectShape::Ufo;
        slot.kind = BgeObjectKind::Ufo;
        slot.renderStyle = BgeObjectRenderStyle::Outline;
        slot.outlineThickness = 2.0f;
        ufoSlotIndex_ = slotIndex;
        return true;
    }

    bool ApplyAsteroidGameUfoViewModeLocked(BgeGameRuntime& runtime, int modeIndex, const BgeGameViewport& viewport, std::wstring& statusText)
    {
        int slotIndex = ActiveAsteroidGameUfoSlotLocked(runtime);
        if (slotIndex < 0) {
            slotIndex = FindReusableAsteroidGameUfoSlotLocked(runtime);
        }
        if (slotIndex < 0) {
            statusText = L"Asteroid Game: UFO test group has no object slot";
            return false;
        }

        ufoViewMode_ = BgeNormalizeUfoViewModeIndex(modeIndex);
        BgeObjectSlotState& slot = (*runtime.objectSlots)[slotIndex];
        BgeApplyUfoViewMode(slot, ufoViewMode_, viewport.width, viewport.playTop, viewport.playHeight);
        ufoSlotIndex_ = slotIndex;
        *runtime.selectedObjectSlot = slotIndex;
        *runtime.objectSelectionActive = false;
        BgeUpdateCollisionFlags(*runtime.objectSlots);
        if (runtime.refreshSelectedObjectGlobalsLocked) {
            runtime.refreshSelectedObjectGlobalsLocked();
        }
        if (runtime.persistActiveObjectGroupLocked) {
            runtime.persistActiveObjectGroupLocked();
        }
        *runtime.rendererStateDirty = true;
        statusText = L"Asteroid Game: " + BuildAsteroidGameTrialStatusTextLocked();
        if (runtime.log) {
            std::ostringstream message;
            message << "[AsteroidGame] module-ufo-view mode=" << ufoViewMode_ << " slot=" << (slotIndex + 1);
            runtime.log(message.str());
        }
        return true;
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
        slot.velocityX = 0.0f;
        slot.velocityY = 0.0f;
        // Asteroids convention: ship spawns pointing straight up. In screen
        // coordinates Y+ is down, so "up" is heading (0, -1). Spawning with
        // (1, 0) made the icon visibly face right when the player pressed
        // any 0-9 visibility key (the mode re-apply caused a brief radius
        // change that read as a "blink" and revealed the wrong heading).
        slot.headingX = 0.0f;
        slot.headingY = -1.0f;
        slot.colorR = 0.10f;
        slot.colorG = 0.82f;
        slot.colorB = 0.95f;
        slot.colorA = 1.0f;
        slot.shape = BgeObjectShape::VectorShip;
        slot.kind = BgeObjectKind::Player;
        BgeApplyPlayerIconVisibilityMode(slot, playerIconVisibilityMode_, false);
        playerHeadingX_ = 0.0f;
        playerHeadingY_ = -1.0f;
        ApplyAsteroidGamePlayerHeadingModeLocked(slot);
        bulletLifeSeconds_[slotIndex] = 0.0f;
    }

    void ApplyAsteroidGamePlayerHeadingModeLocked(BgeObjectSlotState& player)
    {
        BgeApplyPlayerIconHeadingMode(player, playerHeadingX_, playerHeadingY_, playerIconVisibilityMode_);
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
        BgeObjectSlotState& player = (*runtime.objectSlots)[playerSlot];
        BgeApplyPlayerIconVisibilityMode(player, playerIconVisibilityMode_, respawnInvulnerableSeconds_ > 0.0f);
        ApplyAsteroidGamePlayerHeadingModeLocked(player);
    }

    bool ApplyAsteroidGamePlayerIconVisibilityModeLocked(BgeGameRuntime& runtime, int modeIndex, std::wstring& statusText)
    {
        int playerSlot = *runtime.mainPlayerSlot;
        if (!AsteroidGamePlayerAliveLocked(runtime, playerSlot)) {
            return false;
        }

        playerIconVisibilityMode_ = BgeNormalizePlayerIconVisibilityModeIndex(modeIndex);
        trialGroupFocus_ = BgeTrialGroup::PlayerShip;
        BgeObjectSlotState& player = (*runtime.objectSlots)[playerSlot];
        BgeApplyPlayerIconVisibilityMode(player, playerIconVisibilityMode_, respawnInvulnerableSeconds_ > 0.0f);
        ApplyAsteroidGamePlayerHeadingModeLocked(player);
        *runtime.selectedObjectSlot = playerSlot;
        *runtime.objectSelectionActive = false;
        statusText = L"Asteroid Game: player icon mode " + std::to_wstring(playerIconVisibilityMode_)
            + L" (" + BgePlayerIconVisibilityModeName(playerIconVisibilityMode_) + L") | "
            + BuildAsteroidGameTrialStatusTextLocked();
        return true;
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
        *runtime.objectSelectionActive = false;
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
        if (!AsteroidGamePlayerAliveLocked(runtime, *runtime.mainPlayerSlot)) {
            return false;
        }

        BgeObjectSlotState& player = (*runtime.objectSlots)[*runtime.mainPlayerSlot];

        // Rotate the heading vector independently of velocity so the ship
        // can turn in place without being forced to drift.
        float headingX = playerHeadingX_;
        float headingY = playerHeadingY_;
        if (VectorLength(headingX, headingY) < 0.001f) {
            headingX = 0.0f;
            headingY = -1.0f;
        }
        float radians = std::atan2(headingY, headingX) + deltaDegrees * 3.14159265358979323846f / 180.0f;
        playerHeadingX_ = std::cos(radians);
        playerHeadingY_ = std::sin(radians);
        ApplyAsteroidGamePlayerHeadingModeLocked(player);
        return true;
    }

    bool ThrustAsteroidGamePlayerLocked(BgeGameRuntime& runtime, float thrustStep)
    {
        if (!AsteroidGamePlayerAliveLocked(runtime, *runtime.mainPlayerSlot)) {
            return false;
        }

        BgeObjectSlotState& player = (*runtime.objectSlots)[*runtime.mainPlayerSlot];
        // Thrust along the heading vector, not the current velocity, so the
        // pilot decides the direction of acceleration.
        float directionX = player.headingX;
        float directionY = player.headingY;
        if (VectorLength(directionX, directionY) < 0.001f) {
            directionX = 1.0f;
            directionY = 0.0f;
        }
        player.velocityX += directionX * thrustStep;
        player.velocityY += directionY * thrustStep;
        ClampVelocity(player.velocityX, player.velocityY, BGE_ASTEROID_GAME_MAX_PLAYER_SPEED);
        return true;
    }

    bool FireAsteroidGameProjectileLocked(BgeGameRuntime& runtime, int& bulletSlot)
    {
        bulletSlot = -1;
        if (!AsteroidGamePlayerAliveLocked(runtime, *runtime.mainPlayerSlot)) {
            return false;
        }

        int targetSlot = FindReusableAsteroidGameBulletSlotLocked(runtime);
        if (targetSlot < 0) {
            return false;
        }

        auto& slots = *runtime.objectSlots;
        const BgeObjectSlotState& player = slots[*runtime.mainPlayerSlot];
        // Fire along the ship's heading, not its drift velocity.
        float directionX = player.headingX;
        float directionY = player.headingY;
        if (VectorLength(directionX, directionY) < 0.001f) {
            NormalizeVectorOrDefault(player.velocityX, player.velocityY, directionX, directionY);
        }

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

    bool PlayerOverlapsAsteroidLocked(BgeGameRuntime& runtime, int playerSlot) const
    {
        auto& slots = *runtime.objectSlots;
        for (int asteroidIndex = 0; asteroidIndex < BGE_OBJECT_SLOT_COUNT; ++asteroidIndex) {
            if (asteroidIndex == playerSlot) {
                continue;
            }
            const BgeObjectSlotState& asteroid = slots[asteroidIndex];
            if (!asteroid.visible || asteroid.isDeleted || asteroid.kind != BgeObjectKind::Asteroid) {
                continue;
            }
            if (BgeObjectSlotsOverlap(slots[playerSlot], asteroid)) {
                return true;
            }
        }
        return false;
    }

    bool HyperspaceAsteroidGamePlayerLocked(BgeGameRuntime& runtime)
    {
        int playerSlot = *runtime.mainPlayerSlot;
        if (!AsteroidGamePlayerAliveLocked(runtime, playerSlot)) {
            return false;
        }

        BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
        BgeObjectSlotState& player = (*runtime.objectSlots)[playerSlot];
        float minX = player.radius;
        float maxX = (std::max)(minX, viewport.width - player.radius);
        float minY = viewport.playTop + player.radius;
        float maxY = (std::max)(minY, viewport.playTop + viewport.playHeight - player.radius);
        float fallbackX = player.x;
        float fallbackY = player.y;

        for (int attempt = 0; attempt < 12; ++attempt) {
            player.x = RandomFloatInRange(minX, maxX);
            player.y = RandomFloatInRange(minY, maxY);
            if (!PlayerOverlapsAsteroidLocked(runtime, playerSlot)) {
                break;
            }
        }
        if (PlayerOverlapsAsteroidLocked(runtime, playerSlot)) {
            player.x = fallbackX;
            player.y = fallbackY;
            return false;
        }

        respawnInvulnerableSeconds_ = BGE_ASTEROID_GAME_HYPERSPACE_INVULNERABLE_SECONDS;
        *runtime.selectedObjectSlot = playerSlot;
        *runtime.objectSelectionActive = false;
        ApplyAsteroidGamePlayerVisualLocked(runtime);
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
    int playerIconVisibilityMode_ = 0;
    float playerHeadingX_ = 0.0f;
    float playerHeadingY_ = -1.0f;
    float respawnInvulnerableSeconds_ = 0.0f;
    float ufoArrivalSeconds_ = 0.0f;
    int ufoViewMode_ = 0;
    int ufoSlotIndex_ = -1;
    BgeTrialGroup trialGroupFocus_ = BgeTrialGroup::Ufo;
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