#include <RE/Skyrim.h>
#include <RE/R/RendererShadowState.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include "Classification.h"
#include "DoorTarget.h"
#include "LoadWarmup.h"
#include "TextureCache.h"
#include "FaceTreatment.h"
#include "GreyscaleShader.h"
#include "Palette.h"
#include "LightColourScope.h"
#include "A11yApi.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace HC {
namespace {
using namespace std::chrono_literals;
using Feature = RE::BSShaderMaterial::Feature;
constexpr auto iniPath = L"Data\\SKSE\\Plugins\\ClearSight.ini";
struct Settings {
    std::array<std::array<float, 3>, 5> colours{{
        defaultWorld, {0.40f,0.43f,0.45f}, defaultAlly,
        defaultEnemy, defaultImportant}};
    float worldDetail = 0.12f, actorDetail = 0.20f, faceDetail = 0.35f;
    float highlightScale = 0.15f;
    bool nonHostileBlue = true;
    UINT key = 'H'; bool ctrl = true, shift = true;
} config;
struct Snapshot {
    std::unordered_map<RE::FormID, Category> categories;
    bool worldReady = false;
};
std::atomic<std::shared_ptr<const Snapshot>> snapshot{std::make_shared<Snapshot>()};
std::atomic<bool> enabled = false, loading = true, clearCache = false, pendingTask = false;
std::atomic<bool> loadCompleted = false;
std::atomic<std::uint64_t> loadGeneration = 0;
std::atomic<bool> menuBlocked = true;
std::mutex menuMutex;
std::unordered_set<std::string> openMenus;
TextureCache textures;
GreyscaleShaders effectShaders;
GreyscaleShaders sceneryShaders;
bool rendererInitialized = false, rendererFailed = false;
std::uint64_t lastSnapshot = 0;

float ReadFloat(const wchar_t* key, float fallback)
{
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"General", key, L"", buffer, 64, iniPath);
    if (!buffer[0]) return fallback;
    try { auto value = std::stof(buffer); return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback; }
    catch (...) { return fallback; }
}
void ReadConfig()
{
    enabled = GetPrivateProfileIntW(L"General", L"Enabled", 1, iniPath) != 0;
    config.nonHostileBlue = GetPrivateProfileIntW(L"General", L"NonHostileActorsBlue", 1, iniPath) != 0;
    config.highlightScale = ReadFloat(L"HighlightScale", config.highlightScale);
    const auto shadowRange = std::max(0.001f, ReadFloat(L"SceneryShadowRange", 0.06f));
    const auto shadowFloor = std::min(ReadFloat(L"SceneryShadowLift", 0.035f), shadowRange * 0.95f);
    sceneryShaders.ConfigureShadows(shadowFloor, shadowRange);
    spdlog::info("Scenery shadow floor={}, range={}, curve exponent=0.25", shadowFloor, shadowRange);
    config.key = std::clamp(GetPrivateProfileIntW(L"General", L"ToggleKey", 'H', iniPath), 1u, 254u);
    config.ctrl = GetPrivateProfileIntW(L"General", L"RequireCtrl", 1, iniPath) != 0;
    config.shift = GetPrivateProfileIntW(L"General", L"RequireShift", 1, iniPath) != 0;
    config.worldDetail = ReadFloat(L"WorldDetail", config.worldDetail);
    config.actorDetail = ReadFloat(L"ActorDetail", config.actorDetail);
    config.faceDetail = ReadFloat(L"FaceDetail", config.faceDetail);
    textures.maxDimension = std::clamp(GetPrivateProfileIntW(L"General", L"MaxTextureDimension", 256, iniPath), 64u, 2048u);
    textures.maxBytes = std::clamp(GetPrivateProfileIntW(L"General", L"CacheMegabytes", 256, iniPath), 32u, 1024u) * 1024ull * 1024;
    const wchar_t* names[]{L"World", L"Neutral", L"Ally", L"Enemy", L"Important"};
    for (std::size_t i = 0; i < 5; ++i) {
        wchar_t buffer[128]{};
        GetPrivateProfileStringW(L"Palette", names[i], L"", buffer, 128, iniPath);
        if (auto colour = ParseColour(buffer)) config.colours[i] = *colour;
        else if (buffer[0]) spdlog::warn("Invalid Palette entry {}; using its default colour", i);
    }
}

