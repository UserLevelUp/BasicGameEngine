#include "../include/BgeGameModule.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
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

std::wstring LowerModuleArg(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
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
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            auto& slots = *runtime.objectSlots;
            for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
                slots[index] = BgeObjectSlotState{};
                bulletLifeSeconds_[index] = 0.0f;
            }

            gameMode_ = true;
            score_ = 0;
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
        }

        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
        statusText = L"Asteroid Game: player selected; W/Up thrust, S/Down reverse, A/D rotate, Space fire";
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
        if (command == L"asteroid-game" || command == L"asteroids" || command == L"game") {
            return OnStart(runtime, statusText);
        }

        if (command == L"asteroid") {
            std::wstring subcommand = tokens.size() >= 2 ? LowerModuleArg(tokens[1]) : L"status";
            if (subcommand == L"game" || subcommand == L"play" || subcommand == L"start") {
                return OnStart(runtime, statusText);
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
            if (!gameMode_ || !*runtime.animationRunning) {
                return false;
            }
            if (!SelectedAsteroidGamePlayerLocked(runtime)) {
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
        int hits = 0;
        int spawned = 0;
        int score = 0;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!gameMode_ || !*runtime.animationRunning) {
                return false;
            }

            auto& slots = *runtime.objectSlots;
            float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
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
                    spawned += SplitAsteroidGameAsteroidLocked(runtime, asteroidIndex);
                    score_ += 10;
                    score = score_;
                    ++hits;
                    dirty = true;
                    break;
                }
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
            }
        }

        if (hits > 0) {
            if (runtime.setStatus) {
                runtime.setStatus(L"Asteroid hit: score " + std::to_wstring(score) + L", split pieces " + std::to_wstring(spawned));
            }
            if (runtime.log) {
                std::ostringstream message;
                message << "[AsteroidGame] module-hit count=" << hits << " spawned=" << spawned << " score=" << score;
                runtime.log(message.str());
            }
        }
        return dirty;
    }

private:
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
    std::array<float, BGE_OBJECT_SLOT_COUNT> bulletLifeSeconds_{};
};

} // namespace

BgeGameModule& BgeAsteroidGameModule()
{
    static AsteroidGameModule module;
    return module;
}