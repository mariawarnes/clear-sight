#pragma once
// A11y Control Panel - mod registration API (version 1).
//
// A mod that wants its settings in the accessibility menu listens for the
// panel's request message and answers it by calling back into the host. With
// SKSE/CommonLibSSE:
//
//   // in SKSEPluginLoad:
//   SKSE::GetMessagingInterface()->RegisterListener("A11yControlPanel",
//       [](SKSE::MessagingInterface::Message* m) {
//           if (m->type == A11Y_MSG_REQUEST && m->dataLen == sizeof(A11yHost)) {
//               auto* host = static_cast<A11yHost*>(m->data);
//               if (host->version == A11Y_API_VERSION) host->registerMod(&kMyInterface);
//           }
//       });
//
// The panel sends the request once, at kPostPostLoad, so every mod has had its
// SKSEPluginLoad run and can listen first. Nothing is required if the panel is
// not installed. All data in the interface must stay valid for the whole game.
//
// Setting ids are "<modId>.<name>", lower case, e.g. "clearsight.enabled".
// get/set may be called from the game's main or render thread; keep them cheap.
// Plain C types only, so any compiler can implement it.
#include <cstdint>

#define A11Y_API_VERSION 1u
#define A11Y_MSG_REQUEST 0x41313152u  // sent by the panel to listeners of "A11yControlPanel"

enum A11ySettingType : int32_t { A11Y_TOGGLE = 0, A11Y_SLIDER = 1 };

struct A11ySettingInfo {
    const char* id;           // "clearsight.enabled"
    const char* label;        // "Clear Sight"
    const char* description;  // one or two sentences, shown in the preview pane
    int32_t type;             // A11ySettingType
    double min, max, step;    // sliders only
    const char* previewKind;  // "", "contrast", "arcs", "text-size"
    int32_t restartRequired;  // 1: takes effect only after restarting the game
};

struct A11yModInterface {
    uint32_t version;       // A11Y_API_VERSION
    const char* modId;      // "clearsight"
    const char* modName;    // "Clear Sight"
    const char* group;      // heading in the menu, e.g. "Vision"
    uint32_t settingCount;
    const A11ySettingInfo* settings;
    // Toggle: value is 0 or 1. Return 1 on success, 0 on failure.
    int32_t (*get)(const char* id, double* value);
    int32_t (*set)(const char* id, double value);
};

// Payload of A11Y_MSG_REQUEST.
struct A11yHost {
    uint32_t version;  // A11Y_API_VERSION
    // Returns 1 if accepted, 0 if rejected (bad version, duplicate id...).
    int32_t (*registerMod)(const A11yModInterface* mod);
};