bool IsOverlay(std::string_view name)
{
    // These contain world dialogue/HUD rather than inventory/model previews.
    return name == "HUD Menu" || name == "Cursor Menu" || name == "Dialogue Menu" ||
        name == "Fader Menu" || name == "Subtitle Menu";
}
class MenuEvents : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (event) {
            bool block = !IsOverlay(event->menuName.c_str());
            if (event->opening) {
                if (auto menu = RE::UI::GetSingleton()->GetMenu(event->menuName.c_str())) {
                    if (menu->AlwaysOpen() && !menu->menuFlags.any(RE::UI_MENU_FLAGS::kInventoryItemMenu,
                        RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kFreezeFrameBackground)) block = false;
                }
            }
            std::lock_guard lock(menuMutex);
            if (event->opening && block) openMenus.insert(event->menuName.c_str());
            else openMenus.erase(event->menuName.c_str());
            menuBlocked = !openMenus.empty();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} menuEvents;

void UpdateMainThread()
{
    // Keep the worker from scheduling another task until this one finishes.
    struct PendingGuard { ~PendingGuard() { pendingTask = false; } } pendingGuard;
    const auto generation = loadGeneration.load();
    const auto now = GetTickCount64();
    static LoadWarmup warmup;
    auto* ui = RE::UI::GetSingleton();
    const bool menusReady = loadCompleted && ui && !ui->GameIsPaused() &&
        !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
    // Do not touch player/quest/world data while the load is still in progress.
    auto* player = menusReady ? RE::PlayerCharacter::GetSingleton() : nullptr;
    const bool worldReady = player && player->GetParentCell() &&
        player->GetParentCell()->IsAttached() && player->Get3D();
    if (!warmup.Poll(generation, menusReady && worldReady, now)) {
        snapshot.store(std::make_shared<Snapshot>());
        return;
    }
    if (generation != loadGeneration.load() || !loadCompleted) return;
    if (loading.exchange(false)) {
        lastSnapshot = 0;
        std::lock_guard lock(menuMutex);
        std::erase_if(openMenus, [&](const auto& menu) { return !ui->IsMenuOpen(menu); });
        menuBlocked = !openMenus.empty();
        spdlog::info("Load generation {}: world ready; resuming classification", generation);
    }
    DWORD foregroundPID = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPID);
    const bool pressed = foregroundPID == GetCurrentProcessId() &&
        (GetAsyncKeyState(config.key) & 0x8000) &&
        (!config.ctrl || (GetAsyncKeyState(VK_CONTROL) & 0x8000)) &&
        (!config.shift || (GetAsyncKeyState(VK_SHIFT) & 0x8000));
    static bool wasPressed = false;
    if (pressed && !wasPressed && !loading) {
        enabled = !enabled.load();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            if (auto* data = static_cast<RE::HUDData*>(queue->CreateUIMessageData("HUDData"))) {
                data->type = RE::HUD_MESSAGE_TYPE::kNotification;
                data->text = enabled ? "Clear Sight on" : "Clear Sight off";
                data->show = true;
                queue->AddMessage("HUD Menu", RE::UI_MESSAGE_TYPE::kUpdate, data);
            }
        }
        spdlog::info("Mode {}", enabled ? "on" : "off");
    }
    wasPressed = pressed;
    if (!enabled) return;
    if (now - lastSnapshot < 250) return;
    lastSnapshot = now;
    auto result = std::make_shared<Snapshot>();
    std::unordered_set<RE::FormID> important;
    std::unordered_set<RE::FormID> routeDoors;
    std::vector<RE::NiPointer<RE::TESObjectREFR>> objectiveMarkers;
    // The player's current target map already accounts for objective visibility.
    // Never retain raw target/path pointers beyond the target-map lock.
    // Resolve to handles while locked; keep references alive while inspecting.
    struct ActiveTarget {
        RE::FormID quest;
        std::uint32_t alias;
        RE::ObjectRefHandle resolved, tracking;
    };
    std::vector<ActiveTarget> activeTargets;
    {
        RE::BSSpinLockGuard lock(player->GetQuestTargetsLock());
        for (auto& [quest, targets] : player->GetQuestTargets()) {
            if (!quest || !quest->IsActive() || !targets) continue;
            for (auto* target : *targets) {
                if (!target) continue;
                ActiveTarget copy{quest->GetFormID(), target->alias};
                target->GetTargetRef(copy.resolved, false, quest);
                target->GetTrackingRef(copy.tracking, quest);
                activeTargets.push_back(copy);
            }
        }
    }
    static std::uint64_t lastTargetLog = 0;
    const bool logTargets = now - lastTargetLog > 5000;
    if (logTargets) lastTargetLog = now;
    for (const auto& target : activeTargets) {
        // Keep the direct objective and its tracking reference. Actor rules
        // still take precedence, so following a person never makes them yellow.
        for (const auto& handle : {target.resolved, target.tracking}) {
            if (auto ref = handle.get()) {
                const bool firstReference = important.insert(ref->GetFormID()).second;
                // Vanilla XMarker (0000003B): MQ101 uses these beside the
                // keep entrances, with no teleport path or linked door.
                if (firstReference && ref->GetBaseObject() && ref->GetBaseObject()->GetFormID() == 0x3B)
                    objectiveMarkers.push_back(ref);
                if (ref->GetBaseObject() && ref->GetBaseObject()->Is(RE::FormType::Door))
                    routeDoors.insert(ref->GetFormID());
            }
        }
        if (logTargets) {
            const auto r = target.resolved.get(), t = target.tracking.get();
            spdlog::info("Quest {:08X} alias {}: targetRef={:08X}, trackingRef={:08X}",
                target.quest, target.alias,
                r ? r->GetFormID() : 0, t ? t->GetFormID() : 0);
        }
    }
    if (auto* world = RE::TES::GetSingleton()) {
        std::vector<RE::NiPointer<RE::TESObjectREFR>> loadedDoors;
        world->ForEachReference([&](RE::TESObjectREFR* ref) {
            if (!ref || !ref->Is3DLoaded()) return RE::BSContainer::ForEachResult::kContinue;
            if (ref->GetBaseObject() && ref->GetBaseObject()->Is(RE::FormType::Door))
                loadedDoors.emplace_back(ref);
            auto* actor = ref->As<RE::Actor>();
            bool dead = actor && actor->IsDead();
            bool hostile = actor && actor != player && !dead && actor->IsHostileToActor(player);
            bool ally = actor && (actor == player || actor->IsPlayerTeammate());
            bool questObject = important.contains(ref->GetFormID()) || routeDoors.contains(ref->GetFormID()) ||
                (!actor && ref->HasQuestObject());
            const auto category = Classify(actor != nullptr, hostile, ally, questObject, config.nonHostileBlue, dead);
            result->categories.emplace(ref->GetFormID(), category);
            if (actor) {
                const auto previous = snapshot.load();
                const auto found = previous->categories.find(ref->GetFormID());
                if (found == previous->categories.end() || found->second != category)
                    spdlog::info("Actor {:08X} {}: category={}, hostile={}, teammate={}, dead={}",
                        ref->GetFormID(), ref->GetName(), static_cast<int>(category), hostile, ally, dead);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        for (const auto& marker : objectiveMarkers) {
            NearestQuestDoor nearest;
            auto* markerWorld = marker->GetWorldspace();
            for (const auto& door : loadedDoors) {
                const bool sameSpace = markerWorld ? markerWorld == door->GetWorldspace() :
                    marker->GetParentCell() && marker->GetParentCell() == door->GetParentCell();
                const auto delta = door->GetPosition() - marker->GetPosition();
                nearest.Consider(door->GetFormID(), delta.x * delta.x + delta.y * delta.y + delta.z * delta.z, sameSpace);
            }
            if (nearest.id) {
                routeDoors.insert(nearest.id);
                result->categories[nearest.id] = Category::Important;
                if (logTargets) spdlog::info("Quest marker {:08X} -> nearby door {:08X}", marker->GetFormID(), nearest.id);
            }
        }
        if (logTargets) for (const auto& door : loadedDoors) {
            if (routeDoors.contains(door->GetFormID()))
                spdlog::info("Highlighted door {:08X} {}", door->GetFormID(), door->GetName());
        }
        result->worldReady = true;
    }
    static std::uint64_t lastLog = 0;
    if (now - lastLog > 5000) {
        lastLog = now;
        std::array<std::size_t, 5> counts{};
        for (auto [id, category] : result->categories) ++counts[static_cast<std::size_t>(category)];
        spdlog::info("Loaded references: world={}, neutral={}, allies={}, enemies={}, important={}",
            counts[0], counts[1], counts[2], counts[3], counts[4]);
        spdlog::info("Active quest route doors: {}", routeDoors.size());
    }
    if (generation == loadGeneration.load() && loadCompleted && !loading)
        snapshot.store(result);
}

bool ShouldApply()
{
    return enabled && !loading && !menuBlocked && !rendererFailed && snapshot.load()->worldReady;
}

// Slots are restored at the end of every geometry draw. Both the D3D binding
// and Skyrim's deferred binding cache are restored to avoid contaminating UI.
struct Slot {
    UINT index;
    ComPtr<ID3D11ShaderResourceView> original, replacement;
};
struct Draw {
    RE::BSRenderPass* pass;
    std::vector<Slot> slots;
    RE::BSGraphics::PixelShader* shader = nullptr;
    ComPtr<ID3D11PixelShader> originalShader, greyShader;
};
thread_local std::vector<Draw> draws;

ID3D11DeviceContext* Context()
{
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    return renderer ? reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context) : nullptr;
}

// Capture bytecode when Skyrim creates its pixel shaders. Keeping it on the
// COM object avoids stale pointer keys and releases it with the original shader.
struct ShaderCapture {
    using Fn = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);
    static inline REL::Relocation<Fn> original;
    static HRESULT STDMETHODCALLTYPE Create(ID3D11Device* device, const void* code, SIZE_T size,
        ID3D11ClassLinkage* linkage, ID3D11PixelShader** output) {
        const auto hr = original(device, code, size, linkage, output);
        if (SUCCEEDED(hr) && output && *output && code && size <= UINT_MAX)
            (*output)->SetPrivateData(shaderBytecodeId, static_cast<UINT>(size), code);
        return hr;
    }
};
struct RendererInit {
    static inline REL::Relocation<void(*)()> original;
    static void Init() {
        original();
        if (auto* context = Context()) {
            ComPtr<ID3D11Device> device; context->GetDevice(&device);
            const auto* methods = *reinterpret_cast<const std::uintptr_t* const*>(device.Get());
            if (methods[15] == reinterpret_cast<std::uintptr_t>(&ShaderCapture::Create)) return;
            REL::Relocation<std::uintptr_t> table{*reinterpret_cast<std::uintptr_t*>(device.Get())};
            ShaderCapture::original = table.write_vfunc(15, ShaderCapture::Create);
            spdlog::info("Pixel shader capture installed for grey world effects");
        }
    }
    static void Install() {
        const auto call = REL::RelocationID(75595, 77226).address() + 0x2bc;
        if (*reinterpret_cast<const std::uint8_t*>(call) != 0xe8) {
            spdlog::error("Renderer init call differs; effect shader capture disabled"); return;
        }
        SKSE::AllocTrampoline(32);
        original = SKSE::GetTrampoline().write_call<5>(call, Init);
    }
};

