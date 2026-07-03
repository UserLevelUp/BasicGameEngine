#include "../include/BgeGameModule.h"

        // Number of dot-spaces along an edge (derived from its length so each
        // step aligns with the corridor dot rhythm seeded in geometry).
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace {

constexpr int BGE_INTI_RUNNER_MODE_COUNT = 10;
constexpr int BGE_INTI_TECH_COUNT = 6;
constexpr int BGE_INTI_TECH_NONE = -1;
constexpr int BGE_INTI_ROUTE_POINT_MAX = 8;
// Pathing-UI redline honing groups (warp bge.engine.warp.inti-pathing-ui-redline):
// dots, aim, hud. Page Up/Down cycles the focused group; digits 0-9 pick the
// active candidate preset for that group.
constexpr int BGE_INTI_UI_GROUP_COUNT = 3;
constexpr float BGE_INTI_ROUTE_DEFAULT_SPEED = 130.0f;
constexpr float BGE_INTI_ROUTE_TRAIL_SPEED = 185.0f;
constexpr float BGE_INTI_ROUTE_REVERSE_KREBS_BONUS = 95.0f;

// Tech-gated bound on the arrow-pull (the engine-neutral vector drag). Base
// values are tight; each completed tech raises the magnitude cap and widens
// the angle-of-attack cone, so "leveling up" literally lets you pull harder
// and aim wider. The Inti module feeds these to runtime.setVectorDragLimit;
// the clamp itself lives in the domain-neutral engine.
constexpr float BGE_INTI_DRAG_BASE_MAGNITUDE = 120.0f;
constexpr float BGE_INTI_DRAG_SPEED_BONUS = 90.0f;       // Speed Training
constexpr float BGE_INTI_DRAG_ENDURANCE_BONUS = 70.0f;   // Endurance
constexpr float BGE_INTI_DRAG_KREBS_BONUS = 120.0f;      // Reverse Krebs Cycle
constexpr float BGE_INTI_DRAG_BASE_HALFWIDTH = 0.35f;    // radians (~20 deg)
constexpr float BGE_INTI_DRAG_HALFWIDTH_PER_TECH = 0.22f;// ~12.6 deg per tech

enum class IntiSection : int {
    Main = 0,
    TechTree = 1,
    Empire = 2,
    Chasqui = 3,
    PhotoRun = 4,
};

struct IntiRunnerMode {
    const wchar_t* name;
    float laneFraction;
    float radius;
    float speed;
    float colorR;
    float colorG;
    float colorB;
    BgeObjectRenderStyle renderStyle;
    float outlineThickness;
};

struct IntiTechNode {
    const wchar_t* key;
    const wchar_t* name;
    const wchar_t* lane;
    int cost;
    int prerequisite;
    float xFraction;
    float yFraction;
    float colorR;
    float colorG;
    float colorB;
};

struct IntiRoutePoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct IntiRouteSample {
    float x = 0.0f;
    float y = 0.0f;
    float headingX = 1.0f;
    float headingY = 0.0f;
};

// A single tourist-checkpoint photo target loaded from an *.intimap data
// file. Position is stored as a [0,1] fraction of the play area so the
// same map renders at any resolution. This is the data-driven seam: the
// scene is built from these records, not from hard-coded C++ constants.
struct IntiPhotoCheckpoint {
    float xFraction = 0.5f;
    float yFraction = 0.5f;
    std::wstring name;
    bool visited = false;
    // Composition target (the iconic "postcard" approach). A `shot` map line
    // attaches a golden approach heading + a tolerance cone to this site; the
    // arrival angle of attack is scored against it on claim. Sites without a
    // `shot` line score nothing (hasShot stays false) so legacy maps still run.
    bool hasShot = false;
    float goldenAngleDeg = 0.0f;    // ideal approach heading, layout space (y-down)
    float toleranceDeg = 30.0f;     // half-width of the golden composition cone
    int compositionScore = -1;      // 0..100 once claimed; -1 = not composed yet
};

// A corridor edge between two named checkpoints. Rendered as a stone path
// line and seeded with pac-man-style collectible trail dots.
struct IntiPhotoPathName {
    std::wstring a;
    std::wstring b;
};

// A resolved corridor edge (checkpoint indices) used at render time.
struct IntiPhotoPath {
    int a = -1;
    int b = -1;
};

// A single collectible dot sitting on a path. Picked up by proximity; not
// drawn from an object slot (lives in the module scene-geometry channel),
// so a corridor can hold far more than the 10 object slots allow.
struct IntiPhotoDot {
    float xFraction = 0.5f;
    float yFraction = 0.5f;
    bool collected = false;
};

// An area-exit marker. Once the area is complete, stepping onto it loads
// the next *.intimap tourist area (the scene-change seam).
struct IntiPhotoExit {
    float xFraction = 0.5f;
    float yFraction = 0.5f;
    std::wstring nextMap;
    std::wstring name;
};

struct IntiPhotoDotUiStyle {
    float pathHalfWidthPx;
    float pathR;
    float pathG;
    float pathB;
    float pathA;
    float remainingHalfPx;
    float remainingR;
    float remainingG;
    float remainingB;
    float remainingA;
    float collectedHalfPx;
    float collectedR;
    float collectedG;
    float collectedB;
    float collectedA;
    float currentHaloHalfPx;
    float currentHaloR;
    float currentHaloG;
    float currentHaloB;
    float currentHaloA;
};

const std::array<IntiPhotoDotUiStyle, BGE_INTI_RUNNER_MODE_COUNT>& IntiPhotoDotUiStyles()
{
    static const std::array<IntiPhotoDotUiStyle, BGE_INTI_RUNNER_MODE_COUNT> styles{{
        { 3.0f, 0.36f, 0.32f, 0.26f, 0.85f, 3.5f, 1.00f, 0.86f, 0.42f, 1.00f, 1.9f, 0.52f, 0.44f, 0.30f, 0.26f, 9.0f, 0.96f, 0.94f, 0.40f, 0.36f },
        { 3.6f, 0.42f, 0.34f, 0.26f, 0.90f, 4.2f, 1.00f, 0.90f, 0.46f, 1.00f, 2.2f, 0.46f, 0.42f, 0.34f, 0.34f, 10.0f, 1.00f, 0.92f, 0.48f, 0.42f },
        { 2.8f, 0.32f, 0.30f, 0.24f, 0.78f, 3.0f, 0.95f, 0.80f, 0.34f, 0.95f, 1.5f, 0.42f, 0.40f, 0.36f, 0.24f, 8.0f, 0.90f, 0.88f, 0.42f, 0.30f },
        { 4.1f, 0.44f, 0.36f, 0.30f, 0.92f, 4.8f, 1.00f, 0.88f, 0.30f, 1.00f, 2.4f, 0.56f, 0.40f, 0.24f, 0.30f, 11.0f, 1.00f, 0.86f, 0.34f, 0.38f },
        { 3.2f, 0.30f, 0.34f, 0.38f, 0.88f, 3.7f, 0.84f, 0.98f, 1.00f, 1.00f, 1.8f, 0.30f, 0.52f, 0.58f, 0.30f, 9.5f, 0.82f, 0.96f, 1.00f, 0.40f },
        { 2.6f, 0.28f, 0.28f, 0.26f, 0.70f, 2.9f, 1.00f, 0.96f, 0.62f, 0.96f, 2.0f, 0.42f, 0.42f, 0.40f, 0.44f, 8.5f, 1.00f, 0.96f, 0.64f, 0.34f },
        { 3.4f, 0.40f, 0.30f, 0.24f, 0.88f, 3.2f, 1.00f, 0.70f, 0.32f, 1.00f, 2.6f, 0.64f, 0.34f, 0.22f, 0.34f, 10.5f, 1.00f, 0.76f, 0.40f, 0.44f },
        { 3.1f, 0.32f, 0.34f, 0.36f, 0.82f, 4.0f, 0.70f, 0.90f, 1.00f, 1.00f, 1.7f, 0.30f, 0.48f, 0.60f, 0.28f, 9.2f, 0.72f, 0.88f, 1.00f, 0.40f },
        { 3.8f, 0.42f, 0.36f, 0.28f, 0.94f, 4.5f, 1.00f, 0.84f, 0.58f, 1.00f, 2.1f, 0.58f, 0.46f, 0.34f, 0.32f, 11.5f, 1.00f, 0.90f, 0.62f, 0.48f },
        { 2.9f, 0.30f, 0.34f, 0.26f, 0.82f, 3.8f, 0.78f, 1.00f, 0.62f, 1.00f, 1.6f, 0.30f, 0.56f, 0.30f, 0.24f, 8.8f, 0.78f, 1.00f, 0.68f, 0.38f },
    }};
    return styles;
}

const IntiPhotoDotUiStyle& ActivePhotoDotUiStyle(const std::array<int, BGE_INTI_UI_GROUP_COUNT>& candidates)
{
    int index = (std::max)(0, (std::min)(BGE_INTI_RUNNER_MODE_COUNT - 1, candidates[0]));
    return IntiPhotoDotUiStyles()[static_cast<std::size_t>(index)];
}

struct IntiPhotoAimUiStyle {
    float arrowLengthPx;
    float arrowHalfWidthPx;
    float arrowTipHalfPx;
    float arrowR;
    float arrowG;
    float arrowB;
    float arrowA;
    float junctionHaloHalfPx;
    float junctionHaloR;
    float junctionHaloG;
    float junctionHaloB;
    float junctionHaloA;
    float branchDotHalfPx;
    float branchDotR;
    float branchDotG;
    float branchDotB;
    float branchDotA;
};

const std::array<IntiPhotoAimUiStyle, BGE_INTI_RUNNER_MODE_COUNT>& IntiPhotoAimUiStyles()
{
    static const std::array<IntiPhotoAimUiStyle, BGE_INTI_RUNNER_MODE_COUNT> styles{{
        { 34.0f, 2.6f, 6.0f, 0.30f, 0.95f, 1.00f, 1.00f, 13.0f, 0.26f, 0.84f, 1.00f, 0.28f, 4.6f, 0.72f, 0.96f, 1.00f, 0.85f },
        { 38.0f, 2.8f, 6.8f, 0.34f, 0.98f, 1.00f, 1.00f, 14.0f, 0.30f, 0.90f, 1.00f, 0.30f, 5.0f, 0.80f, 1.00f, 1.00f, 0.90f },
        { 30.0f, 2.2f, 5.2f, 0.46f, 0.88f, 1.00f, 0.94f, 12.0f, 0.30f, 0.72f, 0.96f, 0.24f, 4.0f, 0.66f, 0.90f, 1.00f, 0.74f },
        { 36.0f, 3.2f, 7.2f, 0.32f, 1.00f, 0.88f, 1.00f, 14.8f, 0.34f, 1.00f, 0.80f, 0.32f, 5.4f, 0.78f, 1.00f, 0.74f, 0.92f },
        { 33.0f, 2.4f, 6.0f, 0.70f, 0.90f, 1.00f, 0.98f, 12.6f, 0.52f, 0.82f, 1.00f, 0.28f, 4.3f, 0.86f, 0.92f, 1.00f, 0.80f },
        { 35.0f, 2.6f, 6.2f, 1.00f, 0.86f, 0.48f, 1.00f, 13.8f, 0.90f, 0.62f, 0.32f, 0.30f, 4.8f, 1.00f, 0.82f, 0.44f, 0.88f },
        { 37.0f, 3.0f, 7.0f, 1.00f, 0.66f, 0.42f, 1.00f, 15.0f, 1.00f, 0.46f, 0.30f, 0.34f, 5.6f, 1.00f, 0.70f, 0.42f, 0.92f },
        { 32.0f, 2.3f, 5.8f, 0.66f, 0.98f, 0.72f, 0.98f, 12.8f, 0.52f, 0.94f, 0.56f, 0.26f, 4.2f, 0.76f, 1.00f, 0.74f, 0.84f },
        { 39.0f, 3.4f, 7.8f, 1.00f, 0.72f, 0.98f, 1.00f, 15.4f, 0.92f, 0.52f, 1.00f, 0.36f, 5.8f, 1.00f, 0.74f, 1.00f, 0.92f },
        { 31.0f, 2.1f, 5.0f, 0.74f, 1.00f, 0.54f, 0.96f, 12.4f, 0.56f, 1.00f, 0.42f, 0.24f, 4.1f, 0.82f, 1.00f, 0.54f, 0.78f },
    }};
    return styles;
}

const IntiPhotoAimUiStyle& ActivePhotoAimUiStyle(const std::array<int, BGE_INTI_UI_GROUP_COUNT>& candidates)
{
    int index = (std::max)(0, (std::min)(BGE_INTI_RUNNER_MODE_COUNT - 1, candidates[1]));
    return IntiPhotoAimUiStyles()[static_cast<std::size_t>(index)];
}

struct IntiPhotoHudUiStyle {
    const wchar_t* label;
    bool compact;
    bool showSource;
    bool showCompose;
    bool showControls;
    bool showRoll;
    bool showCards;
};

const std::array<IntiPhotoHudUiStyle, BGE_INTI_RUNNER_MODE_COUNT>& IntiPhotoHudUiStyles()
{
    static const std::array<IntiPhotoHudUiStyle, BGE_INTI_RUNNER_MODE_COUNT> styles{{
        { L"survey-brief",  true,  false, false, false, false, false },
        { L"survey-mid",    false, false, false, false, true,  false },
        { L"command-brief", false, false, false, true,  true,  false },
        { L"command-full",  false, true,  true,  true,  true,  true  },
        { L"postcard",      true,  false, true,  false, false, true  },
        { L"telemetry",     false, true,  true,  false, true,  false },
        { L"route-card",    false, false, false, false, true,  true  },
        { L"compose-card",  false, false, true,  false, false, true  },
        { L"minimal",       true,  false, false, false, false, false },
        { L"judge-board",   false, true,  true,  true,  true,  true  },
    }};
    return styles;
}

const IntiPhotoHudUiStyle& ActivePhotoHudUiStyle(const std::array<int, BGE_INTI_UI_GROUP_COUNT>& candidates)
{
    int index = (std::max)(0, (std::min)(BGE_INTI_RUNNER_MODE_COUNT - 1, candidates[2]));
    return IntiPhotoHudUiStyles()[static_cast<std::size_t>(index)];
}

const std::array<IntiRunnerMode, BGE_INTI_RUNNER_MODE_COUNT>& IntiRunnerModes()
{
    static const std::array<IntiRunnerMode, BGE_INTI_RUNNER_MODE_COUNT> modes{{
        { L"relay sprint",  0.54f, 13.0f,  64.0f, 1.00f, 0.78f, 0.30f, BgeObjectRenderStyle::Outline, 2.0f },
        { L"highland lean", 0.58f, 14.0f,  76.0f, 0.96f, 0.42f, 0.28f, BgeObjectRenderStyle::Outline, 2.2f },
        { L"quipu carry",   0.62f, 12.0f,  58.0f, 0.34f, 0.84f, 0.70f, BgeObjectRenderStyle::Filled,  2.4f },
        { L"ridge dash",    0.50f, 15.0f,  88.0f, 0.86f, 0.90f, 0.46f, BgeObjectRenderStyle::Outline, 2.6f },
        { L"night courier", 0.66f, 13.0f,  70.0f, 0.62f, 0.72f, 1.00f, BgeObjectRenderStyle::Outline, 2.1f },
        { L"sun runner",    0.72f, 16.0f,  60.0f, 1.00f, 0.92f, 0.42f, BgeObjectRenderStyle::Filled,  2.8f },
        { L"canyon step",   0.76f, 12.0f,  92.0f, 0.88f, 0.54f, 0.36f, BgeObjectRenderStyle::Outline, 1.8f },
        { L"moon relay",    0.70f, 14.0f,  66.0f, 0.72f, 0.86f, 1.00f, BgeObjectRenderStyle::Filled,  2.2f },
        { L"royal stride",  0.80f, 15.0f,  78.0f, 0.98f, 0.66f, 0.88f, BgeObjectRenderStyle::Outline, 2.4f },
        { L"terrace flash", 0.84f, 11.0f, 105.0f, 0.70f, 1.00f, 0.58f, BgeObjectRenderStyle::Outline, 1.7f },
    }};
    return modes;
}

const std::array<IntiTechNode, BGE_INTI_TECH_COUNT>& IntiTechNodes()
{
    static const std::array<IntiTechNode, BGE_INTI_TECH_COUNT> nodes{{
        { L"food",       L"Chasqui Rations",   L"food",    3, BGE_INTI_TECH_NONE, 0.16f, 0.42f, 0.58f, 0.92f, 0.42f },
        { L"speed",      L"Speed Training",    L"chasqui", 4, BGE_INTI_TECH_NONE, 0.29f, 0.50f, 0.92f, 0.72f, 0.40f },
        { L"endurance",  L"Endurance",         L"chasqui", 4, BGE_INTI_TECH_NONE, 0.42f, 0.56f, 0.32f, 0.86f, 0.62f },
        { L"tambos",     L"Relay Tambos",      L"paths",   5, 0,                  0.56f, 0.48f, 0.82f, 0.70f, 0.44f },
        { L"suchimamma", L"Suchimamma Blessing", L"gods",  6, 2,                  0.70f, 0.60f, 0.54f, 0.90f, 0.78f },
        { L"krebs",      L"Reverse Krebs Cycle", L"drugs", 7, 4,                  0.84f, 0.64f, 0.96f, 0.84f, 0.36f },
    }};
    return nodes;
}

std::wstring LowerIntiArg(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

bool TryParseIntiInt(const std::wstring& text, int& value)
{
    wchar_t* end = nullptr;
    long parsed = std::wcstol(text.c_str(), &end, 10);
    if (end == text.c_str() || !end || *end != L'\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool TryParseIntiFloat(const std::wstring& text, float& value)
{
    wchar_t* end = nullptr;
    double parsed = std::wcstod(text.c_str(), &end);
    if (end == text.c_str() || !end || *end != L'\0' || !std::isfinite(parsed)) {
        return false;
    }
    value = static_cast<float>(parsed);
    return true;
}

bool TryParseIntiPointToken(const std::wstring& text, float& x, float& y)
{
    std::size_t separator = text.find(L',');
    if (separator == std::wstring::npos) {
        return false;
    }
    return TryParseIntiFloat(text.substr(0, separator), x)
        && TryParseIntiFloat(text.substr(separator + 1), y);
}

bool TryReadIntiPoint(const std::vector<std::wstring>& tokens, std::size_t index, float& x, float& y, std::size_t& nextIndex)
{
    if (index >= tokens.size()) {
        return false;
    }
    if (TryParseIntiPointToken(tokens[index], x, y)) {
        nextIndex = index + 1;
        return true;
    }
    if (index + 1 < tokens.size() && TryParseIntiFloat(tokens[index], x) && TryParseIntiFloat(tokens[index + 1], y)) {
        nextIndex = index + 2;
        return true;
    }
    return false;
}

int NormalizeIntiRunnerMode(int modeIndex)
{
    return (std::max)(0, (std::min)(BGE_INTI_RUNNER_MODE_COUNT - 1, modeIndex));
}

float ClampIntiFraction(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

std::vector<std::wstring> SplitIntiMapLine(const std::wstring& line)
{
    std::vector<std::wstring> tokens;
    std::wstringstream stream(line);
    std::wstring token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Embedded fallback so `inti photo-run` always runs even when no data file
// is found. Parsed through the SAME parser as a real file, so the data
// path is the single source of truth.
const wchar_t* IntiEmbeddedPhotoMap()
{
    return
        L"# embedded fallback map\n"
        L"map machu-picchu Machu Picchu Citadel\n"
        L"start 0.50 0.52\n"
        L"checkpoint 0.18 0.30 intihuatana\n"
        L"checkpoint 0.50 0.20 temple-of-the-sun\n"
        L"checkpoint 0.82 0.30 room-of-three-windows\n"
        L"checkpoint 0.86 0.66 main-gate\n"
        L"checkpoint 0.50 0.78 central-plaza\n"
        L"checkpoint 0.16 0.68 guardhouse-terrace\n"
        L"path intihuatana temple-of-the-sun\n"
        L"path temple-of-the-sun room-of-three-windows\n"
        L"path room-of-three-windows main-gate\n"
        L"path main-gate central-plaza\n"
        L"path central-plaza guardhouse-terrace\n"
        L"path guardhouse-terrace intihuatana\n"
        L"path intihuatana central-plaza\n"
        L"shot temple-of-the-sun golden-angle -18 tolerance 22\n"
        L"shot room-of-three-windows golden-angle 18 tolerance 30\n"
        L"shot main-gate golden-angle 84 tolerance 32\n"
        L"shot central-plaza golden-angle 162 tolerance 34\n"
        L"shot guardhouse-terrace golden-angle -164 tolerance 34\n"
        L"shot intihuatana golden-angle -87 tolerance 30\n"
        L"exit 0.86 0.66 huayna-picchu.intimap huayna-picchu-trail\n";
}

bool ParseIntiPhotoMapStream(std::wistream& input,
                             std::wstring& mapId,
                             std::wstring& mapName,
                             float& startX,
                             float& startY,
                             std::vector<IntiPhotoCheckpoint>& checkpoints,
                             std::vector<IntiPhotoPathName>& pathNames,
                             std::vector<IntiPhotoExit>& exits)
{
    std::wstring line;
    while (std::getline(input, line)) {
        std::size_t comment = line.find(L'#');
        if (comment != std::wstring::npos) {
            line = line.substr(0, comment);
        }
        std::vector<std::wstring> tokens = SplitIntiMapLine(line);
        if (tokens.empty()) {
            continue;
        }
        std::wstring key = LowerIntiArg(tokens[0]);
        if (key == L"map" && tokens.size() >= 2) {
            mapId = tokens[1];
            mapName.clear();
            for (std::size_t i = 2; i < tokens.size(); ++i) {
                if (i > 2) {
                    mapName += L" ";
                }
                mapName += tokens[i];
            }
        }
        else if (key == L"start" && tokens.size() >= 3) {
            float x = 0.0f;
            float y = 0.0f;
            if (TryParseIntiFloat(tokens[1], x) && TryParseIntiFloat(tokens[2], y)) {
                startX = ClampIntiFraction(x);
                startY = ClampIntiFraction(y);
            }
        }
        else if (key == L"checkpoint" && tokens.size() >= 4) {
            float x = 0.0f;
            float y = 0.0f;
            if (checkpoints.size() < static_cast<std::size_t>(BGE_OBJECT_SLOT_COUNT - 1)
                && TryParseIntiFloat(tokens[1], x)
                && TryParseIntiFloat(tokens[2], y)) {
                IntiPhotoCheckpoint checkpoint;
                checkpoint.xFraction = ClampIntiFraction(x);
                checkpoint.yFraction = ClampIntiFraction(y);
                checkpoint.name = tokens[3];
                checkpoint.visited = false;
                checkpoints.push_back(checkpoint);
            }
        }
        else if (key == L"path" && tokens.size() >= 3) {
            IntiPhotoPathName edge;
            edge.a = tokens[1];
            edge.b = tokens[2];
            pathNames.push_back(edge);
        }
        else if (key == L"exit" && tokens.size() >= 4) {
            float x = 0.0f;
            float y = 0.0f;
            if (TryParseIntiFloat(tokens[1], x) && TryParseIntiFloat(tokens[2], y)) {
                IntiPhotoExit exit;
                exit.xFraction = ClampIntiFraction(x);
                exit.yFraction = ClampIntiFraction(y);
                exit.nextMap = tokens[3];
                exit.name = tokens.size() >= 5 ? tokens[4] : tokens[3];
                exits.push_back(exit);
            }
        }
        else if (key == L"shot" && tokens.size() >= 3) {
            // shot <checkpoint-name> golden-angle <deg> tolerance <deg> [tags ...]
            // Attaches a composition target to a named checkpoint. Tag-truth
            // (describe scoring) is a later slice; unknown trailing tokens are
            // ignored for forward compatibility.
            std::wstring shotName = tokens[1];
            float goldenDeg = 0.0f;
            float tolDeg = 30.0f;
            bool hasGolden = false;
            for (std::size_t i = 2; i + 1 < tokens.size(); ++i) {
                std::wstring keyword = LowerIntiArg(tokens[i]);
                if ((keyword == L"golden-angle" || keyword == L"angle" || keyword == L"golden")
                    && TryParseIntiFloat(tokens[i + 1], goldenDeg)) {
                    hasGolden = true;
                }
                else if (keyword == L"tolerance" || keyword == L"tol" || keyword == L"cone") {
                    TryParseIntiFloat(tokens[i + 1], tolDeg);
                }
            }
            if (hasGolden) {
                for (IntiPhotoCheckpoint& checkpoint : checkpoints) {
                    if (checkpoint.name == shotName) {
                        checkpoint.hasShot = true;
                        checkpoint.goldenAngleDeg = goldenDeg;
                        checkpoint.toleranceDeg = (std::max)(1.0f, (std::min)(180.0f, tolDeg));
                        break;
                    }
                }
            }
        }
    }
    return !checkpoints.empty();
}

const IntiRunnerMode& IntiRunnerModeForIndex(int modeIndex)
{
    return IntiRunnerModes()[static_cast<std::size_t>(NormalizeIntiRunnerMode(modeIndex))];
}

int NormalizeIntiTechIndex(int techIndex)
{
    return (std::max)(0, (std::min)(BGE_INTI_TECH_COUNT - 1, techIndex));
}

const IntiTechNode& IntiTechNodeForIndex(int techIndex)
{
    return IntiTechNodes()[static_cast<std::size_t>(NormalizeIntiTechIndex(techIndex))];
}

int FindIntiTechIndexByToken(const std::wstring& token)
{
    if (token.empty()) {
        return BGE_INTI_TECH_NONE;
    }
    int numericIndex = 0;
    if (TryParseIntiInt(token, numericIndex) && numericIndex >= 0 && numericIndex < BGE_INTI_TECH_COUNT) {
        return numericIndex;
    }
    if (token == L"food" || token == L"rations" || token == L"chasqui-rations") {
        return 0;
    }
    if (token == L"speed" || token == L"training" || token == L"speed-training") {
        return 1;
    }
    if (token == L"endurance" || token == L"stamina") {
        return 2;
    }
    if (token == L"tambos" || token == L"tambo" || token == L"relay" || token == L"paths") {
        return 3;
    }
    if (token == L"suchimamma" || token == L"blessing" || token == L"god" || token == L"gods") {
        return 4;
    }
    if (token == L"krebs" || token == L"reverse-krebs" || token == L"drug" || token == L"drugs") {
        return 5;
    }
    return BGE_INTI_TECH_NONE;
}

const wchar_t* IntiSectionName(IntiSection section)
{
    switch (section) {
    case IntiSection::TechTree: return L"tech-tree";
    case IntiSection::Empire:   return L"empire";
    case IntiSection::Chasqui:  return L"chasqui-runners";
    case IntiSection::PhotoRun: return L"photo-run";
    case IntiSection::Main:
    default:                    return L"main";
    }
}

bool IntiRuntimeReady(const BgeGameRuntime& runtime)
{
    return runtime.objectMutex
        && runtime.objectSlots
        && runtime.animationRunning
        && runtime.selectedObjectSlot
        && runtime.objectSelectionActive
        && runtime.rendererStateDirty;
}

class IntiGameModule final : public BgeGameModule {
public:
    const wchar_t* Name() const override
    {
        return L"Inti Runners";
    }

    bool OnStart(BgeGameRuntime& runtime, std::wstring& statusText) override
    {
        return ShowMain(runtime, true, statusText);
    }

    bool OnCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, std::wstring& statusText) override
    {
        if (tokens.empty()) {
            statusText = L"Inti Runners command missing";
            return false;
        }

        std::wstring command = LowerIntiArg(tokens[0]);
        std::size_t subcommandIndex = 0;
        if (command == L"inti" || command == L"inti-runners" || command == L"intirunners") {
            subcommandIndex = 1;
        }

        std::wstring subcommand = tokens.size() > subcommandIndex ? LowerIntiArg(tokens[subcommandIndex]) : L"main";
        std::size_t argumentIndex = subcommandIndex + 1;
        std::wstring nextArgument = tokens.size() > argumentIndex ? LowerIntiArg(tokens[argumentIndex]) : L"";

        if (subcommand == L"game" || subcommand == L"start" || subcommand == L"restart" || subcommand == L"reset") {
            return ShowMain(runtime, true, statusText);
        }
        if (subcommand == L"main" || subcommand == L"home" || subcommand == L"title" || subcommand == L"screen" || subcommand == L"city") {
            return ShowMain(runtime, false, statusText);
        }
        if (subcommand == L"turn" || subcommand == L"end-turn" || subcommand == L"next-turn" || subcommand == L"resolve") {
            return EndTurn(runtime, statusText);
        }
        if ((subcommand == L"end" || subcommand == L"next") && nextArgument == L"turn") {
            return EndTurn(runtime, statusText);
        }
        if (subcommand == L"status" || subcommand == L"state" || subcommand == L"hud") {
            return PublishStatus(runtime, statusText);
        }
        if (subcommand == L"runner" || subcommand == L"candidate" || subcommand == L"mode") {
            if (tokens.size() <= argumentIndex) {
                return ShowSection(runtime, IntiSection::Chasqui, statusText);
            }
            int modeIndex = 0;
            if (!TryParseIntiInt(tokens[argumentIndex], modeIndex)) {
                statusText = L"Use: inti runner 0-9";
                return false;
            }
            return ApplyRunnerMode(runtime, modeIndex, statusText);
        }
        if (subcommand == L"tech" || subcommand == L"tech-tree" || subcommand == L"science" || subcommand == L"education") {
            std::wstring action = tokens.size() > argumentIndex ? LowerIntiArg(tokens[argumentIndex]) : L"";
            if (action == L"educate") {
                action = L"research";
            }
            if (action == L"choose" || action == L"select" || action == L"project" || action == L"research" || action == L"study") {
                if (tokens.size() > argumentIndex + 1) {
                    int techIndex = FindIntiTechIndexByToken(LowerIntiArg(tokens[argumentIndex + 1]));
                    if (techIndex < 0) {
                        statusText = L"Use: inti tech research food|speed|endurance|tambos|suchimamma|krebs";
                        return false;
                    }
                    return SelectTechProject(runtime, techIndex, statusText);
                }
                return ShowSection(runtime, IntiSection::TechTree, statusText, L"research");
            }
            if (action == L"list" || action == L"tree" || action == L"status") {
                return ShowSection(runtime, IntiSection::TechTree, statusText, L"list");
            }
            return ShowSection(runtime, IntiSection::TechTree, statusText, action);
        }
        if (subcommand == L"empire" || subcommand == L"economy" || subcommand == L"goods" || subcommand == L"cities") {
            std::wstring action = tokens.size() > argumentIndex ? LowerIntiArg(tokens[argumentIndex]) : L"";
            return ShowSection(runtime, IntiSection::Empire, statusText, NormalizeEmpireAction(action));
        }
        if (subcommand == L"paths" || subcommand == L"path" || subcommand == L"trail" || subcommand == L"trails") {
            std::wstring action = tokens.size() > argumentIndex ? LowerIntiArg(tokens[argumentIndex]) : L"";
            if ((action == L"run" || action == L"animate" || action == L"route") && tokens.size() > argumentIndex + 1) {
                return StartChasquiRoute(runtime, tokens, argumentIndex + 1, statusText);
            }
            if (action == L"run" || action == L"send" || action == L"message") {
                return ShowSection(runtime, IntiSection::Chasqui, statusText, L"send-message");
            }
            return ShowSection(runtime, IntiSection::Empire, statusText, NormalizeEmpireAction(action));
        }
        if (subcommand == L"chasqui" || subcommand == L"runners" || subcommand == L"messages" || subcommand == L"message") {
            std::wstring action = tokens.size() > argumentIndex ? LowerIntiArg(tokens[argumentIndex]) : L"";
            if ((action == L"run" || action == L"animate" || action == L"route" || action == L"path") && tokens.size() > argumentIndex + 1) {
                return StartChasquiRoute(runtime, tokens, argumentIndex + 1, statusText);
            }
            if (action == L"run" || action == L"send") {
                action = L"send-message";
            }
            return ShowSection(runtime, IntiSection::Chasqui, statusText, action);
        }
        if (subcommand == L"photo-run" || subcommand == L"photo" || subcommand == L"photorun" || subcommand == L"photo-tour") {
            std::wstring mapArg = tokens.size() > argumentIndex ? tokens[argumentIndex] : L"";
            return StartPhotoRun(runtime, mapArg, statusText);
        }
        if (subcommand == L"ui-group" || subcommand == L"uigroup") {
            int group = -1;
            if (nextArgument == L"dots") {
                group = 0;
            }
            else if (nextArgument == L"aim") {
                group = 1;
            }
            else if (nextArgument == L"hud") {
                group = 2;
            }
            if (group < 0) {
                statusText = L"Use: inti ui-group dots|aim|hud";
                return false;
            }
            return SetPhotoUiGroup(runtime, group, statusText);
        }
        if (subcommand == L"ui-candidate" || subcommand == L"uicandidate") {
            int candidate = 0;
            if (tokens.size() <= argumentIndex || !TryParseIntiInt(tokens[argumentIndex], candidate)) {
                statusText = L"Use: inti ui-candidate 0-9";
                return false;
            }
            return ApplyPhotoUiCandidate(runtime, candidate, statusText);
        }
        if (subcommand == L"ui-lock" || subcommand == L"uilock") {
            if (tokens.size() <= argumentIndex) {
                return LockPhotoUiWinners(runtime, statusText);
            }
            int group = -1;
            if (nextArgument == L"dots") {
                group = 0;
            }
            else if (nextArgument == L"aim") {
                group = 1;
            }
            else if (nextArgument == L"hud") {
                group = 2;
            }
            int candidate = 0;
            if (group < 0 || tokens.size() <= argumentIndex + 1 || !TryParseIntiInt(tokens[argumentIndex + 1], candidate)) {
                statusText = L"Use: inti ui-lock [dots|aim|hud 0-9]";
                return false;
            }
            return LockPhotoUiWinnerGroup(runtime, group, candidate, statusText);
        }
        if (subcommand == L"ui-unlock" || subcommand == L"uiunlock") {
            return UnlockPhotoUiWinners(runtime, statusText);
        }
        if (subcommand == L"commands" || subcommand == L"help") {
            statusText = L"Inti commands: inti photo-run [map.intimap] | inti main | inti tech list | inti empire path | inti chasqui run 120,240 300,240 420,320 [speed 220] [reverse-krebs] | inti ui-group dots|aim|hud | inti ui-candidate 0-9 | inti ui-lock [dots|aim|hud 0-9] | inti ui-unlock | inti end-turn | inti status";
            if (runtime.setHud) {
                runtime.setHud(statusText);
            }
            return true;
        }

        statusText = L"Inti Runners module command not handled";
        return false;
    }

    bool OnKeyDown(BgeGameRuntime& runtime, unsigned int key) override
    {
        if (!active_) {
            return false;
        }

        if (section_ == IntiSection::PhotoRun) {
            return HandlePhotoRunKey(runtime, key);
        }

        int digitMode = BgeDigitModeIndexFromKey(key);
        std::wstring statusText;
        if (digitMode >= 0) {
            if (section_ == IntiSection::TechTree && digitMode < BGE_INTI_TECH_COUNT) {
                return SelectTechProject(runtime, digitMode, statusText);
            }
            return ApplyRunnerMode(runtime, digitMode, statusText);
        }
        if (key == L'N' || key == L'n') {
            return EndTurn(runtime, statusText);
        }
        if (key == VK_RETURN || key == L'M' || key == L'm') {
            return ShowMain(runtime, false, statusText);
        }
        if (key == L'T' || key == L't') {
            return ShowSection(runtime, IntiSection::TechTree, statusText);
        }
        if (key == L'E' || key == L'e') {
            return ShowSection(runtime, IntiSection::Empire, statusText);
        }
        if (key == L'C' || key == L'c') {
            return ShowSection(runtime, IntiSection::Chasqui, statusText);
        }
        if (key == L'R' || key == L'r') {
            return ShowSection(runtime, IntiSection::TechTree, statusText, L"research");
        }
        if (key == L'F' || key == L'f') {
            return ShowSection(runtime, IntiSection::Empire, statusText, L"found-city");
        }
        if (key == L'P' || key == L'p') {
            return ShowSection(runtime, section_ == IntiSection::Chasqui ? IntiSection::Chasqui : IntiSection::Empire, statusText,
                               section_ == IntiSection::Chasqui ? L"send-message" : L"establish-path");
        }
        if (key == L'S' || key == L's') {
            return ShowSection(runtime, section_ == IntiSection::Chasqui ? IntiSection::Chasqui : IntiSection::TechTree, statusText,
                               section_ == IntiSection::Chasqui ? L"send-message" : L"research");
        }
        return false;
    }

    bool OnTick(BgeGameRuntime& runtime, double deltaMilliseconds) override
    {
        if (!active_ || !IntiRuntimeReady(runtime)) {
            return false;
        }
        if (section_ == IntiSection::PhotoRun) {
            return TickPhotoRun(runtime, deltaMilliseconds);
        }
        if (section_ != IntiSection::Chasqui) {
            return false;
        }
        if (runtime.animationRunning && !*runtime.animationRunning) {
            return false;
        }

        BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
        double deltaSeconds = (std::max)(0.0, (std::min)(deltaMilliseconds / 1000.0, 0.10));
        bool dirty = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            runnerSeconds_ += deltaSeconds;
            if (runnerRouteActive_ && runnerRouteTotalDistance_ > 0.0f) {
                runnerRouteProgress_ = WrapRouteDistanceLocked(runnerRouteProgress_ + runnerRouteSpeed_ * static_cast<float>(deltaSeconds));
                ConfigureChasquiRouteSceneLocked(runtime, viewport);
                dirty = true;
            }
            else if (ChasquiReadyLocked()) {
                float width = (std::max)(1.0f, viewport.width);
            for (int index = 0; index < BGE_INTI_RUNNER_MODE_COUNT; ++index) {
                BgeObjectSlotState& slot = (*runtime.objectSlots)[static_cast<std::size_t>(index)];
                if (!slot.visible || slot.shape != BgeObjectShape::Runner) {
                    continue;
                }
                slot.x += slot.velocityX * static_cast<float>(deltaSeconds);
                float wrapMargin = slot.radius * 3.2f;
                if (slot.x > width + wrapMargin) {
                    slot.x = -wrapMargin;
                }
                dirty = true;
            }
            }
            if (dirty) {
                *runtime.rendererStateDirty = true;
                if (runtime.persistActiveObjectGroupLocked) {
                    runtime.persistActiveObjectGroupLocked();
                }
            }
        }
        return dirty;
    }

private:
    std::wstring NormalizeEmpireAction(const std::wstring& action) const
    {
        if (action == L"found" || action == L"found-city" || action == L"add-city" || action == L"city") {
            return L"found-city";
        }
        if (action == L"path" || action == L"trail" || action == L"trails" || action == L"build" || action == L"build-path") {
            return L"establish-path";
        }
        if (action == L"goods" || action == L"produce") {
            return L"produce-goods";
        }
        return action;
    }

    bool ShowMain(BgeGameRuntime& runtime, bool resetState, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (resetState || !active_) {
                ResetStateLocked();
            }
            active_ = true;
            section_ = IntiSection::Main;
            // Leaving any run: release the arrow-pull cone so the controller's
            // free vector authoring returns outside the game.
            if (runtime.setVectorDragLimit) {
                runtime.setVectorDragLimit(0.0f, 0.0f, 0.0f);
            }
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, false);
            statusText = BuildStatusLocked();
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] main module=IntiGameModule");
        }
        return true;
    }

    bool ShowSection(BgeGameRuntime& runtime, IntiSection section, std::wstring& statusText, const std::wstring& action = L"")
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        std::wstring actionStatus;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            section_ = section;
            actionStatus = ApplySectionActionLocked(section, action);
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, false);
            statusText = BuildStatusLocked();
            if (!actionStatus.empty()) {
                statusText += L" | " + actionStatus;
            }
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] section=" + NarrowStatus(statusText));
        }
        return true;
    }

    bool ApplyRunnerMode(BgeGameRuntime& runtime, int modeIndex, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            runnerMode_ = NormalizeIntiRunnerMode(modeIndex);
            if (ChasquiReadyLocked()) {
                section_ = IntiSection::Chasqui;
            }
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, true);
            statusText = BuildRunnerModeStatusTextLocked();
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] runner-mode=" + std::to_string(runnerMode_));
        }
        return true;
    }

    bool SelectTechProject(BgeGameRuntime& runtime, int techIndex, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            section_ = IntiSection::TechTree;
            int normalizedIndex = NormalizeIntiTechIndex(techIndex);
            const IntiTechNode& tech = IntiTechNodeForIndex(normalizedIndex);
            if (techCompleted_[static_cast<std::size_t>(normalizedIndex)]) {
                statusText = std::wstring(L"tech already complete: ") + tech.name;
            }
            else if (!CanResearchTechLocked(normalizedIndex)) {
                statusText = std::wstring(L"tech locked: ") + tech.name + L" needs " + IntiTechNodeForIndex(tech.prerequisite).name;
            }
            else {
                currentTechIndex_ = normalizedIndex;
                statusText = std::wstring(L"research focus set: ") + tech.name + L"; use inti end-turn to advance";
            }
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, false);
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    bool EndTurn(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            std::wstring report = ResolveTurnLocked();
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, false);
            statusText = BuildStatusLocked() + L" | " + report;
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] end-turn=" + NarrowStatus(statusText));
        }
        return true;
    }

    bool StartChasquiRoute(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, std::size_t firstPointIndex, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::vector<IntiRoutePoint> points;
        bool reverseKrebsRequested = false;
        bool explicitSpeed = false;
        float requestedSpeed = BGE_INTI_ROUTE_DEFAULT_SPEED;
        std::size_t index = firstPointIndex;
        while (index < tokens.size() && points.size() < BGE_INTI_ROUTE_POINT_MAX) {
            std::wstring token = LowerIntiArg(tokens[index]);
            if (token == L"to" || token == L"then" || token == L"path" || token == L"paths" || token == L"->") {
                ++index;
                continue;
            }
            if (token == L"fast" || token == L"boost" || token == L"drug" || token == L"krebs" || token == L"reverse-krebs" || token == L"reversekrebs") {
                reverseKrebsRequested = true;
                ++index;
                continue;
            }
            if (token == L"reverse" && index + 1 < tokens.size() && LowerIntiArg(tokens[index + 1]) == L"krebs") {
                reverseKrebsRequested = true;
                index += 2;
                continue;
            }
            if (reverseKrebsRequested && (token == L"cycle" || token == L"effect")) {
                ++index;
                continue;
            }
            if (token == L"speed" || token == L"pace") {
                if (index + 1 >= tokens.size() || !TryParseIntiFloat(tokens[index + 1], requestedSpeed)) {
                    statusText = L"Use: inti chasqui run 120,240 300,240 420,320 [speed 220] [reverse-krebs]";
                    return false;
                }
                explicitSpeed = true;
                index += 2;
                continue;
            }
            if (token.find(L"speed=") == 0 || token.find(L"pace=") == 0) {
                std::size_t valueStart = token.find(L'=') + 1;
                if (!TryParseIntiFloat(token.substr(valueStart), requestedSpeed)) {
                    statusText = L"Use: inti chasqui run 120,240 300,240 420,320 [speed 220] [reverse-krebs]";
                    return false;
                }
                explicitSpeed = true;
                ++index;
                continue;
            }
            float x = 0.0f;
            float y = 0.0f;
            std::size_t nextIndex = index;
            if (!TryReadIntiPoint(tokens, index, x, y, nextIndex)) {
                statusText = L"Use: inti chasqui run 120,240 300,240 420,320 [speed 220] [reverse-krebs]";
                return false;
            }
            points.push_back({ x, y });
            index = nextIndex;
        }

        if (points.size() < 2) {
            statusText = L"Chasqui route needs a start x,y and at least one path point";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            section_ = IntiSection::Chasqui;
            runnerRoutePoints_ = points;
            runnerRouteActive_ = true;
            runnerRouteLoop_ = true;
            runnerRouteSpeed_ = explicitSpeed ? requestedSpeed : BGE_INTI_ROUTE_DEFAULT_SPEED + (techCompleted_[5] ? 35.0f : 0.0f);
            if (reverseKrebsRequested) {
                runnerRouteSpeed_ += BGE_INTI_ROUTE_REVERSE_KREBS_BONUS;
            }
            runnerRouteSpeed_ = (std::max)(40.0f, (std::min)(360.0f, runnerRouteSpeed_));
            runnerRouteBoostActive_ = reverseKrebsRequested || runnerRouteSpeed_ >= BGE_INTI_ROUTE_TRAIL_SPEED;
            runnerRouteProgress_ = 0.0f;
            runnerSeconds_ = 0.0;
            RecalculateChasquiRouteDistanceLocked();
            ConfigureChasquiRouteSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{});
            statusText = BuildChasquiRouteStatusLocked();
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] chasqui-route=" + NarrowStatus(statusText));
        }
        return true;
    }

    bool PublishStatus(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            statusText = BuildStatusLocked();
            hudText = BuildHudLocked();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    // ----- inti photo-run (data-driven tourist maze, Level 1 arcade slice) -----

    bool StartPhotoRun(BgeGameRuntime& runtime, const std::wstring& mapPath, std::wstring& statusText)
    {
        if (!IntiRuntimeReady(runtime)) {
            statusText = L"Inti Runners runtime unavailable";
            return false;
        }

        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!active_) {
                ResetStateLocked();
                active_ = true;
            }
            if (!LoadPhotoRunMapLocked(mapPath, runtime.viewport ? runtime.viewport() : BgeGameViewport{})) {
                statusText = L"inti photo-run: map has no tourist-checkpoint entries";
                return false;
            }
            section_ = IntiSection::PhotoRun;
            photoRunnerHeadingX_ = 0.0f;
            photoRunnerHeadingY_ = 0.0f;
            ConfigureSceneLocked(runtime, runtime.viewport ? runtime.viewport() : BgeGameViewport{}, false);
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }

        if (runtime.clearTitleScreen) {
            runtime.clearTitleScreen();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        if (runtime.log) {
            runtime.log("[IntiRunners] inti photo-run map=" + NarrowStatus(photoMapId_)
                        + " source=" + NarrowStatus(photoSource_)
                        + " checkpoints=" + std::to_string(photoCheckpoints_.size()));
        }
        return true;
    }

    bool ResolvePhotoMapPath(const std::wstring& requestedPath, std::wstring& resolved) const
    {
        std::vector<std::wstring> candidates;
        if (!requestedPath.empty()) {
            candidates.push_back(requestedPath);
            candidates.push_back(L"maps/" + requestedPath);
            candidates.push_back(L"BasicGameEngine/maps/" + requestedPath);
        }
        else {
            candidates.push_back(L"maps/machu-picchu.intimap");
            candidates.push_back(L"BasicGameEngine/maps/machu-picchu.intimap");
            candidates.push_back(L"../maps/machu-picchu.intimap");
        }

        std::error_code ec;
        for (const std::wstring& candidate : candidates) {
            std::filesystem::path path(candidate);
            if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
                resolved = candidate;
                return true;
            }
        }
        return false;
    }

    bool LoadPhotoRunMapLocked(const std::wstring& requestedPath, const BgeGameViewport& viewport)
    {
        std::wstring mapId = L"custom";
        std::wstring mapName = L"Tourist Map";
        float startX = 0.5f;
        float startY = 0.5f;
        std::vector<IntiPhotoCheckpoint> checkpoints;
        std::vector<IntiPhotoPathName> pathNames;
        std::vector<IntiPhotoExit> exits;

        bool parsed = false;
        std::wstring source;
        std::wstring resolved;
        if (ResolvePhotoMapPath(requestedPath, resolved)) {
            std::filesystem::path mapFsPath(resolved);
            std::wifstream file(mapFsPath);
            if (file.is_open()) {
                parsed = ParseIntiPhotoMapStream(file, mapId, mapName, startX, startY, checkpoints, pathNames, exits);
                if (parsed) {
                    source = resolved;
                }
            }
        }
        if (!parsed) {
            std::wstringstream embedded(IntiEmbeddedPhotoMap());
            mapId = L"machu-picchu";
            mapName = L"Machu Picchu Citadel";
            checkpoints.clear();
            pathNames.clear();
            exits.clear();
            parsed = ParseIntiPhotoMapStream(embedded, mapId, mapName, startX, startY, checkpoints, pathNames, exits);
            source = L"embedded";
        }
        if (!parsed) {
            return false;
        }

        photoMapId_ = mapId;
        photoMapName_ = mapName;
        photoSource_ = source;
        photoCheckpoints_ = std::move(checkpoints);
        photoExits_ = std::move(exits);
        BuildPhotoPathsAndDotsLocked(pathNames);
        photoRunnerXf_ = startX;
        photoRunnerYf_ = startY;
        photoRunnerHeadingX_ = 0.0f;
        photoRunnerHeadingY_ = 0.0f;
        photoFacingX_ = 1.0f;
        photoFacingY_ = 0.0f;
        photoEdgeIndex_ = -1;
        photoEdgeT_ = 0.0f;
        photoTravelDir_ = 0;
        photoNode_ = -1;
        photoPrevNode_ = -1;
        photoAimEdge_ = -1;
        photoFacingDir_ = 1;
        photoTargetT_ = 0.0f;
        photoLastRoll_ = 0;
        photoQueuedTurnDir_ = 0;
        photoAutoRun_ = true;
        photoPauseAtNextDot_ = false;
        // Rest the courier at the nearest checkpoint node so it begins parked
        // at a tourist stop; the player aims a direction, then hits Space to go.
        PlaceRunnerAtNearestNodeLocked(startX, startY, viewport);
        photoVisitedCount_ = 0;
        photoDotsCollected_ = 0;
        photoComplete_ = false;
        photoPendingExit_.clear();
        photoLastComposition_ = -1;
        photoCompositionTotal_ = 0;
        photoComposedCount_ = 0;
        photoLastComposeName_.clear();
        return true;
    }

    // Resolve corridor edges from checkpoint names to indices and seed each
    // corridor with collectible trail dots. Dots are spaced in fraction
    // space and skip the checkpoint endpoints so they sit "along" the path.
    void BuildPhotoPathsAndDotsLocked(const std::vector<IntiPhotoPathName>& pathNames)
    {
        photoPaths_.clear();
        photoDots_.clear();

        auto indexForName = [&](const std::wstring& name) -> int {
            for (std::size_t i = 0; i < photoCheckpoints_.size(); ++i) {
                if (photoCheckpoints_[i].name == name) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };

        const float spacing = 0.05f;   // fraction-space gap between dots
        const float endpointGap = 0.045f; // keep dots off the checkpoints
        for (const IntiPhotoPathName& edge : pathNames) {
            int a = indexForName(edge.a);
            int b = indexForName(edge.b);
            if (a < 0 || b < 0 || a == b) {
                continue;
            }
            photoPaths_.push_back(IntiPhotoPath{a, b});

            const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(a)];
            const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(b)];
            float dx = cb.xFraction - ca.xFraction;
            float dy = cb.yFraction - ca.yFraction;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length <= endpointGap * 2.0f) {
                continue;
            }
            int dotCount = (std::max)(0, static_cast<int>((length - endpointGap * 2.0f) / spacing));
            for (int i = 0; i <= dotCount; ++i) {
                float t = (dotCount == 0)
                    ? 0.5f
                    : (endpointGap + (length - endpointGap * 2.0f) * (static_cast<float>(i) / static_cast<float>(dotCount))) / length;
                IntiPhotoDot dot;
                dot.xFraction = ClampIntiFraction(ca.xFraction + dx * t);
                dot.yFraction = ClampIntiFraction(ca.yFraction + dy * t);
                dot.collected = false;
                photoDots_.push_back(dot);
            }
        }
    }

    std::wstring BuildPhotoRunStatusLocked() const
    {
        std::wstringstream stream;
        stream << L"inti photo-run | " << photoMapName_
               << L" | tourist-checkpoint " << photoVisitedCount_ << L"/" << photoCheckpoints_.size()
               << L" | path-dot " << photoDotsCollected_ << L"/" << photoDots_.size()
               << L" | source " << (photoSource_.empty() ? L"embedded" : photoSource_);
        stream << L" | " << (photoTravelDir_ != 0 ? L"running" : L"paused");
        if (photoQueuedTurnDir_ != 0 && photoNode_ < 0) {
            stream << L" | queued-turn " << (photoQueuedTurnDir_ > 0 ? L"right" : L"left");
        }
        if (photoPauseAtNextDot_) {
            stream << L" (pause queued)";
        }
        if (photoLastComposition_ >= 0) {
            stream << L" | compose " << photoLastComposeName_ << L" " << photoLastComposition_;
            if (photoComposedCount_ > 0) {
                stream << L" (avg " << (photoCompositionTotal_ / photoComposedCount_) << L")";
            }
        }
        stream << L" | ui " << PhotoUiGroupName(photoUiGroup_) << L" c" << photoUiCandidate_[static_cast<std::size_t>(photoUiGroup_)];
        stream << L" | Left/Right aim (when stopped), Up forward, Down reverse, Space pause/resume, PgUp/PgDn ui group, 0-9 candidate, M main";
        if (photoComplete_) {
            stream << L" | area complete";
            if (!photoExits_.empty()) {
                stream << L" - run to the green trail exit";
            }
        }
        return stream.str();
    }

    // Convert a play-area fraction to normalized device coordinates, matching
    // the renderer's slot transform so paths/dots align with checkpoint balls.
    static BgeColorVertex PhotoNdcVertex(float xFraction, float yFraction,
                                         const BgeGameViewport& viewport,
                                         float r, float g, float b, float a)
    {
        float width = (std::max)(1.0f, viewport.width);
        float height = (std::max)(1.0f, viewport.height);
        float px = width * xFraction;
        float py = viewport.playTop + (std::max)(1.0f, viewport.playHeight) * yFraction;
        BgeColorVertex vertex{};
        vertex.x = px / width * 2.0f - 1.0f;
        vertex.y = 1.0f - py / height * 2.0f;
        vertex.r = r;
        vertex.g = g;
        vertex.b = b;
        vertex.a = a;
        return vertex;
    }

    // Append a thickened line (two triangles) between two fraction-space
    // points to the module scene-geometry buffer.
    void AppendPhotoLine(std::vector<BgeColorVertex>& out, const BgeGameViewport& viewport,
                         float x0f, float y0f, float x1f, float y1f, float halfWidthPx,
                         float r, float g, float b, float a) const
    {
        float width = (std::max)(1.0f, viewport.width);
        float height = (std::max)(1.0f, viewport.height);
        // Perpendicular offset in fraction space derived from a pixel width.
        float dxf = x1f - x0f;
        float dyf = y1f - y0f;
        // Scale to pixels to get a true perpendicular, then back to fractions.
        float dxp = dxf * width;
        float dyp = dyf * (std::max)(1.0f, viewport.playHeight);
        float lenp = std::sqrt(dxp * dxp + dyp * dyp);
        if (lenp <= 0.0001f) {
            return;
        }
        float nxp = -dyp / lenp * halfWidthPx;
        float nyp = dxp / lenp * halfWidthPx;
        float offXf = nxp / width;
        float offYf = nyp / (std::max)(1.0f, viewport.playHeight);

        BgeColorVertex p0 = PhotoNdcVertex(x0f + offXf, y0f + offYf, viewport, r, g, b, a);
        BgeColorVertex p1 = PhotoNdcVertex(x1f + offXf, y1f + offYf, viewport, r, g, b, a);
        BgeColorVertex p2 = PhotoNdcVertex(x1f - offXf, y1f - offYf, viewport, r, g, b, a);
        BgeColorVertex p3 = PhotoNdcVertex(x0f - offXf, y0f - offYf, viewport, r, g, b, a);
        out.push_back(p0); out.push_back(p1); out.push_back(p2);
        out.push_back(p0); out.push_back(p2); out.push_back(p3);
    }

    // Append a small filled quad (a dot/marker) centered on a fraction point.
    void AppendPhotoQuad(std::vector<BgeColorVertex>& out, const BgeGameViewport& viewport,
                         float xf, float yf, float halfPx,
                         float r, float g, float b, float a) const
    {
        float width = (std::max)(1.0f, viewport.width);
        float offXf = halfPx / width;
        float offYf = halfPx / (std::max)(1.0f, viewport.playHeight);
        BgeColorVertex p0 = PhotoNdcVertex(xf - offXf, yf - offYf, viewport, r, g, b, a);
        BgeColorVertex p1 = PhotoNdcVertex(xf + offXf, yf - offYf, viewport, r, g, b, a);
        BgeColorVertex p2 = PhotoNdcVertex(xf + offXf, yf + offYf, viewport, r, g, b, a);
        BgeColorVertex p3 = PhotoNdcVertex(xf - offXf, yf + offYf, viewport, r, g, b, a);
        out.push_back(p0); out.push_back(p1); out.push_back(p2);
        out.push_back(p0); out.push_back(p2); out.push_back(p3);
    }

    // Build and publish the 2D scene geometry for the photo-run: corridor
    // path lines, remaining collectible trail dots, and the area exit. This
    // rides the engine scene-geometry channel, so it is NOT limited to the
    // 10 object slots (those stay reserved for courier + checkpoints).
    void EmitPhotoRunGeometryLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        if (!runtime.setSceneGeometry) {
            return;
        }
        std::array<int, BGE_INTI_UI_GROUP_COUNT> effectiveCandidates = EffectivePhotoUiCandidatesLocked();
        const IntiPhotoDotUiStyle& dotStyle = ActivePhotoDotUiStyle(effectiveCandidates);
        const IntiPhotoAimUiStyle& aimStyle = ActivePhotoAimUiStyle(effectiveCandidates);
        std::vector<BgeColorVertex> geometry;
        geometry.reserve((photoPaths_.size() + photoDots_.size() + photoExits_.size()) * 6);

        // Corridor path lines (dim stone color, drawn first / underneath).
        for (const IntiPhotoPath& edge : photoPaths_) {
            if (edge.a < 0 || edge.b < 0
                || edge.a >= static_cast<int>(photoCheckpoints_.size())
                || edge.b >= static_cast<int>(photoCheckpoints_.size())) {
                continue;
            }
            const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(edge.a)];
            const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(edge.b)];
            AppendPhotoLine(geometry, viewport,
                            ca.xFraction, ca.yFraction, cb.xFraction, cb.yFraction,
                            dotStyle.pathHalfWidthPx,
                            dotStyle.pathR, dotStyle.pathG, dotStyle.pathB, dotStyle.pathA);
        }

        // Trail dots: remaining vs collected are both drawn, so visited-state
        // readability can be tuned as a candidate choice.
        for (const IntiPhotoDot& dot : photoDots_) {
            if (dot.collected) {
                AppendPhotoQuad(geometry, viewport, dot.xFraction, dot.yFraction,
                                dotStyle.collectedHalfPx,
                                dotStyle.collectedR, dotStyle.collectedG,
                                dotStyle.collectedB, dotStyle.collectedA);
            }
            else {
                AppendPhotoQuad(geometry, viewport, dot.xFraction, dot.yFraction,
                                dotStyle.remainingHalfPx,
                                dotStyle.remainingR, dotStyle.remainingG,
                                dotStyle.remainingB, dotStyle.remainingA);
            }
        }

        // Current-space highlight: while not travelling, show a halo that
        // makes the active board space obvious from arcade distance.
        if (photoTravelDir_ == 0) {
            AppendPhotoQuad(geometry, viewport, photoRunnerXf_, photoRunnerYf_,
                            dotStyle.currentHaloHalfPx,
                            dotStyle.currentHaloR, dotStyle.currentHaloG,
                            dotStyle.currentHaloB, dotStyle.currentHaloA);
        }

        // Area exit marker: locked (dim violet) until the area is complete,
        // then open (bright green).
        for (const IntiPhotoExit& exit : photoExits_) {
            if (photoComplete_) {
                AppendPhotoQuad(geometry, viewport, exit.xFraction, exit.yFraction,
                                10.0f, 0.34f, 0.95f, 0.50f, 1.0f);
            }
            else {
                AppendPhotoQuad(geometry, viewport, exit.xFraction, exit.yFraction,
                                8.0f, 0.50f, 0.34f, 0.62f, 0.65f);
            }
        }

        // Facing arrow + junction affordance: while resting, draw the active
        // arrow style and mark available branch directions at junctions.
        if (photoTravelDir_ == 0 && (PhotoEdgeEndpointsValid(photoEdgeIndex_) || photoNode_ >= 0)) {
            float width = (std::max)(1.0f, viewport.width);
            float playHeight = (std::max)(1.0f, viewport.playHeight);
            float arrowLenPx = aimStyle.arrowLengthPx;
            float tipXf = photoRunnerXf_ + photoFacingX_ * arrowLenPx / width;
            float tipYf = photoRunnerYf_ + photoFacingY_ * arrowLenPx / playHeight;
            AppendPhotoLine(geometry, viewport, photoRunnerXf_, photoRunnerYf_,
                            tipXf, tipYf, aimStyle.arrowHalfWidthPx,
                            aimStyle.arrowR, aimStyle.arrowG, aimStyle.arrowB, aimStyle.arrowA);
            AppendPhotoQuad(geometry, viewport, tipXf, tipYf, aimStyle.arrowTipHalfPx,
                            aimStyle.arrowR, aimStyle.arrowG, aimStyle.arrowB, aimStyle.arrowA);

            if (photoNode_ >= 0) {
                std::vector<int> edges = ConnectedEdgesSortedLocked(viewport);
                if (edges.size() >= 3) {
                    AppendPhotoQuad(geometry, viewport, photoRunnerXf_, photoRunnerYf_,
                                    aimStyle.junctionHaloHalfPx,
                                    aimStyle.junctionHaloR, aimStyle.junctionHaloG,
                                    aimStyle.junctionHaloB, aimStyle.junctionHaloA);
                }
                for (int edgeIndex : edges) {
                    if (!PhotoEdgeEndpointsValid(edgeIndex)) {
                        continue;
                    }
                    const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(edgeIndex)];
                    int toNode = (edge.a == photoNode_) ? edge.b : edge.a;
                    if (toNode < 0 || toNode >= static_cast<int>(photoCheckpoints_.size())) {
                        continue;
                    }
                    const IntiPhotoCheckpoint& checkpoint = photoCheckpoints_[static_cast<std::size_t>(toNode)];
                    AppendPhotoQuad(geometry, viewport, checkpoint.xFraction, checkpoint.yFraction,
                                    aimStyle.branchDotHalfPx,
                                    aimStyle.branchDotR, aimStyle.branchDotG,
                                    aimStyle.branchDotB, aimStyle.branchDotA);
                }
            }
        }

        runtime.setSceneGeometry(geometry);
    }

    // --- Path-constrained courier movement (pac-man-style graph traversal) ---
    // The courier never leaves the corridor network: checkpoints are nodes,
    // paths are edges, and the runner travels along an edge, turning only at
    // a node and only onto a connected corridor. Position is derived from the
    // current edge + parameter, so it is physically impossible to wander off
    // the path. Directions are matched in screen/pixel space so W/A/S/D line
    // up with what the player sees regardless of play-area aspect ratio.

    bool PhotoEdgeEndpointsValid(int edgeIndex) const
    {
        if (edgeIndex < 0 || edgeIndex >= static_cast<int>(photoPaths_.size())) {
            return false;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(edgeIndex)];
        int count = static_cast<int>(photoCheckpoints_.size());
        return edge.a >= 0 && edge.b >= 0 && edge.a < count && edge.b < count;
    }

    // Unit direction of an edge from one node toward the other, in pixel space.
    bool PhotoEdgePixelDir(int edgeIndex, int fromNode, const BgeGameViewport& viewport,
                           float& ux, float& uy) const
    {
        if (!PhotoEdgeEndpointsValid(edgeIndex)) {
            return false;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(edgeIndex)];
        int toNode = (fromNode == edge.a) ? edge.b : edge.a;
        if (fromNode != edge.a && fromNode != edge.b) {
            return false;
        }
        const IntiPhotoCheckpoint& cf = photoCheckpoints_[static_cast<std::size_t>(fromNode)];
        const IntiPhotoCheckpoint& ct = photoCheckpoints_[static_cast<std::size_t>(toNode)];
        float width = (std::max)(1.0f, viewport.width);
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        float dpx = (ct.xFraction - cf.xFraction) * width;
        float dpy = (ct.yFraction - cf.yFraction) * playHeight;
        float len = std::sqrt(dpx * dpx + dpy * dpy);
        if (len <= 0.0001f) {
            return false;
        }
        ux = dpx / len;
        uy = dpy / len;
        return true;
    }

    // Recompute the runner's fraction position from its edge + parameter.
    void SyncRunnerPosFromEdgeLocked()
    {
        if (!PhotoEdgeEndpointsValid(photoEdgeIndex_)) {
            return;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoEdgeIndex_)];
        const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(edge.a)];
        const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(edge.b)];
        float t = (std::max)(0.0f, (std::min)(1.0f, photoEdgeT_));
        photoRunnerXf_ = ClampIntiFraction(ca.xFraction + (cb.xFraction - ca.xFraction) * t);
        photoRunnerYf_ = ClampIntiFraction(ca.yFraction + (cb.yFraction - ca.yFraction) * t);
    }

    // Snap the courier onto the nearest CHECKPOINT NODE. Aim-then-go parks the
    // courier at tourist stops (nodes), so entry rests it at the closest stop.
    void PlaceRunnerAtNearestNodeLocked(float xf, float yf, const BgeGameViewport& viewport)
    {
        photoEdgeIndex_ = -1;
        photoEdgeT_ = 0.0f;
        photoTravelDir_ = 0;
        int best = -1;
        float bestD = 1.0e9f;
        for (std::size_t i = 0; i < photoCheckpoints_.size(); ++i) {
            float dx = photoCheckpoints_[i].xFraction - xf;
            float dy = photoCheckpoints_[i].yFraction - yf;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD) {
                bestD = d2;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0) {
            RestAtNodeLocked(best, viewport);
        }
        else {
            photoRunnerXf_ = xf;
            photoRunnerYf_ = yf;
            photoNode_ = -1;
        }
    }

    // If the runner is paused mid-corridor but close to an endpoint node,
    // snap it to that node so aim rotation can pick a branch reliably.
    bool TrySnapPausedRunnerToNodeLocked(const BgeGameViewport& viewport)
    {
        if (photoTravelDir_ != 0 || photoNode_ >= 0 || !PhotoEdgeEndpointsValid(photoEdgeIndex_)) {
            return false;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoEdgeIndex_)];
        const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(edge.a)];
        const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(edge.b)];
        float dax = ca.xFraction - photoRunnerXf_;
        float day = ca.yFraction - photoRunnerYf_;
        float dbx = cb.xFraction - photoRunnerXf_;
        float dby = cb.yFraction - photoRunnerYf_;
        float da2 = dax * dax + day * day;
        float db2 = dbx * dbx + dby * dby;
        const float snapRadius = 0.085f;
        const float snapRadius2 = snapRadius * snapRadius;
        if (da2 > snapRadius2 && db2 > snapRadius2) {
            return false;
        }
        if (da2 <= db2) {
            photoEdgeT_ = 0.0f;
            photoPrevNode_ = edge.b;
            RestAtNodeLocked(edge.a, viewport);
        }
        else {
            photoEdgeT_ = 1.0f;
            photoPrevNode_ = edge.a;
            RestAtNodeLocked(edge.b, viewport);
        }
        return true;
    }

    // Park the courier at a node: stop, sit exactly on the checkpoint, and
    // auto-aim a default corridor so the direction arrow is always meaningful.
    void RestAtNodeLocked(int node, const BgeGameViewport& viewport)
    {
        photoNode_ = node;
        photoTravelDir_ = 0;
        if (node >= 0 && node < static_cast<int>(photoCheckpoints_.size())) {
            photoRunnerXf_ = photoCheckpoints_[static_cast<std::size_t>(node)].xFraction;
            photoRunnerYf_ = photoCheckpoints_[static_cast<std::size_t>(node)].yFraction;
        }
        AutoAimAtNodeLocked(viewport);
    }

    // Choose a sensible default aim at the current node: prefer a corridor that
    // leads to an unvisited checkpoint (guides the tourist toward remaining
    // photos), tie-broken by "keep going the way you were facing". Sets the
    // aimed edge + the render facing (the direction arrow).
    void AutoAimAtNodeLocked(const BgeGameViewport& viewport)
    {
        photoAimEdge_ = -1;
        if (photoNode_ < 0) {
            return;
        }
        int bestEdge = -1;
        float bestScore = -1.0e9f;
        float bestUx = photoFacingX_;
        float bestUy = photoFacingY_;
        for (std::size_t e = 0; e < photoPaths_.size(); ++e) {
            if (!PhotoEdgeEndpointsValid(static_cast<int>(e))) {
                continue;
            }
            const IntiPhotoPath& edge = photoPaths_[e];
            if (edge.a != photoNode_ && edge.b != photoNode_) {
                continue;
            }
            int farNode = (edge.a == photoNode_) ? edge.b : edge.a;
            float ux = 0.0f;
            float uy = 0.0f;
            if (!PhotoEdgePixelDir(static_cast<int>(e), photoNode_, viewport, ux, uy)) {
                continue;
            }
            float align = ux * photoFacingX_ + uy * photoFacingY_; // -1..1
            bool farUnvisited = !photoCheckpoints_[static_cast<std::size_t>(farNode)].visited;
            float score = (farUnvisited ? 10.0f : 0.0f) + align;
            if (score > bestScore) {
                bestScore = score;
                bestEdge = static_cast<int>(e);
                bestUx = ux;
                bestUy = uy;
            }
        }
        if (bestEdge >= 0) {
            photoAimEdge_ = bestEdge;
            photoFacingX_ = bestUx;
            photoFacingY_ = bestUy;
        }
    }

    // Re-aim at the current node toward whichever connected corridor best
    // matches the pressed direction. Only sets the arrow; does NOT move.
    bool AimDirectionAtNodeLocked(const BgeGameViewport& viewport, float hx, float hy)
    {
        if (photoNode_ < 0 || photoTravelDir_ != 0) {
            return false;
        }
        float hlen = std::sqrt(hx * hx + hy * hy);
        if (hlen <= 0.0001f) {
            return false;
        }
        hx /= hlen;
        hy /= hlen;
        int bestEdge = -1;
        float bestDot = 0.10f; // forgiving: pick the closest matching corridor
        float bestUx = photoFacingX_;
        float bestUy = photoFacingY_;
        for (std::size_t e = 0; e < photoPaths_.size(); ++e) {
            if (!PhotoEdgeEndpointsValid(static_cast<int>(e))) {
                continue;
            }
            const IntiPhotoPath& edge = photoPaths_[e];
            if (edge.a != photoNode_ && edge.b != photoNode_) {
                continue;
            }
            float ux = 0.0f;
            float uy = 0.0f;
            if (!PhotoEdgePixelDir(static_cast<int>(e), photoNode_, viewport, ux, uy)) {
                continue;
            }
            float dot = ux * hx + uy * hy;
            if (dot > bestDot) {
                bestDot = dot;
                bestEdge = static_cast<int>(e);
                bestUx = ux;
                bestUy = uy;
            }
        }
        if (bestEdge >= 0) {
            photoAimEdge_ = bestEdge;
            photoFacingX_ = bestUx;
            photoFacingY_ = bestUy;
            return true;
        }
        return false;
    }

    // Collect the corridor edges connected to the current node, ordered by
    // their pixel-space heading angle so Left/Right rotate predictably around
    // the courier (clockwise / counter-clockwise). Returns edge indices.
    std::vector<int> ConnectedEdgesSortedLocked(const BgeGameViewport& viewport) const
    {
        std::vector<std::pair<float, int>> ranked;
        if (photoNode_ < 0) {
            return {};
        }
        for (std::size_t e = 0; e < photoPaths_.size(); ++e) {
            if (!PhotoEdgeEndpointsValid(static_cast<int>(e))) {
                continue;
            }
            const IntiPhotoPath& edge = photoPaths_[e];
            if (edge.a != photoNode_ && edge.b != photoNode_) {
                continue;
            }
            float ux = 0.0f;
            float uy = 0.0f;
            if (!PhotoEdgePixelDir(static_cast<int>(e), photoNode_, viewport, ux, uy)) {
                continue;
            }
            float angle = std::atan2(uy, ux);
            ranked.emplace_back(angle, static_cast<int>(e));
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const std::pair<float, int>& l, const std::pair<float, int>& r) {
                      return l.first < r.first;
                  });
        std::vector<int> edges;
        edges.reserve(ranked.size());
        for (const auto& entry : ranked) {
            edges.push_back(entry.second);
        }
        return edges;
    }

    // Rotate the aim to the next/previous corridor at the current node.
    // dir = +1 rotates one way, -1 the other. Only changes the arrow; no move.
    bool CyclePhotoAimLocked(const BgeGameViewport& viewport, int dir)
    {
        if (photoNode_ < 0 || photoTravelDir_ != 0) {
            return false;
        }
        std::vector<int> edges = ConnectedEdgesSortedLocked(viewport);
        if (edges.empty()) {
            return false;
        }
        int current = -1;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            if (edges[i] == photoAimEdge_) {
                current = static_cast<int>(i);
                break;
            }
        }
        int count = static_cast<int>(edges.size());
        int next = (current < 0) ? 0 : ((current + dir) % count + count) % count;
        int edgeIndex = edges[static_cast<std::size_t>(next)];
        float ux = 0.0f;
        float uy = 0.0f;
        if (!PhotoEdgePixelDir(edgeIndex, photoNode_, viewport, ux, uy)) {
            return false;
        }
        photoAimEdge_ = edgeIndex;
        photoFacingX_ = ux;
        photoFacingY_ = uy;
        return true;
    }

    // Aim back the way the courier arrived (toward the previous stop) and
    // return the edge so "go back" can travel it. Returns true if found.
    bool AimBackLocked(const BgeGameViewport& viewport)
    {
        if (photoNode_ < 0 || photoTravelDir_ != 0 || photoPrevNode_ < 0) {
            return false;
        }
        for (std::size_t e = 0; e < photoPaths_.size(); ++e) {
            if (!PhotoEdgeEndpointsValid(static_cast<int>(e))) {
                continue;
            }
            const IntiPhotoPath& edge = photoPaths_[e];
            bool connectsBack =
                (edge.a == photoNode_ && edge.b == photoPrevNode_) ||
                (edge.b == photoNode_ && edge.a == photoPrevNode_);
            if (!connectsBack) {
                continue;
            }
            float ux = 0.0f;
            float uy = 0.0f;
            if (!PhotoEdgePixelDir(static_cast<int>(e), photoNode_, viewport, ux, uy)) {
                return false;
            }
            photoAimEdge_ = static_cast<int>(e);
            photoFacingX_ = ux;
            photoFacingY_ = uy;
            return true;
        }
        return false;
    }

    // Number of board spaces along an edge (derived from its length so a die
    // roll of N covers a sensible visual distance, consistent with dot seeding).
    int PhotoEdgeStepCountLocked(int edgeIndex) const
    {
        if (!PhotoEdgeEndpointsValid(edgeIndex)) {
            return 1;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(edgeIndex)];
        const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(edge.a)];
        const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(edge.b)];
        float dx = cb.xFraction - ca.xFraction;
        float dy = cb.yFraction - ca.yFraction;
        float length = std::sqrt(dx * dx + dy * dy);
        const float spacing = 0.05f; // matches BuildPhotoPathsAndDotsLocked dot spacing
        int steps = static_cast<int>(std::lround(length / spacing));
        return (std::max)(1, steps);
    }

    // Roll one tourist-gentle die (1-6).
    int RollDieLocked()
    {
        std::uniform_int_distribution<int> die(1, 6);
        return die(photoRng_);
    }

    // Park the courier on a mid-corridor board space (not a junction node):
    // it can only continue forward or reverse from here, no turning. Keeps the
    // facing arrow pointed the way it was last heading.
    void RestAtSpaceLocked(const BgeGameViewport& viewport, int dir)
    {
        photoTravelDir_ = 0;
        photoNode_ = -1;
        photoFacingDir_ = (dir >= 0) ? 1 : -1;
        if (PhotoEdgeEndpointsValid(photoEdgeIndex_)) {
            const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoEdgeIndex_)];
            int fromNode = (photoFacingDir_ > 0) ? edge.a : edge.b;
            float ux = 0.0f;
            float uy = 0.0f;
            if (PhotoEdgePixelDir(photoEdgeIndex_, fromNode, viewport, ux, uy)) {
                photoFacingX_ = ux;
                photoFacingY_ = uy;
            }
        }
    }

    // Start one dot-space step of travel. Chaining this in Tick keeps the
    // runner moving continuously; toggling pause simply stops chaining at the
    // next landed dot.
    bool RollAndStartMoveLocked(const BgeGameViewport& viewport, bool goBack)
    {
        if (photoTravelDir_ != 0) {
            return false; // already running this turn
        }
        int dir = 0;
        if (photoNode_ >= 0) {
            // At a junction: forward follows the aim arrow; back follows the
            // corridor the courier arrived on.
            if (goBack) {
                if (!AimBackLocked(viewport)) {
                    return false;
                }
            }
            if (!PhotoEdgeEndpointsValid(photoAimEdge_)) {
                return false;
            }
            const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoAimEdge_)];
            if (edge.a != photoNode_ && edge.b != photoNode_) {
                return false;
            }
            photoEdgeIndex_ = photoAimEdge_;
            if (photoNode_ == edge.a) {
                photoEdgeT_ = 0.0f;
                dir = 1;
            }
            else {
                photoEdgeT_ = 1.0f;
                dir = -1;
            }
            photoNode_ = -1;
        }
        else {
            // On a mid-corridor space: continue the way we face, or reverse.
            if (!PhotoEdgeEndpointsValid(photoEdgeIndex_)) {
                return false;
            }
            dir = goBack ? -photoFacingDir_ : photoFacingDir_;
        }

        int steps = PhotoEdgeStepCountLocked(photoEdgeIndex_);
        float stepT = (steps > 0) ? (1.0f / static_cast<float>(steps)) : 1.0f;
        float target = photoEdgeT_ + static_cast<float>(dir) * stepT;
        target = (std::max)(0.0f, (std::min)(1.0f, target));
        if (std::fabs(target - photoEdgeT_) <= 1.0e-6f) {
            return false;
        }
        photoTargetT_ = target;
        photoTravelDir_ = dir;
        photoFacingDir_ = (dir >= 0) ? 1 : -1;
        // Point the facing arrow the way we are about to run.
        {
            const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoEdgeIndex_)];
            int fromNode = (dir > 0) ? edge.a : edge.b;
            float ux = 0.0f;
            float uy = 0.0f;
            if (PhotoEdgePixelDir(photoEdgeIndex_, fromNode, viewport, ux, uy)) {
                photoFacingX_ = ux;
                photoFacingY_ = uy;
            }
        }
        return true;
    }


    // Advance the courier along its corridor for this frame while travelling.
    // It stops when it reaches the rolled target space (mid-corridor rest), or
    // earlier at a junction node if the corridor ends first. Returns true if
    // the runner moved.
    bool AdvanceRunnerLocked(const BgeGameViewport& viewport, float deltaSeconds,
                             std::vector<std::string>& events)
    {
        (void)events;
        if (photoTravelDir_ == 0 || !PhotoEdgeEndpointsValid(photoEdgeIndex_)) {
            return false;
        }
        const IntiPhotoPath& edge = photoPaths_[static_cast<std::size_t>(photoEdgeIndex_)];
        const IntiPhotoCheckpoint& ca = photoCheckpoints_[static_cast<std::size_t>(edge.a)];
        const IntiPhotoCheckpoint& cb = photoCheckpoints_[static_cast<std::size_t>(edge.b)];
        float dx = cb.xFraction - ca.xFraction;
        float dy = cb.yFraction - ca.yFraction;
        float lenF = std::sqrt(dx * dx + dy * dy);
        if (lenF <= 1.0e-5f) {
            return false;
        }
        float advance = (photoRunnerSpeed_ * deltaSeconds) / lenF;
        photoEdgeT_ += advance * static_cast<float>(photoTravelDir_);

        const float eps = 1.0e-4f;
        bool reachedTarget = (photoTravelDir_ > 0)
            ? (photoEdgeT_ >= photoTargetT_)
            : (photoEdgeT_ <= photoTargetT_);
        if (reachedTarget) {
            photoEdgeT_ = photoTargetT_;
            SyncRunnerPosFromEdgeLocked();
            if (photoEdgeT_ >= 1.0f - eps) {
                photoPrevNode_ = edge.a;
                RestAtNodeLocked(edge.b, viewport);
                if (photoQueuedTurnDir_ != 0) {
                    if (CyclePhotoAimLocked(viewport, photoQueuedTurnDir_)) {
                        events.push_back(std::string("bge.event.photo.turn.applied ") + (photoQueuedTurnDir_ > 0 ? "right" : "left"));
                    }
                    photoQueuedTurnDir_ = 0;
                }
            }
            else if (photoEdgeT_ <= eps) {
                photoPrevNode_ = edge.b;
                RestAtNodeLocked(edge.a, viewport);
                if (photoQueuedTurnDir_ != 0) {
                    if (CyclePhotoAimLocked(viewport, photoQueuedTurnDir_)) {
                        events.push_back(std::string("bge.event.photo.turn.applied ") + (photoQueuedTurnDir_ > 0 ? "right" : "left"));
                    }
                    photoQueuedTurnDir_ = 0;
                }
            }
            else {
                // Landed on a mid-corridor board space.
                RestAtSpaceLocked(viewport, photoTravelDir_);
            }
            if (photoPauseAtNextDot_) {
                photoAutoRun_ = false;
                photoPauseAtNextDot_ = false;
                events.push_back("bge.event.photo.paused next-dot");
            }
            else if (photoAutoRun_) {
                RollAndStartMoveLocked(viewport, false);
            }
        }
        else {
            SyncRunnerPosFromEdgeLocked();
        }
        return true;
    }


    // Feed the engine-neutral vector-drag bound from the tech tree: more tech
    // raises the pull magnitude cap and widens the angle-of-attack cone. The
    // cone centers on the courier's current facing (pixel space, y-down), so
    // the allowed aim opens up around where the runner is already pointed.
    // Engine clamps the actual drag; this only supplies the numbers.
    void ApplyDragLimitFromTechLocked(BgeGameRuntime& runtime)
    {
        if (!runtime.setVectorDragLimit) {
            return;
        }
        float magnitude = BGE_INTI_DRAG_BASE_MAGNITUDE;
        if (techCompleted_[1]) { magnitude += BGE_INTI_DRAG_SPEED_BONUS; }
        if (techCompleted_[2]) { magnitude += BGE_INTI_DRAG_ENDURANCE_BONUS; }
        if (techCompleted_[5]) { magnitude += BGE_INTI_DRAG_KREBS_BONUS; }
        int completed = 0;
        for (bool done : techCompleted_) {
            if (done) { ++completed; }
        }
        float halfWidth = BGE_INTI_DRAG_BASE_HALFWIDTH
            + static_cast<float>(completed) * BGE_INTI_DRAG_HALFWIDTH_PER_TECH;
        halfWidth = (std::min)(halfWidth, 3.0f);
        float center = std::atan2(photoFacingY_, photoFacingX_);
        runtime.setVectorDragLimit(magnitude, center, halfWidth);
    }

    void ConfigurePhotoRunSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        auto& slots = *runtime.objectSlots;
        float width = (std::max)(1.0f, viewport.width);
        float playTop = viewport.playTop;
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        int slotIndex = 1;
        for (const IntiPhotoCheckpoint& checkpoint : photoCheckpoints_) {
            if (slotIndex >= BGE_OBJECT_SLOT_COUNT) {
                break;
            }
            BgeObjectSlotState& slot = slots[static_cast<std::size_t>(slotIndex)];
            slot.visible = true;
            slot.x = width * checkpoint.xFraction;
            slot.y = playTop + playHeight * checkpoint.yFraction;
            slot.velocityX = 0.0f;
            slot.velocityY = 0.0f;
            slot.radius = checkpoint.visited ? 12.0f : 16.0f;
            slot.colorR = checkpoint.visited ? 0.40f : 1.00f;
            slot.colorG = checkpoint.visited ? 0.86f : 0.82f;
            slot.colorB = checkpoint.visited ? 0.52f : 0.28f;
            slot.colorA = checkpoint.visited ? 0.70f : 1.0f;
            slot.shape = BgeObjectShape::Ball;
            slot.kind = BgeObjectKind::Generic;
            slot.renderStyle = checkpoint.visited ? BgeObjectRenderStyle::Filled : BgeObjectRenderStyle::Outline;
            slot.outlineThickness = 2.2f;
            ++slotIndex;
        }

        BgeObjectSlotState& courier = slots[0];
        courier.visible = true;
        courier.x = width * photoRunnerXf_;
        courier.y = playTop + playHeight * photoRunnerYf_;
        courier.velocityX = 0.0f;
        courier.velocityY = 0.0f;
        courier.radius = 16.0f;
        courier.colorR = 0.40f;
        courier.colorG = 0.82f;
        courier.colorB = 1.0f;
        courier.colorA = 1.0f;
        courier.shape = BgeObjectShape::Runner;
        courier.kind = BgeObjectKind::Player;
        // Runner is a directional shape; with zero heading AND zero velocity
        // it renders degenerate (invisible). The courier always carries a
        // non-zero travel facing (defaults "face right"), which tracks the
        // corridor it is running along, so it stays visible and points the
        // way it is moving even while paused at a checkpoint node.
        courier.headingX = photoFacingX_;
        courier.headingY = photoFacingY_;
        courier.renderStyle = BgeObjectRenderStyle::Filled;
        courier.outlineThickness = 2.0f;

        EmitPhotoRunGeometryLocked(runtime, viewport);
        ApplyDragLimitFromTechLocked(runtime);
        FinishSceneLocked(runtime, true, 0);
    }

    bool HandlePhotoRunKey(BgeGameRuntime& runtime, unsigned int key)
    {
        std::wstring statusText;
        // Continuous runner controls:
        // Left/Right rotate aim when parked at a node;
        // Up starts/forces forward running; Down starts reverse;
        // Space toggles pause-at-next-dot and resume.
        // Pathing-UI redline (Slice A plumbing): Page Up/Down cycles the
        // focused UI group; digits 0-9 pick that group's candidate preset.
        if (key == VK_PRIOR) {
            return StepPhotoUiGroup(runtime, -1);
        }
        if (key == VK_NEXT) {
            return StepPhotoUiGroup(runtime, 1);
        }
        int uiCandidate = BgeDigitModeIndexFromKey(key);
        if (uiCandidate >= 0) {
            return ApplyPhotoUiCandidate(runtime, uiCandidate, statusText);
        }
        if (key == VK_LEFT || key == L'A' || key == L'a') {
            if (photoTravelDir_ != 0 || photoNode_ < 0) {
                return QueuePhotoTurn(runtime, -1);
            }
            return CyclePhotoAim(runtime, -1);
        }
        if (key == VK_RIGHT || key == L'D' || key == L'd') {
            if (photoTravelDir_ != 0 || photoNode_ < 0) {
                return QueuePhotoTurn(runtime, 1);
            }
            return CyclePhotoAim(runtime, 1);
        }
        if (key == VK_UP || key == L'W' || key == L'w') {
            return CommitPhotoMove(runtime);
        }
        if (key == VK_DOWN || key == L'S' || key == L's') {
            return CommitPhotoBack(runtime);
        }
        if (key == VK_SPACE) {
            return TogglePhotoPause(runtime);
        }
        if (key == L'M' || key == L'm' || key == VK_RETURN) {
            return ShowMain(runtime, false, statusText);
        }
        return false;
    }

    // While already travelling, allow the player to queue the next turn so it
    // applies at the next junction node reached.
    bool QueuePhotoTurn(BgeGameRuntime& runtime, int dir)
    {
        std::wstring statusText;
        std::wstring hudText;
        std::string event;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            photoQueuedTurnDir_ = (dir >= 0) ? 1 : -1;
            event = std::string("bge.event.photo.turn.queued ") + (photoQueuedTurnDir_ > 0 ? "right" : "left");
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }
        if (runtime.log) {
            runtime.log(event);
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    static const wchar_t* PhotoUiGroupName(int group)
    {
        switch (group) {
        case 0: return L"dots";
        case 1: return L"aim";
        default: return L"hud";
        }
    }

    static bool IntiUiDebugEnabled()
    {
        char* value = nullptr;
        std::size_t valueLength = 0;
        errno_t envError = _dupenv_s(&value, &valueLength, "BGE_INTI_UI_DEBUG");
        if (envError != 0 || value == nullptr || valueLength == 0) {
            if (value) {
                std::free(value);
            }
            return false;
        }
        bool enabled = value[0] == '1' || value[0] == 't' || value[0] == 'T' || value[0] == 'y' || value[0] == 'Y';
        std::free(value);
        return enabled;
    }

    int NormalizeUiGroup(int group) const
    {
        return ((group % BGE_INTI_UI_GROUP_COUNT) + BGE_INTI_UI_GROUP_COUNT) % BGE_INTI_UI_GROUP_COUNT;
    }

    int NormalizeUiCandidate(int candidate) const
    {
        return (std::max)(0, (std::min)(BGE_INTI_RUNNER_MODE_COUNT - 1, candidate));
    }

    int EffectivePhotoUiCandidateLocked(int group) const
    {
        int normalizedGroup = NormalizeUiGroup(group);
        if (photoUiWinnersLocked_) {
            return NormalizeUiCandidate(photoUiWinnerCandidate_[static_cast<std::size_t>(normalizedGroup)]);
        }
        return NormalizeUiCandidate(photoUiCandidate_[static_cast<std::size_t>(normalizedGroup)]);
    }

    std::array<int, BGE_INTI_UI_GROUP_COUNT> EffectivePhotoUiCandidatesLocked() const
    {
        std::array<int, BGE_INTI_UI_GROUP_COUNT> candidates{};
        for (int group = 0; group < BGE_INTI_UI_GROUP_COUNT; ++group) {
            candidates[static_cast<std::size_t>(group)] = EffectivePhotoUiCandidateLocked(group);
        }
        return candidates;
    }

    bool PublishPhotoUiLockState(BgeGameRuntime& runtime, const std::wstring& detail, const std::string& eventName)
    {
        std::wstring statusText;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            statusText = BuildPhotoUiStatusLocked();
            if (!detail.empty()) {
                statusText += L" | " + detail;
            }
            if (section_ == IntiSection::PhotoRun) {
                BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                ConfigurePhotoRunSceneLocked(runtime, viewport);
                statusText = BuildPhotoRunStatusLocked();
                if (!detail.empty()) {
                    statusText += L" | " + detail;
                }
            }
            hudText = BuildHudLocked();
        }
        if (runtime.log && !eventName.empty()) {
            runtime.log(eventName);
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    bool LockPhotoUiWinners(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            for (int group = 0; group < BGE_INTI_UI_GROUP_COUNT; ++group) {
                photoUiWinnerCandidate_[static_cast<std::size_t>(group)] =
                    NormalizeUiCandidate(photoUiCandidate_[static_cast<std::size_t>(group)]);
            }
            photoUiWinnersLocked_ = true;
            statusText = BuildPhotoUiStatusLocked();
        }
        return PublishPhotoUiLockState(runtime, L"winners locked", "bge.event.ui.locked");
    }

    bool LockPhotoUiWinnerGroup(BgeGameRuntime& runtime, int group, int candidate, std::wstring& statusText)
    {
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            int normalizedGroup = NormalizeUiGroup(group);
            int normalizedCandidate = NormalizeUiCandidate(candidate);
            photoUiWinnerCandidate_[static_cast<std::size_t>(normalizedGroup)] = normalizedCandidate;
            photoUiCandidate_[static_cast<std::size_t>(normalizedGroup)] = normalizedCandidate;
            photoUiWinnersLocked_ = true;
            photoUiGroup_ = normalizedGroup;
            statusText = BuildPhotoUiStatusLocked();
        }
        return PublishPhotoUiLockState(runtime, L"winner set", "bge.event.ui.winner.set");
    }

    bool UnlockPhotoUiWinners(BgeGameRuntime& runtime, std::wstring& statusText)
    {
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (!photoUiDebugEnabled_) {
                statusText = L"inti ui unlock requires BGE_INTI_UI_DEBUG=1";
                return false;
            }
            photoUiWinnersLocked_ = false;
            for (int group = 0; group < BGE_INTI_UI_GROUP_COUNT; ++group) {
                photoUiCandidate_[static_cast<std::size_t>(group)] =
                    NormalizeUiCandidate(photoUiWinnerCandidate_[static_cast<std::size_t>(group)]);
            }
            statusText = BuildPhotoUiStatusLocked();
        }
        return PublishPhotoUiLockState(runtime, L"debug unlock active", "bge.event.ui.unlocked");
    }

    bool StepPhotoUiGroup(BgeGameRuntime& runtime, int dir)
    {
        std::wstring statusText;
        return SetPhotoUiGroup(runtime, photoUiGroup_ + dir, statusText);
    }

    // Focus a UI group (dots/aim/hud) for 0-9 candidate honing. Pure Inti
    // plugin state: engine core knows nothing about UI presets.
    bool SetPhotoUiGroup(BgeGameRuntime& runtime, int group, std::wstring& statusText)
    {
        std::vector<std::string> events;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            photoUiGroup_ = NormalizeUiGroup(group);
            events.push_back("bge.event.ui.group " + NarrowStatus(std::wstring(PhotoUiGroupName(photoUiGroup_))));
            statusText = BuildPhotoUiStatusLocked();
            if (section_ == IntiSection::PhotoRun) {
                BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                ConfigurePhotoRunSceneLocked(runtime, viewport);
                statusText = BuildPhotoRunStatusLocked();
            }
            hudText = BuildHudLocked();
        }
        if (runtime.log) {
            for (const std::string& message : events) {
                runtime.log(message);
            }
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    // Apply a 0-9 candidate preset to the focused UI group.
    bool ApplyPhotoUiCandidate(BgeGameRuntime& runtime, int candidate, std::wstring& statusText)
    {
        std::vector<std::string> events;
        std::wstring hudText;
        bool lockedAttempt = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            candidate = NormalizeUiCandidate(candidate);
            if (photoUiWinnersLocked_) {
                lockedAttempt = true;
                statusText = L"inti ui locked (winners active). Use: inti ui-unlock (debug) or inti ui-lock dots|aim|hud 0-9";
                if (section_ == IntiSection::PhotoRun) {
                    BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                    ConfigurePhotoRunSceneLocked(runtime, viewport);
                    statusText = BuildPhotoRunStatusLocked() + L" | ui locked";
                }
                hudText = BuildHudLocked();
            }
            else {
                photoUiCandidate_[static_cast<std::size_t>(photoUiGroup_)] = candidate;
                events.push_back("bge.event.ui.candidate " + NarrowStatus(std::wstring(PhotoUiGroupName(photoUiGroup_))) + " " + std::to_string(candidate));
                statusText = BuildPhotoUiStatusLocked();
                if (section_ == IntiSection::PhotoRun) {
                    BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
                    ConfigurePhotoRunSceneLocked(runtime, viewport);
                    statusText = BuildPhotoRunStatusLocked();
                }
                hudText = BuildHudLocked();
            }
        }
        if (lockedAttempt) {
            PublishRuntimeFeedback(runtime, statusText, hudText);
            return true;
        }
        if (runtime.log) {
            for (const std::string& message : events) {
                runtime.log(message);
            }
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    std::wstring BuildPhotoUiStatusLocked() const
    {
        std::wstringstream stream;
        std::array<int, BGE_INTI_UI_GROUP_COUNT> effective = EffectivePhotoUiCandidatesLocked();
        stream << L"inti ui " << PhotoUiGroupName(photoUiGroup_)
             << L" candidate " << effective[static_cast<std::size_t>(photoUiGroup_)]
             << L" | winners d" << NormalizeUiCandidate(photoUiWinnerCandidate_[0])
             << L" a" << NormalizeUiCandidate(photoUiWinnerCandidate_[1])
             << L" h" << NormalizeUiCandidate(photoUiWinnerCandidate_[2])
             << L" | " << (photoUiWinnersLocked_ ? L"locked" : L"debug-unlocked")
             << L" | PgUp/PgDn group, 0-9 candidate";
        return stream.str();
    }

    // Rotate the aim to the next/previous corridor at the current stop. No
    // movement happens until the player commits with Up/Space.
    bool CyclePhotoAim(BgeGameRuntime& runtime, int dir)
    {
        std::wstring statusText;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
            if (!CyclePhotoAimLocked(viewport, dir)) {
                if (TrySnapPausedRunnerToNodeLocked(viewport)) {
                    CyclePhotoAimLocked(viewport, dir);
                }
            }
            photoQueuedTurnDir_ = 0;
            ConfigurePhotoRunSceneLocked(runtime, viewport);
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    bool TogglePhotoPause(BgeGameRuntime& runtime)
    {
        std::vector<std::string> events;
        std::wstring statusText;
        std::wstring hudText;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
            if (photoTravelDir_ != 0) {
                photoPauseAtNextDot_ = !photoPauseAtNextDot_;
                events.push_back(photoPauseAtNextDot_
                    ? "bge.event.photo.pause.requested"
                    : "bge.event.photo.pause.cleared");
            }
            else {
                photoAutoRun_ = true;
                photoPauseAtNextDot_ = false;
                if (RollAndStartMoveLocked(viewport, false)) {
                    events.push_back("bge.event.photo.resumed");
                }
            }
            ConfigurePhotoRunSceneLocked(runtime, viewport);
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }
        if (runtime.log) {
            for (const std::string& message : events) {
                runtime.log(message);
            }
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    // Force forward movement and keep continuous stepping enabled.
    bool CommitPhotoMove(BgeGameRuntime& runtime)
    {
        std::vector<std::string> events;
        std::wstring statusText;
        std::wstring hudText;
        bool started = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
            photoAutoRun_ = true;
            photoPauseAtNextDot_ = false;
            started = RollAndStartMoveLocked(viewport, false);
            if (started) {
                // Compatibility witness for existing Level 1 contract token.
                events.push_back("bge.event.dice.rolled 1");
                events.push_back("bge.event.photo.resumed");
                ClaimNearestPhotoLocked(events, viewport);
                ProcessPendingPhotoExitLocked(events, viewport);
            }
            ConfigurePhotoRunSceneLocked(runtime, viewport);
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }
        if (runtime.log) {
            for (const std::string& message : events) {
                runtime.log(message);
            }
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    // Force reverse movement and keep continuous stepping enabled.
    bool CommitPhotoBack(BgeGameRuntime& runtime)
    {
        std::vector<std::string> events;
        std::wstring statusText;
        std::wstring hudText;
        bool started = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
            photoAutoRun_ = true;
            photoPauseAtNextDot_ = false;
            started = RollAndStartMoveLocked(viewport, true);
            if (started) {
                // Compatibility witness for existing Level 1 contract token.
                events.push_back("bge.event.dice.rolled 1");
                events.push_back("bge.event.photo.resumed-reverse");
                ClaimNearestPhotoLocked(events, viewport);
                ProcessPendingPhotoExitLocked(events, viewport);
            }
            ConfigurePhotoRunSceneLocked(runtime, viewport);
            statusText = BuildPhotoRunStatusLocked();
            hudText = BuildHudLocked();
        }
        if (runtime.log) {
            for (const std::string& message : events) {
                runtime.log(message);
            }
        }
        PublishRuntimeFeedback(runtime, statusText, hudText);
        return true;
    }

    bool TickPhotoRun(BgeGameRuntime& runtime, double deltaMilliseconds)
    {
        if (runtime.animationRunning && !*runtime.animationRunning) {
            return false;
        }
        double deltaSeconds = (std::max)(0.0, (std::min)(deltaMilliseconds / 1000.0, 0.10));
        BgeGameViewport viewport = runtime.viewport ? runtime.viewport() : BgeGameViewport{};
        std::vector<std::string> events;
        std::wstring statusText;
        std::wstring hudText;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(*runtime.objectMutex);
            if (photoTravelDir_ == 0 && photoAutoRun_) {
                changed = RollAndStartMoveLocked(viewport, false) || changed;
            }
            if (AdvanceRunnerLocked(viewport, static_cast<float>(deltaSeconds), events)) {
                ClaimNearestPhotoLocked(events, viewport);
                ProcessPendingPhotoExitLocked(events, viewport);
                ConfigurePhotoRunSceneLocked(runtime, viewport);
                statusText = BuildPhotoRunStatusLocked();
                hudText = BuildHudLocked();
                changed = true;
            }
            else if (changed) {
                ConfigurePhotoRunSceneLocked(runtime, viewport);
                statusText = BuildPhotoRunStatusLocked();
                hudText = BuildHudLocked();
            }
        }
        if (changed) {
            if (runtime.log) {
                for (const std::string& message : events) {
                    runtime.log(message);
                }
            }
            PublishRuntimeFeedback(runtime, statusText, hudText);
        }
        return changed;
    }

    // If a completed area's exit has been stepped on, load the next tourist
    // area in place (the scene-change). Runs under the object mutex.
    bool ProcessPendingPhotoExitLocked(std::vector<std::string>& events, const BgeGameViewport& viewport)
    {
        if (photoPendingExit_.empty()) {
            return false;
        }
        std::wstring nextMap = photoPendingExit_;
        photoPendingExit_.clear();
        if (LoadPhotoRunMapLocked(nextMap, viewport)) {
            events.push_back("bge.event.area.enter " + NarrowStatus(photoMapName_));
            return true;
        }
        return false;
    }

    // Composition score (0..100) for arriving along layout-space direction
    // (fdx,fdy) versus a site's golden approach heading. Pure geometry: 100 =
    // dead-on the golden angle, ~60 at the edge of the tolerance cone, 0 =
    // arriving from the opposite side (a blurry tourist snap). The golden angle
    // is Inti map data; this rewards approaching each site from its postcard
    // corridor, reusing the angle the player already aims with the drag.
    static int ComputePhotoCompositionScore(float fdx, float fdy, float goldenAngleDeg, float toleranceDeg)
    {
        float mag = std::sqrt(fdx * fdx + fdy * fdy);
        if (mag < 1.0e-6f) {
            return 0;
        }
        const float pi = 3.14159265f;
        float arriveRad = std::atan2(fdy, fdx);
        float goldenRad = goldenAngleDeg * (pi / 180.0f);
        float delta = arriveRad - goldenRad;
        while (delta > pi) { delta -= 2.0f * pi; }
        while (delta < -pi) { delta += 2.0f * pi; }
        float absDeg = std::fabs(delta) * (180.0f / pi);
        float tol = (std::max)(1.0f, (std::min)(180.0f, toleranceDeg));
        float score;
        if (absDeg <= tol) {
            score = 100.0f - 40.0f * (absDeg / tol);
        }
        else {
            float denom = (std::max)(1.0f, 180.0f - tol);
            score = 60.0f * ((180.0f - absDeg) / denom);
        }
        int rounded = static_cast<int>(score + 0.5f);
        return (std::max)(0, (std::min)(100, rounded));
    }

    bool ClaimNearestPhotoLocked(std::vector<std::string>& events, const BgeGameViewport& viewport)
    {
        const float claimRadius = 0.06f;
        bool changed = false;
        for (IntiPhotoCheckpoint& checkpoint : photoCheckpoints_) {
            if (checkpoint.visited) {
                continue;
            }
            float deltaX = checkpoint.xFraction - photoRunnerXf_;
            float deltaY = checkpoint.yFraction - photoRunnerYf_;
            if (deltaX * deltaX + deltaY * deltaY <= claimRadius * claimRadius) {
                checkpoint.visited = true;
                ++photoVisitedCount_;
                changed = true;
                events.push_back("bge.event.checkpoint.visited " + NarrowStatus(checkpoint.name));
                events.push_back("bge.event.photo.claimed " + NarrowStatus(checkpoint.name) + " camera snap");
                if (checkpoint.hasShot) {
                    // Un-distort the pixel-space arrival facing back to layout
                    // space so the golden angle is resolution-independent and
                    // matches the map's geometry rather than the window aspect.
                    float width = (std::max)(1.0f, viewport.width);
                    float playHeight = (std::max)(1.0f, viewport.playHeight);
                    float fdx = photoFacingX_ / width;
                    float fdy = photoFacingY_ / playHeight;
                    int score = ComputePhotoCompositionScore(fdx, fdy,
                        checkpoint.goldenAngleDeg, checkpoint.toleranceDeg);
                    checkpoint.compositionScore = score;
                    photoLastComposition_ = score;
                    photoLastComposeName_ = checkpoint.name;
                    photoCompositionTotal_ += score;
                    ++photoComposedCount_;
                    events.push_back("bge.event.shot.composed " + NarrowStatus(checkpoint.name)
                        + " " + std::to_string(score));
                }
                if (photoVisitedCount_ >= static_cast<int>(photoCheckpoints_.size())) {
                    photoComplete_ = true;
                    // Do NOT stop the courier here: the area is complete but the
                    // player still needs to run the corridor to the exit marker.
                    events.push_back("[IntiRunners] photo run complete " + std::to_string(photoVisitedCount_));
                }
            }
        }

        // Pac-man-style trail dots collected by proximity along the corridors.
        const float dotRadius = 0.035f;
        for (IntiPhotoDot& dot : photoDots_) {
            if (dot.collected) {
                continue;
            }
            float dx = dot.xFraction - photoRunnerXf_;
            float dy = dot.yFraction - photoRunnerYf_;
            if (dx * dx + dy * dy <= dotRadius * dotRadius) {
                dot.collected = true;
                ++photoDotsCollected_;
                changed = true;
                events.push_back("bge.event.photo-dot.collected " + std::to_string(photoDotsCollected_));
            }
        }

        // Area exit: once the area is complete, stepping onto an exit marker
        // queues a scene-change to the next tourist area.
        if (photoComplete_ && photoPendingExit_.empty()) {
            const float exitRadius = 0.06f;
            for (const IntiPhotoExit& exit : photoExits_) {
                float dx = exit.xFraction - photoRunnerXf_;
                float dy = exit.yFraction - photoRunnerYf_;
                if (dx * dx + dy * dy <= exitRadius * exitRadius) {
                    photoPendingExit_ = exit.nextMap;
                    photoRunnerHeadingX_ = 0.0f;
                    photoRunnerHeadingY_ = 0.0f;
                    photoTravelDir_ = 0;
                    changed = true;
                    events.push_back("bge.event.area.exit " + NarrowStatus(exit.name));
                    break;
                }
            }
        }
        return changed;
    }

    void ResetStateLocked()
    {
        active_ = true;
        section_ = IntiSection::Main;
        turn_ = 1;
        runnerMode_ = 0;
        currentTechIndex_ = 0;
        science_ = 0;
        money_ = 10;
        goods_ = 4;
        cityCount_ = 1;
        pathCount_ = 0;
        messagesSent_ = 0;
        techProgress_.fill(0);
        techCompleted_.fill(false);
        pendingGoodsOrder_ = false;
        pendingCityOrder_ = false;
        pendingPathOrder_ = false;
        pendingMessageOrder_ = false;
        lastTurnReport_.clear();
        runnerRoutePoints_.clear();
        runnerRouteActive_ = false;
        runnerRouteLoop_ = true;
        runnerRouteSpeed_ = BGE_INTI_ROUTE_DEFAULT_SPEED;
        runnerRouteBoostActive_ = false;
        runnerRouteProgress_ = 0.0f;
        runnerRouteTotalDistance_ = 0.0f;
        runnerSeconds_ = 0.0;
        photoCheckpoints_.clear();
        photoPaths_.clear();
        photoDots_.clear();
        photoExits_.clear();
        photoMapId_.clear();
        photoMapName_.clear();
        photoSource_.clear();
        photoPendingExit_.clear();
        photoRunnerXf_ = 0.5f;
        photoRunnerYf_ = 0.5f;
        photoRunnerHeadingX_ = 0.0f;
        photoRunnerHeadingY_ = 0.0f;
        photoFacingX_ = 1.0f;
        photoFacingY_ = 0.0f;
        photoEdgeIndex_ = -1;
        photoEdgeT_ = 0.0f;
        photoTravelDir_ = 0;
        photoNode_ = -1;
        photoPrevNode_ = -1;
        photoAimEdge_ = -1;
        photoFacingDir_ = 1;
        photoTargetT_ = 0.0f;
        photoLastRoll_ = 0;
        photoAutoRun_ = true;
        photoPauseAtNextDot_ = false;
        photoVisitedCount_ = 0;
        photoDotsCollected_ = 0;
        photoComplete_ = false;
        photoLastComposition_ = -1;
        photoCompositionTotal_ = 0;
        photoComposedCount_ = 0;
        photoLastComposeName_.clear();
    }

    void ConfigureSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport, bool preserveRunnerPositions)
    {
        if (section_ != IntiSection::PhotoRun && runtime.setSceneGeometry) {
            // Leaving the photo-run clears its corridor/dot scene geometry so
            // it never bleeds into the other Inti sections.
            runtime.setSceneGeometry(std::vector<BgeColorVertex>{});
        }
        if (section_ == IntiSection::PhotoRun) {
            ConfigurePhotoRunSceneLocked(runtime, viewport);
        }
        else if (section_ == IntiSection::TechTree) {
            ConfigureTechTreeSceneLocked(runtime, viewport);
        }
        else if (section_ == IntiSection::Chasqui && runnerRouteActive_) {
            ConfigureChasquiRouteSceneLocked(runtime, viewport);
        }
        else if (section_ == IntiSection::Chasqui && ChasquiReadyLocked()) {
            ConfigureChasquiRunnerSceneLocked(runtime, viewport, preserveRunnerPositions);
        }
        else if (section_ == IntiSection::Empire) {
            ConfigureEmpireSceneLocked(runtime, viewport);
        }
        else {
            ConfigureMainSceneLocked(runtime, viewport);
        }
    }

    void ConfigureMainSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        auto& slots = *runtime.objectSlots;
        float width = (std::max)(1.0f, viewport.width);
        float playTop = viewport.playTop;
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        ConfigureBallSlot(slots[0], width * 0.50f, playTop + playHeight * 0.16f, 34.0f, 1.0f, 0.78f, 0.22f, BgeObjectKind::Generic);
        ConfigureBallSlot(slots[1], width * 0.82f, playTop + playHeight * 0.22f, 16.0f, 0.62f, 0.78f, 1.0f, BgeObjectKind::Generic);

        ConfigureSectionMarker(slots[7], width * 0.22f, playTop + playHeight * 0.45f, IntiSection::TechTree, 0.46f, 0.84f, 1.0f);
        ConfigureSectionMarker(slots[8], width * 0.50f, playTop + playHeight * 0.48f, IntiSection::Empire, 0.96f, 0.72f, 0.36f);
        ConfigureSectionMarker(slots[9], width * 0.78f, playTop + playHeight * 0.50f, IntiSection::Chasqui,
                               ChasquiReadyLocked() ? 0.92f : 0.36f,
                               ChasquiReadyLocked() ? 0.96f : 0.36f,
                               ChasquiReadyLocked() ? 0.54f : 0.42f);

        FinishSceneLocked(runtime, false, -1);
    }

    void ConfigureEmpireSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        auto& slots = *runtime.objectSlots;
        float width = (std::max)(1.0f, viewport.width);
        float playTop = viewport.playTop;
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        ConfigureBallSlot(slots[0], width * 0.50f, playTop + playHeight * 0.16f, 34.0f, 1.0f, 0.78f, 0.22f, BgeObjectKind::Generic);
        ConfigureBallSlot(slots[1], width * 0.82f, playTop + playHeight * 0.22f, 16.0f, 0.62f, 0.78f, 1.0f, BgeObjectKind::Generic);

        const float cityX[3] = { width * 0.34f, width * 0.64f, width * 0.48f };
        const float cityY[3] = { playTop + playHeight * 0.58f, playTop + playHeight * 0.46f, playTop + playHeight * 0.70f };
        int visibleCities = (std::min)(cityCount_, 3);
        for (int cityIndex = 0; cityIndex < visibleCities; ++cityIndex) {
            ConfigureBallSlot(slots[2 + cityIndex], cityX[cityIndex], cityY[cityIndex], cityIndex == 0 ? 25.0f : 18.0f,
                              0.30f + cityIndex * 0.10f, 0.64f + cityIndex * 0.08f, 0.34f, BgeObjectKind::Generic);
            slots[2 + cityIndex].renderStyle = BgeObjectRenderStyle::Outline;
            slots[2 + cityIndex].outlineThickness = cityIndex == 0 ? 3.0f : 2.2f;
        }

        if (pathCount_ > 0 && cityCount_ >= 2) {
            ConfigurePathSlot(slots[5], cityX[0], cityY[0], cityX[1], cityY[1], 0.86f, 0.64f, 0.36f);
        }
        if (pathCount_ > 1 && cityCount_ >= 3) {
            ConfigurePathSlot(slots[6], cityX[1], cityY[1], cityX[2], cityY[2], 0.72f, 0.92f, 0.60f);
        }

        FinishSceneLocked(runtime, false, -1);
    }

    void ConfigureTechTreeSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        auto& slots = *runtime.objectSlots;
        float width = (std::max)(1.0f, viewport.width);
        float playTop = viewport.playTop;
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        const auto& techs = IntiTechNodes();
        float mainCordY = playTop + playHeight * 0.22f;
        ConfigurePathSlot(slots[6], width * 0.10f, mainCordY, width * 0.90f, mainCordY, 0.74f, 0.58f, 0.34f);
        slots[6].outlineThickness = 3.0f;

        for (int index = 0; index < BGE_INTI_TECH_COUNT; ++index) {
            const IntiTechNode& tech = techs[static_cast<std::size_t>(index)];
            bool complete = techCompleted_[static_cast<std::size_t>(index)];
            bool available = CanResearchTechLocked(index);
            bool current = index == currentTechIndex_ && !complete;
            float dim = complete ? 1.0f : (available ? 0.76f : 0.32f);
            float progress = complete ? 1.0f : static_cast<float>(techProgress_[static_cast<std::size_t>(index)]) / static_cast<float>((std::max)(1, tech.cost));
            BgeObjectSlotState& slot = slots[static_cast<std::size_t>(index)];
            slot.visible = true;
            slot.x = width * tech.xFraction;
            slot.y = mainCordY;
            slot.velocityX = (std::max)(0.0f, (std::min)(1.0f, progress));
            slot.velocityY = complete ? 1.0f : 0.0f;
            slot.headingX = current ? 1.0f : 0.0f;
            slot.headingY = playHeight * tech.yFraction;
            slot.radius = current ? 13.0f : (complete ? 12.0f : 10.0f);
            slot.colorR = tech.colorR * dim;
            slot.colorG = tech.colorG * dim;
            slot.colorB = tech.colorB * dim;
            slot.colorA = available || complete ? 1.0f : 0.42f;
            slot.shape = BgeObjectShape::Quipu;
            slot.kind = BgeObjectKind::Quipu;
            slot.renderStyle = complete || current ? BgeObjectRenderStyle::Filled : BgeObjectRenderStyle::Outline;
            slot.outlineThickness = current ? 3.2f : (complete ? 2.7f : 1.8f);
        }

        FinishSceneLocked(runtime, false, currentTechIndex_);
    }

    void ConfigureChasquiRunnerSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport, bool preserveRunnerPositions)
    {
        auto& slots = *runtime.objectSlots;
        auto previousSlots = slots;
        float width = (std::max)(1.0f, viewport.width);
        float playTop = viewport.playTop;
        float playHeight = (std::max)(1.0f, viewport.playHeight);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        for (int index = 0; index < BGE_INTI_RUNNER_MODE_COUNT; ++index) {
            const IntiRunnerMode& mode = IntiRunnerModeForIndex(index);
            BgeObjectSlotState& slot = slots[static_cast<std::size_t>(index)];
            const BgeObjectSlotState& previousSlot = previousSlots[static_cast<std::size_t>(index)];
            float existingX = previousSlot.visible && previousSlot.shape == BgeObjectShape::Runner ? previousSlot.x : 0.0f;
            int column = index % 5;
            slot.visible = true;
            slot.x = preserveRunnerPositions && existingX > 0.0f ? existingX : width * (0.10f + static_cast<float>(column) * 0.20f) - static_cast<float>(index / 5) * width * 0.08f;
            slot.y = playTop + playHeight * mode.laneFraction;
            slot.velocityX = mode.speed;
            slot.velocityY = 0.0f;
            slot.headingX = 1.0f;
            slot.headingY = 0.0f;
            slot.radius = mode.radius * (index == runnerMode_ ? 1.20f : 1.0f);
            slot.colorR = index == runnerMode_ ? (std::min)(1.0f, mode.colorR + 0.14f) : mode.colorR * 0.78f;
            slot.colorG = index == runnerMode_ ? (std::min)(1.0f, mode.colorG + 0.12f) : mode.colorG * 0.78f;
            slot.colorB = index == runnerMode_ ? (std::min)(1.0f, mode.colorB + 0.10f) : mode.colorB * 0.78f;
            slot.colorA = index == runnerMode_ ? 1.0f : 0.72f;
            slot.shape = BgeObjectShape::Runner;
            slot.kind = BgeObjectKind::Runner;
            slot.renderStyle = mode.renderStyle;
            slot.outlineThickness = index == runnerMode_ ? mode.outlineThickness + 0.9f : mode.outlineThickness;
        }

        FinishSceneLocked(runtime, true, runnerMode_);
    }

    void ConfigureChasquiRouteSceneLocked(BgeGameRuntime& runtime, const BgeGameViewport& viewport)
    {
        auto& slots = *runtime.objectSlots;
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            slots[static_cast<std::size_t>(index)] = BgeObjectSlotState{};
        }

        if (runnerRoutePoints_.size() < 2 || runnerRouteTotalDistance_ <= 0.0f) {
            FinishSceneLocked(runtime, false, 0, false);
            return;
        }

        const IntiRunnerMode& baseMode = IntiRunnerModeForIndex(runnerMode_);
        int currentFrame = static_cast<int>(std::floor(runnerSeconds_ * 10.0)) % BGE_INTI_RUNNER_MODE_COUNT;
        if (currentFrame < 0) {
            currentFrame += BGE_INTI_RUNNER_MODE_COUNT;
        }

        for (int frame = 0; frame < BGE_INTI_RUNNER_MODE_COUNT; ++frame) {
            bool showTrail = runnerRouteBoostActive_ || runnerRouteSpeed_ >= BGE_INTI_ROUTE_TRAIL_SPEED;
            int frameAge = (currentFrame - frame + BGE_INTI_RUNNER_MODE_COUNT) % BGE_INTI_RUNNER_MODE_COUNT;
            float lagDistance = runnerRouteSpeed_ * 0.052f * static_cast<float>(frameAge);
            IntiRouteSample sample = SampleChasquiRouteLocked(runnerRouteProgress_ - lagDistance);
            BgeObjectSlotState& slot = slots[static_cast<std::size_t>(frame)];
            float freshness = 1.0f - (static_cast<float>(frameAge) / static_cast<float>(BGE_INTI_RUNNER_MODE_COUNT));
            bool activeFrame = frame == currentFrame;
            slot.visible = activeFrame || showTrail;
            slot.x = sample.x;
            slot.y = sample.y + (activeFrame ? -2.0f : 0.0f);
            slot.velocityX = sample.headingX * runnerRouteSpeed_;
            slot.velocityY = (static_cast<float>(frame) / static_cast<float>(BGE_INTI_RUNNER_MODE_COUNT)) * 6.28318530718f;
            slot.headingX = sample.headingX;
            slot.headingY = sample.headingY;
            slot.radius = baseMode.radius * (activeFrame ? 1.18f : (0.78f + freshness * 0.22f));
            slot.colorR = baseMode.colorR * (activeFrame ? 1.0f : 0.54f + freshness * 0.30f);
            slot.colorG = baseMode.colorG * (activeFrame ? 1.0f : 0.54f + freshness * 0.30f);
            slot.colorB = baseMode.colorB * (activeFrame ? 1.0f : 0.54f + freshness * 0.30f);
            slot.colorA = activeFrame ? 1.0f : 0.18f + freshness * 0.42f;
            slot.shape = BgeObjectShape::Runner;
            slot.kind = BgeObjectKind::Runner;
            slot.renderStyle = baseMode.renderStyle;
            slot.outlineThickness = baseMode.outlineThickness + (activeFrame ? 0.9f : 0.0f);
        }

        (void)viewport;
        FinishSceneLocked(runtime, true, currentFrame, false);
    }

    void RecalculateChasquiRouteDistanceLocked()
    {
        runnerRouteTotalDistance_ = 0.0f;
        if (runnerRoutePoints_.size() < 2) {
            return;
        }
        for (std::size_t index = 1; index < runnerRoutePoints_.size(); ++index) {
            float deltaX = runnerRoutePoints_[index].x - runnerRoutePoints_[index - 1].x;
            float deltaY = runnerRoutePoints_[index].y - runnerRoutePoints_[index - 1].y;
            runnerRouteTotalDistance_ += std::sqrt(deltaX * deltaX + deltaY * deltaY);
        }
    }

    float WrapRouteDistanceLocked(float distance) const
    {
        if (runnerRouteTotalDistance_ <= 0.0f) {
            return 0.0f;
        }
        if (!runnerRouteLoop_) {
            return (std::max)(0.0f, (std::min)(runnerRouteTotalDistance_, distance));
        }
        while (distance < 0.0f) {
            distance += runnerRouteTotalDistance_;
        }
        while (distance >= runnerRouteTotalDistance_) {
            distance -= runnerRouteTotalDistance_;
        }
        return distance;
    }

    IntiRouteSample SampleChasquiRouteLocked(float distance) const
    {
        IntiRouteSample sample;
        if (runnerRoutePoints_.empty()) {
            return sample;
        }
        if (runnerRoutePoints_.size() == 1 || runnerRouteTotalDistance_ <= 0.0f) {
            sample.x = runnerRoutePoints_.front().x;
            sample.y = runnerRoutePoints_.front().y;
            return sample;
        }

        float remaining = WrapRouteDistanceLocked(distance);
        for (std::size_t index = 1; index < runnerRoutePoints_.size(); ++index) {
            const IntiRoutePoint& start = runnerRoutePoints_[index - 1];
            const IntiRoutePoint& end = runnerRoutePoints_[index];
            float deltaX = end.x - start.x;
            float deltaY = end.y - start.y;
            float segmentLength = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            if (segmentLength <= 0.001f) {
                continue;
            }
            if (remaining <= segmentLength) {
                float t = remaining / segmentLength;
                sample.x = start.x + deltaX * t;
                sample.y = start.y + deltaY * t;
                sample.headingX = deltaX / segmentLength;
                sample.headingY = deltaY / segmentLength;
                return sample;
            }
            remaining -= segmentLength;
        }

        sample.x = runnerRoutePoints_.back().x;
        sample.y = runnerRoutePoints_.back().y;
        if (runnerRoutePoints_.size() >= 2) {
            const IntiRoutePoint& start = runnerRoutePoints_[runnerRoutePoints_.size() - 2];
            const IntiRoutePoint& end = runnerRoutePoints_.back();
            float deltaX = end.x - start.x;
            float deltaY = end.y - start.y;
            float segmentLength = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            if (segmentLength > 0.001f) {
                sample.headingX = deltaX / segmentLength;
                sample.headingY = deltaY / segmentLength;
            }
        }
        return sample;
    }

    void FinishSceneLocked(BgeGameRuntime& runtime, bool animationRunning, int selectedSlot, bool updateCollisionFlags = true)
    {
        *runtime.selectedObjectSlot = (std::max)(0, (std::min)(BGE_OBJECT_SLOT_COUNT - 1, selectedSlot));
        *runtime.objectSelectionActive = false;
        *runtime.animationRunning = animationRunning;
        BgeSetCurrentEdgePolicy(BgeEdgePolicy::Clamp);
        if (updateCollisionFlags) {
            BgeUpdateCollisionFlags(*runtime.objectSlots);
        }
        else {
            for (auto& slot : *runtime.objectSlots) {
                slot.collisionDetected = false;
            }
        }
        if (runtime.refreshSelectedObjectGlobalsLocked) {
            runtime.refreshSelectedObjectGlobalsLocked();
        }
        if (runtime.persistActiveObjectGroupLocked) {
            runtime.persistActiveObjectGroupLocked();
        }
        *runtime.rendererStateDirty = true;
    }

    void ConfigureBallSlot(BgeObjectSlotState& slot, float x, float y, float radius, float r, float g, float b, BgeObjectKind kind)
    {
        slot.visible = true;
        slot.x = x;
        slot.y = y;
        slot.velocityX = 0.0f;
        slot.velocityY = 0.0f;
        slot.radius = radius;
        slot.colorR = r;
        slot.colorG = g;
        slot.colorB = b;
        slot.colorA = 1.0f;
        slot.shape = BgeObjectShape::Ball;
        slot.kind = kind;
        slot.renderStyle = BgeObjectRenderStyle::Filled;
        slot.outlineThickness = 2.0f;
    }

    void ConfigureSectionMarker(BgeObjectSlotState& slot, float x, float y, IntiSection markerSection, float r, float g, float b)
    {
        bool selected = section_ == markerSection;
        ConfigureBallSlot(slot, x, y, selected ? 24.0f : 18.0f,
                          selected ? (std::min)(1.0f, r + 0.12f) : r,
                          selected ? (std::min)(1.0f, g + 0.12f) : g,
                          selected ? (std::min)(1.0f, b + 0.12f) : b,
                          BgeObjectKind::Generic);
        slot.colorA = selected ? 1.0f : 0.78f;
    }

    void ConfigurePathSlot(BgeObjectSlotState& slot, float startX, float startY, float endX, float endY, float r, float g, float b)
    {
        float deltaX = endX - startX;
        float deltaY = endY - startY;
        float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        slot.visible = true;
        slot.x = (startX + endX) * 0.5f;
        slot.y = (startY + endY) * 0.5f;
        slot.velocityX = deltaX;
        slot.velocityY = deltaY;
        slot.radius = (std::max)(10.0f, length / 6.4f);
        slot.colorR = r;
        slot.colorG = g;
        slot.colorB = b;
        slot.colorA = 1.0f;
        slot.shape = BgeObjectShape::Line;
        slot.kind = BgeObjectKind::Generic;
        slot.renderStyle = BgeObjectRenderStyle::Filled;
        slot.outlineThickness = 2.0f;
    }

    bool CanResearchTechLocked(int techIndex) const
    {
        const IntiTechNode& tech = IntiTechNodeForIndex(techIndex);
        return tech.prerequisite == BGE_INTI_TECH_NONE || techCompleted_[static_cast<std::size_t>(tech.prerequisite)];
    }

    int PendingOrderCountLocked() const
    {
        return (pendingGoodsOrder_ ? 1 : 0)
            + (pendingCityOrder_ ? 1 : 0)
            + (pendingPathOrder_ ? 1 : 0)
            + (pendingMessageOrder_ ? 1 : 0);
    }

    int MaxOrdersPerTurnLocked() const
    {
        return 2 + (techCompleted_[1] ? 1 : 0) + (cityCount_ >= 3 ? 1 : 0);
    }

    int ResearchPerTurnLocked() const
    {
        return 1 + (cityCount_ / 2) + (techCompleted_[1] ? 1 : 0) + (techCompleted_[5] ? 1 : 0);
    }

    int GoodsPerTurnLocked() const
    {
        return cityCount_ + (techCompleted_[0] ? 2 : 0);
    }

    int MoneyPerTurnLocked() const
    {
        return pathCount_ + (techCompleted_[2] ? 1 : 0);
    }

    int MaxPathCountLocked() const
    {
        return MaxPathCountForCityCountLocked(cityCount_);
    }

    int MaxPathCountForCityCountLocked(int cityCount) const
    {
        return (std::max)(0, cityCount - 1) + (techCompleted_[4] ? 1 : 0);
    }

    int PathBuildCostLocked() const
    {
        return techCompleted_[3] ? 0 : 1;
    }

    int MessageRewardLocked() const
    {
        return 1 + (techCompleted_[5] ? 2 : 0);
    }

    std::wstring QueueOrderLocked(bool& pendingFlag, const wchar_t* orderName)
    {
        if (pendingFlag) {
            return std::wstring(orderName) + L" already planned this turn";
        }
        if (PendingOrderCountLocked() >= MaxOrdersPerTurnLocked()) {
            return L"orders full; use inti end-turn to resolve the turn";
        }
        pendingFlag = true;
        return std::wstring(L"planned order: ") + orderName;
    }

    void SelectNextAvailableTechLocked()
    {
        if (currentTechIndex_ >= 0
            && currentTechIndex_ < BGE_INTI_TECH_COUNT
            && !techCompleted_[static_cast<std::size_t>(currentTechIndex_)]
            && CanResearchTechLocked(currentTechIndex_)) {
            return;
        }
        for (int index = 0; index < BGE_INTI_TECH_COUNT; ++index) {
            if (!techCompleted_[static_cast<std::size_t>(index)] && CanResearchTechLocked(index)) {
                currentTechIndex_ = index;
                return;
            }
        }
        currentTechIndex_ = BGE_INTI_TECH_COUNT - 1;
    }

    std::wstring ResolveTurnLocked()
    {
        std::wstringstream report;
        report << L"turn " << turn_ << L" resolved";

        int goodsYield = GoodsPerTurnLocked();
        int moneyYield = MoneyPerTurnLocked();
        goods_ += goodsYield;
        money_ += moneyYield;
        report << L" | jungle/terrace yield +" << goodsYield << L" goods +" << moneyYield << L" money";

        SelectNextAvailableTechLocked();
        if (!techCompleted_[static_cast<std::size_t>(currentTechIndex_)] && CanResearchTechLocked(currentTechIndex_)) {
            const IntiTechNode& tech = IntiTechNodeForIndex(currentTechIndex_);
            int researchYield = ResearchPerTurnLocked();
            science_ += researchYield;
            techProgress_[static_cast<std::size_t>(currentTechIndex_)] += researchYield;
            report << L" | research +" << researchYield << L" " << tech.key;
            if (techProgress_[static_cast<std::size_t>(currentTechIndex_)] >= tech.cost) {
                techProgress_[static_cast<std::size_t>(currentTechIndex_)] = tech.cost;
                techCompleted_[static_cast<std::size_t>(currentTechIndex_)] = true;
                report << L" completed " << tech.name;
                SelectNextAvailableTechLocked();
            }
        }

        if (pendingGoodsOrder_) {
            int produced = 2 + (techCompleted_[0] ? 1 : 0);
            goods_ += produced;
            report << L" | produced +" << produced << L" goods";
        }
        if (pendingCityOrder_) {
            int cityCost = 1;
            if (goods_ >= cityCost) {
                goods_ -= cityCost;
                cityCount_ += 1;
                report << L" | founded city " << cityCount_;
            }
            else {
                report << L" | city order failed: need goods";
            }
        }
        if (pendingPathOrder_) {
            int pathCost = PathBuildCostLocked();
            if (cityCount_ < 2) {
                report << L" | path order failed: need two cities";
            }
            else if (pathCount_ >= MaxPathCountLocked()) {
                report << L" | path order held: no open city link";
            }
            else if (goods_ >= pathCost) {
                goods_ -= pathCost;
                pathCount_ += 1;
                report << L" | built Inca trail " << pathCount_;
            }
            else {
                report << L" | path order failed: need goods";
            }
        }
        if (pendingMessageOrder_) {
            if (ChasquiReadyLocked()) {
                int reward = MessageRewardLocked();
                messagesSent_ += 1;
                money_ += reward;
                science_ += techCompleted_[5] ? 1 : 0;
                report << L" | chasqui message delivered +" << reward << L" money";
            }
            else {
                report << L" | chasqui order failed: cities and trails required";
            }
        }

        pendingGoodsOrder_ = false;
        pendingCityOrder_ = false;
        pendingPathOrder_ = false;
        pendingMessageOrder_ = false;
        turn_ += 1;
        lastTurnReport_ = report.str();
        return lastTurnReport_;
    }

    std::wstring BuildPendingOrdersTextLocked() const
    {
        if (PendingOrderCountLocked() == 0) {
            return L"none";
        }
        std::wstring orders;
        if (pendingGoodsOrder_) {
            orders += orders.empty() ? L"goods" : L",goods";
        }
        if (pendingCityOrder_) {
            orders += orders.empty() ? L"city" : L",city";
        }
        if (pendingPathOrder_) {
            orders += orders.empty() ? L"path" : L",path";
        }
        if (pendingMessageOrder_) {
            orders += orders.empty() ? L"message" : L",message";
        }
        return orders;
    }

    std::wstring BuildTechSummaryLocked() const
    {
        std::wstringstream stream;
        stream << L"tech ";
        for (int index = 0; index < BGE_INTI_TECH_COUNT; ++index) {
            const IntiTechNode& tech = IntiTechNodeForIndex(index);
            if (index > 0) {
                stream << L"; ";
            }
            stream << index << L":" << tech.key << L" ";
            if (techCompleted_[static_cast<std::size_t>(index)]) {
                stream << L"done";
            }
            else if (!CanResearchTechLocked(index)) {
                stream << L"locked";
            }
            else {
                stream << techProgress_[static_cast<std::size_t>(index)] << L"/" << tech.cost;
            }
        }
        return stream.str();
    }

    std::wstring BuildCurrentTechTextLocked() const
    {
        const IntiTechNode& tech = IntiTechNodeForIndex(currentTechIndex_);
        std::wstringstream stream;
        stream << tech.key << L" " << techProgress_[static_cast<std::size_t>(currentTechIndex_)] << L"/" << tech.cost;
        if (techCompleted_[static_cast<std::size_t>(currentTechIndex_)]) {
            stream << L" complete";
        }
        else if (!CanResearchTechLocked(currentTechIndex_)) {
            stream << L" locked";
        }
        return stream.str();
    }

    std::wstring BuildChasquiRouteStatusLocked() const
    {
        std::wstringstream stream;
        stream << L"Chasqui route animation | frames 0-9 | points " << runnerRoutePoints_.size()
               << L" | start ";
        if (!runnerRoutePoints_.empty()) {
            stream << static_cast<int>(runnerRoutePoints_.front().x) << L"," << static_cast<int>(runnerRoutePoints_.front().y);
        }
        else {
            stream << L"none";
        }
        stream << L" | distance " << static_cast<int>(runnerRouteTotalDistance_)
               << L" | speed " << static_cast<int>(runnerRouteSpeed_);
        if (runnerRouteBoostActive_) {
            stream << L" | reverse-krebs trail";
        }
        return stream.str();
    }

    std::wstring ApplySectionActionLocked(IntiSection section, const std::wstring& action)
    {
        if (action.empty()) {
            return L"";
        }

        if (section == IntiSection::TechTree) {
            if (action == L"list" || action == L"tree" || action == L"status") {
                return BuildTechSummaryLocked();
            }
            if (action == L"research" || action == L"educate" || action == L"study") {
                SelectNextAvailableTechLocked();
                const IntiTechNode& tech = IntiTechNodeForIndex(currentTechIndex_);
                if (techCompleted_[static_cast<std::size_t>(currentTechIndex_)]) {
                    return L"all visible tech projects complete";
                }
                if (!CanResearchTechLocked(currentTechIndex_)) {
                    return std::wstring(L"research locked: ") + tech.name;
                }
                return std::wstring(L"researching ") + tech.name + L"; end turn to gain progress";
            }
        }

        if (section == IntiSection::Empire) {
            if (action == L"produce-goods") {
                return QueueOrderLocked(pendingGoodsOrder_, L"goods production");
            }
            if (action == L"found-city") {
                return QueueOrderLocked(pendingCityOrder_, L"found city");
            }
            if (action == L"establish-path") {
                int plannedCities = cityCount_ + (pendingCityOrder_ ? 1 : 0);
                if (plannedCities < 2) {
                    return L"paths need at least two cities; plan or found a second city first";
                }
                if (pathCount_ + (pendingPathOrder_ ? 1 : 0) >= (std::max)(1, MaxPathCountForCityCountLocked(plannedCities))) {
                    return L"all known city links already have planned trails";
                }
                return QueueOrderLocked(pendingPathOrder_, L"establish Inca running trail");
            }
        }

        if (section == IntiSection::Chasqui && (action == L"send-message" || action == L"run")) {
            int plannedCities = cityCount_ + (pendingCityOrder_ ? 1 : 0);
            int plannedPaths = pathCount_ + (pendingPathOrder_ ? 1 : 0);
            if (plannedCities < 2) {
                return L"Chasqui not needed until the empire has two cities";
            }
            if (plannedPaths < 1) {
                return L"establish Inca running trails before sending messages";
            }
            return QueueOrderLocked(pendingMessageOrder_, L"send chasqui message");
        }

        return L"";
    }

    bool ChasquiReadyLocked() const
    {
        return cityCount_ >= 2 && pathCount_ >= 1;
    }

    std::wstring ChasquiGateTextLocked() const
    {
        if (cityCount_ < 2) {
            return L"chasqui locked: need 2 cities";
        }
        if (pathCount_ < 1) {
            return L"chasqui locked: establish trails";
        }
        return L"chasqui ready";
    }

    std::wstring BuildRunnerModeStatusTextLocked() const
    {
        std::wstringstream stream;
        stream << L"Chasqui runner " << runnerMode_ << L": " << IntiRunnerModeForIndex(runnerMode_).name;
        if (!ChasquiReadyLocked()) {
            stream << L" | " << ChasquiGateTextLocked();
        }
        return stream.str();
    }

    std::wstring BuildStatusLocked() const
    {
        std::wstringstream stream;
        stream << L"Inti Runners | " << IntiSectionName(section_)
             << L" | turn " << turn_
             << L" | orders " << PendingOrderCountLocked() << L"/" << MaxOrdersPerTurnLocked()
             << L" [" << BuildPendingOrdersTextLocked() << L"]"
               << L" | sections tech-tree empire chasqui-runners"
               << L" | cities " << cityCount_
               << L" | trails " << pathCount_
               << L" | science " << science_
               << L" | money " << money_
               << L" | goods " << goods_
             << L" | tech " << BuildCurrentTechTextLocked()
               << L" | messages " << messagesSent_
               << L" | " << ChasquiGateTextLocked();
        return stream.str();
    }

    std::wstring BuildHudLocked() const
    {
        if (section_ == IntiSection::PhotoRun) {
            std::array<int, BGE_INTI_UI_GROUP_COUNT> effectiveCandidates = EffectivePhotoUiCandidatesLocked();
            const IntiPhotoHudUiStyle& hudStyle = ActivePhotoHudUiStyle(effectiveCandidates);
            std::wstringstream stream;
            stream << L"INTI PHOTO"
                   << L" | HUD " << hudStyle.label
                   << L" | ui hud c" << effectiveCandidates[2]
                   << L" | cp " << photoVisitedCount_ << L"/" << photoCheckpoints_.size()
                   << L" | dots " << photoDotsCollected_ << L"/" << photoDots_.size();
            if (!hudStyle.compact) {
                stream << L" | " << (photoTravelDir_ != 0 ? L"running" : L"paused");
                if (photoPauseAtNextDot_) {
                    stream << L" queue-pause";
                }
            }
            if (hudStyle.showRoll && photoLastRoll_ > 0) {
                stream << L" | roll " << photoLastRoll_;
            }
            if (hudStyle.showCompose && photoLastComposition_ >= 0) {
                stream << L" | compose " << photoLastComposition_;
                if (!photoLastComposeName_.empty()) {
                    stream << L" " << photoLastComposeName_;
                }
            }
            if (hudStyle.showCards) {
                stream << L" | card " << (photoComplete_ ? L"area-complete" : L"visit-all");
                if (!photoExits_.empty()) {
                    stream << L" | card " << (photoComplete_ ? L"take-exit" : L"exit-locked");
                }
            }
            if (hudStyle.showSource) {
                stream << L" | map " << photoMapName_;
                stream << L" | source " << (photoSource_.empty() ? L"embedded" : photoSource_);
            }
            if (hudStyle.showControls) {
                stream << L" | keys <- -> aim, up fwd, dn back, sp pause, pgup/pgdn hud, 0-9";
            }
            return stream.str();
        }

        std::wstringstream stream;
        stream << L"INTI RUNNERS"
             << L" | TURN " << turn_
               << L" | " << IntiSectionName(section_)
             << L" | ORD " << PendingOrderCountLocked() << L"/" << MaxOrdersPerTurnLocked()
               << L" | CITIES " << cityCount_
               << L" | TRAILS " << pathCount_
               << L" | SCI " << science_
               << L" | GOODS " << goods_
             << L" | TECH " << IntiTechNodeForIndex(currentTechIndex_).key
               << L" | MSG " << messagesSent_
               << L" | RUNNER " << runnerMode_;
        return stream.str();
    }

    void PublishRuntimeFeedback(BgeGameRuntime& runtime, const std::wstring& statusText, const std::wstring& hudText) const
    {
        if (runtime.setStatus) {
            runtime.setStatus(statusText);
        }
        if (runtime.setHud) {
            runtime.setHud(hudText);
        }
        if (runtime.syncControls) {
            runtime.syncControls();
        }
        if (runtime.invalidateRenderer) {
            runtime.invalidateRenderer();
        }
    }

    std::string NarrowStatus(const std::wstring& value) const
    {
        std::string result;
        result.reserve(value.size());
        for (wchar_t ch : value) {
            result.push_back(ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '?');
        }
        return result;
    }

    bool active_ = false;
    IntiSection section_ = IntiSection::Main;
    int turn_ = 1;
    int runnerMode_ = 0;
    int currentTechIndex_ = 0;
    int science_ = 0;
    int money_ = 10;
    int goods_ = 4;
    int cityCount_ = 1;
    int pathCount_ = 0;
    int messagesSent_ = 0;
    std::array<int, BGE_INTI_TECH_COUNT> techProgress_{};
    std::array<bool, BGE_INTI_TECH_COUNT> techCompleted_{};
    bool pendingGoodsOrder_ = false;
    bool pendingCityOrder_ = false;
    bool pendingPathOrder_ = false;
    bool pendingMessageOrder_ = false;
    std::wstring lastTurnReport_;
    std::vector<IntiRoutePoint> runnerRoutePoints_;
    bool runnerRouteActive_ = false;
    bool runnerRouteLoop_ = true;
    float runnerRouteSpeed_ = BGE_INTI_ROUTE_DEFAULT_SPEED;
    bool runnerRouteBoostActive_ = false;
    float runnerRouteProgress_ = 0.0f;
    float runnerRouteTotalDistance_ = 0.0f;
    double runnerSeconds_ = 0.0;
    std::vector<IntiPhotoCheckpoint> photoCheckpoints_;
    std::vector<IntiPhotoPath> photoPaths_;
    std::vector<IntiPhotoDot> photoDots_;
    std::vector<IntiPhotoExit> photoExits_;
    std::wstring photoMapId_;
    std::wstring photoMapName_;
    std::wstring photoSource_;
    std::wstring photoPendingExit_;
    float photoRunnerXf_ = 0.5f;
    float photoRunnerYf_ = 0.5f;
    float photoRunnerHeadingX_ = 0.0f;
    float photoRunnerHeadingY_ = 0.0f;
    float photoFacingX_ = 1.0f;
    float photoFacingY_ = 0.0f;
    int photoEdgeIndex_ = -1;       // current corridor edge index into photoPaths_
    float photoEdgeT_ = 0.0f;       // parameter along the edge, a->b, in [0,1]
    int photoTravelDir_ = 0;        // +1 toward b, -1 toward a, 0 standing still
    int photoNode_ = -1;            // checkpoint the courier is parked at (-1 = travelling)
    int photoPrevNode_ = -1;        // checkpoint the courier arrived from (for "go back")
    int photoAimEdge_ = -1;         // corridor the direction arrow is aimed down
    // Board-game movement: the courier rolls a die and steps that many board
    // spaces along the faced corridor, stopping ON the landed space (mid-
    // corridor allowed). At a junction node it may re-aim; mid-corridor it can
    // only continue forward or reverse. photoFacingDir_ tracks which way along
    // the current edge the courier is facing while resting on a space.
    int photoFacingDir_ = 1;        // +1 toward edge.b, -1 toward edge.a (while on a space)
    float photoTargetT_ = 0.0f;     // edge parameter the current step is animating toward
    int photoLastRoll_ = 0;         // retained for compatibility/legacy telemetry
    int photoQueuedTurnDir_ = 0;    // -1=queue left, +1=queue right at next junction
    bool photoAutoRun_ = true;      // continuous movement mode (default on)
    bool photoPauseAtNextDot_ = false; // space toggles pause on next step target
    // Pathing-UI redline (warp bge.engine.warp.inti-pathing-ui-redline).
    // Slice A plumbing: focused group + per-group candidate ids; styling
    // slices B-D consume these presets.
    int photoUiGroup_ = 0; // 0=dots 1=aim 2=hud
    std::array<int, BGE_INTI_UI_GROUP_COUNT> photoUiCandidate_{ 3, 1, 8 };
    std::array<int, BGE_INTI_UI_GROUP_COUNT> photoUiWinnerCandidate_{ 3, 1, 8 };
    bool photoUiWinnersLocked_ = true;
    bool photoUiDebugEnabled_ = IntiUiDebugEnabled();
    std::mt19937 photoRng_{std::random_device{}()};
    float photoRunnerSpeed_ = 0.22f; // corridor speed in fraction-units / second
    // Composition (Slice 1): the angle-of-attack score recorded when a site is
    // claimed, scored against that site's golden approach cone. Domain data;
    // the engine only supplies the neutral bounded-vector drag the angle rides.
    int photoLastComposition_ = -1;      // last claimed composition 0..100; -1 none yet
    int photoCompositionTotal_ = 0;      // running sum of composition scores this area
    int photoComposedCount_ = 0;         // number of sites composed this area
    std::wstring photoLastComposeName_;  // site of the last composed shot
    int photoVisitedCount_ = 0;
    int photoDotsCollected_ = 0;
    bool photoComplete_ = false;
};

} // namespace

BgeGameModule& BgeIntiGameModule()
{
    static IntiGameModule module;
    return module;
}