void ApplyGreyscaleShader(Draw& draw, ID3D11DeviceContext* context, bool scenery = false)
{
    auto* shader = RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().currentPixelShader;
    if (!shader || !shader->shader) return;
    auto* original = reinterpret_cast<ID3D11PixelShader*>(shader->shader);
    ComPtr<ID3D11Device> device; context->GetDevice(&device);
    auto grey = (scenery ? sceneryShaders : effectShaders).Get(device.Get(), original);
    static std::unordered_set<ID3D11PixelShader*> reported;
    if (reported.insert(original).second)
        spdlog::info("World/effect shader {:X}: {}", shader->id, grey ? "grey output ready" : "conversion unavailable");
    if (!grey) return;
    draw.shader = shader; draw.originalShader = original; draw.greyShader = grey;
    shader->shader = reinterpret_cast<REX::W32::ID3D11PixelShader*>(grey.Get());
    context->PSSetShader(grey.Get(), nullptr, 0);
}

void ApplySlot(Draw& draw, ID3D11DeviceContext* context, UINT index, const Treatment& treatment)
{
    auto& state = RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData();
    auto* original = state.PSTexture[index];
    if (!original) return;
    if (auto replacement = textures.Get(context, original, treatment)) {
        draw.slots.push_back({index, original, replacement});
        state.PSTexture[index] = replacement.Get();
        state.PSResourceModifiedBits |= 1u << index;
        context->PSSetShaderResources(index, 1, replacement.GetAddressOf());
    }
}

template <int Kind> struct GeometryHook {
    using Fn = void(RE::BSShader*, RE::BSRenderPass*, std::uint32_t);
    static inline REL::Relocation<Fn*> setup;
    static inline REL::Relocation<Fn*> restore;

    static void Setup(RE::BSShader* shader, RE::BSRenderPass* pass, std::uint32_t flags)
    {
        {
            LightColourScope neutralLights;
            if (ShouldApply() && pass && pass->sceneLights) {
                for (std::size_t i = 0; i < pass->numLights; ++i) {
                    auto* light = pass->sceneLights[i];
                    if (!light || !light->pointLight || !light->light) continue;
                    auto& data = light->light->GetLightRuntimeData();
                    neutralLights.Add(data.diffuse.red, data.diffuse.green, data.diffuse.blue);
                    neutralLights.Add(data.ambient.red, data.ambient.green, data.ambient.blue);
                }
            }
            // Includes local lights from fire, torches, spells and enchanted
            // objects. Restore immediately after upload, before the actual draw.
            setup(shader, pass, flags);
        }
        draws.push_back({pass, {}});
        if (!ShouldApply() || !pass || !pass->geometry || rendererFailed) return;
        auto* context = Context();
        if (!context) return;
        if constexpr (Kind >= 3) {
            // Keep vanilla effect shape, animation, transparency and brightness;
            // convert its final RGB so vertex tints and hardcoded blood go grey too.
            ApplyGreyscaleShader(draws.back(), context);
            return;
        }
        if (!rendererInitialized) {
            ComPtr<ID3D11Device> device; context->GetDevice(&device);
            std::string error;
            if (!textures.Initialize(device.Get(), error)) {
                rendererFailed = true; spdlog::error("GPU initialization failed: {}", error); return;
            }
            rendererInitialized = true;
            spdlog::info("Diffuse renderer initialized");
        }
        if (clearCache.exchange(false)) textures.Clear();
        Category category = Category::World;
        if (auto* owner = pass->geometry->GetUserData()) {
            auto current = snapshot.load();
            const auto found = current->categories.find(owner->GetFormID());
            // Missing owner classification must leave the original object visible.
            if (found == current->categories.end()) return;
            category = found->second;
        }
        auto feature = pass->shaderProperty && pass->shaderProperty->material ?
            pass->shaderProperty->material->GetFeature() : Feature::kDefault;
        const bool face = feature == Feature::kFaceGen || feature == Feature::kFaceGenRGBTint || feature == Feature::kEye;
        float detail = category == Category::World ? config.worldDetail : config.actorDetail;
        if (face) detail = config.faceDetail;
        Treatment treatment{config.colours[static_cast<std::size_t>(category)], detail};
        auto& draw = draws.back();
        // Neutralize final scenery RGB after vertex tint, foliage backlighting,
        // fog and material shading. Shared shaders remain coloured for actors
        // and quest objects because replacement is scoped to this draw only.
        if (DesaturateScenery(category)) ApplyGreyscaleShader(draw, context, true);
        ApplySlot(draw, context, 0, treatment);
        if constexpr (Kind == 0) {
            if (feature == Feature::kMultiTexLand || feature == Feature::kMultiTexLandLODBlend) {
                for (UINT slot = 1; slot <= 6; ++slot) ApplySlot(draw, context, slot, treatment);
            } else if (feature == Feature::kFaceGen) {
                // Face tint/detail maps must not reintroduce source skin colours.
                // Facegen uses c*c + 2*t*c*(1-c), then roughly 4*detail.
                // Half-grey tint and quarter-grey detail preserve c; white does not.
                ApplySlot(draw, context, 3, faceTint);
                ApplySlot(draw, context, 4, faceDetailNeutral);
            } else if (feature == Feature::kGlowMap) {
                ApplySlot(draw, context, 6, Treatment{treatment.colour, 1});
            }
        }
    }
    static void Restore(RE::BSShader* shader, RE::BSRenderPass* pass, std::uint32_t flags)
    {
        if (!draws.empty() && draws.back().pass == pass) {
            auto& state = RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData();
            if (auto* context = Context()) {
                if (auto& draw = draws.back(); draw.shader) {
                    draw.shader->shader = reinterpret_cast<REX::W32::ID3D11PixelShader*>(draw.originalShader.Get());
                    context->PSSetShader(draw.originalShader.Get(), nullptr, 0);
                }
                for (auto& slot : draws.back().slots) {
                    state.PSTexture[slot.index] = slot.original.Get();
                    state.PSResourceModifiedBits |= 1u << slot.index;
                    context->PSSetShaderResources(slot.index, 1, slot.original.GetAddressOf());
                }
            }
            draws.pop_back();
        }
        restore(shader, pass, flags);
    }
    static void Install(REL::VariantID id)
    {
        REL::Relocation<std::uintptr_t> table{id};
        setup = table.write_vfunc(0x6, Setup);
        restore = table.write_vfunc(0x7, Restore);
    }
};

struct MaterialHook {
    using Fn = void(RE::BSShader*, const RE::BSShaderMaterial*);
    static inline REL::Relocation<Fn*> original;
    static void Setup(RE::BSShader* shader, const RE::BSShaderMaterial* material) {
        // Skyrim uploads these values in SetupMaterial; restore CPU data immediately.
        RE::NiColor* tint = nullptr;
        if (ShouldApply() && material) {
            if (material->GetFeature() == Feature::kFaceGenRGBTint)
                tint = &static_cast<RE::BSLightingShaderMaterialFacegenTint*>(const_cast<RE::BSShaderMaterial*>(material))->tintColor;
            else if (material->GetFeature() == Feature::kHairTint)
                tint = &static_cast<RE::BSLightingShaderMaterialHairTint*>(const_cast<RE::BSShaderMaterial*>(material))->tintColor;
        }
        RE::NiColor saved{};
        if (tint) {
            saved = *tint;
            *tint = material->GetFeature() == Feature::kFaceGenRGBTint ?
                RE::NiColor{0.5f,0.5f,0.5f} : RE::NiColor{1,1,1};
        }
        RE::BSLightingShaderMaterialBase* lighting = nullptr;
        float savedSpecular = 0;
        float* environment = nullptr;
        float savedEnvironment = 0;
        if (ShouldApply() && material && material->GetType() == RE::BSShaderMaterial::Type::kLighting) {
            lighting = static_cast<RE::BSLightingShaderMaterialBase*>(const_cast<RE::BSShaderMaterial*>(material));
            savedSpecular = lighting->specularColorScale;
            lighting->specularColorScale *= config.highlightScale;
            if (material->GetFeature() == Feature::kEnvironmentMap) {
                environment = &static_cast<RE::BSLightingShaderMaterialEnvmap*>(lighting)->envMapScale;
                savedEnvironment = *environment;
                *environment *= config.highlightScale;
            }
        }
        original(shader, material);
        if (lighting) lighting->specularColorScale = savedSpecular;
        if (environment) *environment = savedEnvironment;
        if (tint) *tint = saved;
    }
};

// Accessibility menu hookup (A11y Control Panel). Optional: nothing happens unless the
// panel is installed and sends its request.
const A11ySettingInfo a11ySettings[] = {
    {"clearsight.enabled", "High contrast",
     "Colours allies blue, enemies red and important objects yellow against muted scenery. Also toggled in game with Ctrl+Shift+H.",
     A11Y_TOGGLE, 0, 1, 1, "contrast", 0}};
int32_t A11yGet(const char* id, double* value)
{
    if (!id || !value || std::string_view(id) != "clearsight.enabled") return 0;
    *value = enabled.load() ? 1.0 : 0.0;
    return 1;
}
int32_t A11ySet(const char* id, double value)
{
    if (!id || std::string_view(id) != "clearsight.enabled") return 0;
    enabled = value != 0.0;
    spdlog::info("Mode {} (accessibility menu)", enabled ? "on" : "off");
    return 1;
}
const A11yModInterface a11yInterface{A11Y_API_VERSION, "clearsight", "Clear Sight", "Vision",
                                     static_cast<std::uint32_t>(std::size(a11ySettings)), a11ySettings, A11yGet, A11ySet};
void OnPanelMessage(SKSE::MessagingInterface::Message* message)
{
    if (message->type != A11Y_MSG_REQUEST || message->dataLen != sizeof(A11yHost)) return;
    auto* host = static_cast<A11yHost*>(message->data);
    if (host && host->version == A11Y_API_VERSION && host->registerMod) host->registerMod(&a11yInterface);
}

void OnMessage(SKSE::MessagingInterface::Message* message)
{
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        if (GetModuleHandleW(L"CommunityShaders.dll") || GetModuleHandleW(L"enbseries.dll") ||
            std::filesystem::exists("enbseries.ini")) {
            spdlog::error("This build targets vanilla rendering. Community Shaders/ENB detected; hooks not installed."); return;
        }
        GeometryHook<0>::Install(RE::VTABLE_BSLightingShader[0]);
        GeometryHook<1>::Install(RE::VTABLE_BSGrassShader[0]);
        GeometryHook<2>::Install(RE::VTABLE_BSDistantTreeShader[0]);
        GeometryHook<3>::Install(RE::VTABLE_BSEffectShader[0]);
        GeometryHook<4>::Install(RE::VTABLE_BSParticleShader[0]);
        GeometryHook<5>::Install(RE::VTABLE_BSBloodSplatterShader[0]);
        REL::Relocation<std::uintptr_t> lighting{RE::VTABLE_BSLightingShader[0]};
        MaterialHook::original = lighting.write_vfunc(0x4, MaterialHook::Setup);
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuEvents);
        // One pending task maximum. All game classification runs on the main thread.
        std::thread([] {
            for (;;) {
                if (!pendingTask.exchange(true)) SKSE::GetTaskInterface()->AddTask(UpdateMainThread);
                std::this_thread::sleep_for(25ms);
            }
        }).detach();
        spdlog::info("Native hooks installed; Ctrl+Shift+H toggles the mode");
    } else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        loading = true; loadCompleted = false; ++loadGeneration;
        clearCache = true; snapshot.store(std::make_shared<Snapshot>());
        spdlog::info("Load generation {}: suspended before save load", loadGeneration.load());
    } else if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        loading = true; loadCompleted = false; ++loadGeneration;
        clearCache = true; snapshot.store(std::make_shared<Snapshot>());
        // SKSE encodes success in the pointer value, not a pointer to bool.
        const bool success = message->type == SKSE::MessagingInterface::kNewGame || message->data != nullptr;
        loadCompleted = success;
        spdlog::info("Load generation {}: load success={}; waiting for world readiness", loadGeneration.load(), success);
    }
}
}
}

SKSE_PLUGIN_VERSION = [] {
    SKSE::PluginVersionData data;
    data.PluginVersion({0,3,13,0});
    data.PluginName("Clear Sight - High Contrast Accessibility");
    data.AuthorName("mlw05");
    data.UsesAddressLibrary(); data.UsesUpdatedStructs();
    data.CompatibleVersions({REL::Version{1,7,104,0}});
    return data;
}();

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != REL::Version{1,7,104,0}) return false;
    SKSE::Init(skse);
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "ClearSight.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto logger = std::make_shared<spdlog::logger>("ClearSight", sink);
    spdlog::set_default_logger(logger); spdlog::flush_on(spdlog::level::info);
    HC::ReadConfig();
    spdlog::info("Clear Sight - High Contrast Accessibility 0.3.13; runtime {}", skse->RuntimeVersion().string());
    if (!std::filesystem::exists("enbseries.ini")) HC::RendererInit::Install();
    SKSE::GetMessagingInterface()->RegisterListener("A11yControlPanel", HC::OnPanelMessage);
    return SKSE::GetMessagingInterface()->RegisterListener(HC::OnMessage);
}
