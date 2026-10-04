#include "Anchor.h"
#ifdef ENABLE_ANCHOR
#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <cstdio>

void AnchorMenu() {
    auto* anchor = Anchor::Instance;
    if (anchor == nullptr) {
        return;
    }
    ImGui::TextWrapped("%s", anchor->Status());
#ifdef DIPTYCH_GAME_MODULE
    ImGui::TextWrapped("The combined session owns this connection and progress admission.");
#else
    static char host[256], room[65], team[65], name[65];
    static int port;
    static bool initialized = false;
    if (!initialized) {
        std::snprintf(host, sizeof(host), "%s", CVarGetString("gRemote.Anchor.Host", "127.0.0.1"));
        std::snprintf(room, sizeof(room), "%s", CVarGetString("gRemote.Anchor.RoomId", ""));
        std::snprintf(team, sizeof(team), "%s", CVarGetString("gRemote.Anchor.TeamId", "default"));
        std::snprintf(name, sizeof(name), "%s", CVarGetString("gRemote.Anchor.Name", ""));
        port = CVarGetInteger("gRemote.Anchor.Port", 43383);
        initialized = true;
    }
    ImGui::TextWrapped("Requires an Anchor server supporting MM permanent progress. Items, rewards and cycle-local "
                       "state are not shared.");
    ImGui::BeginDisabled(anchor->isEnabled);
    ImGui::InputText("Host", host, sizeof(host));
    ImGui::InputInt("Port", &port);
    ImGui::InputText("Room", room, sizeof(room));
    ImGui::InputText("Team", team, sizeof(team));
    ImGui::InputText("Name", name, sizeof(name));
    ImGui::EndDisabled();
    if (anchor->isEnabled) {
        if (ImGui::Button("Disconnect")) {
            anchor->Disconnect();
        }
    } else {
        ImGui::BeginDisabled(host[0] == 0 || room[0] == 0 || team[0] == 0 || port < 1 || port > 65535);
        if (ImGui::Button("Connect")) {
            CVarSetString("gRemote.Anchor.Host", host);
            CVarSetInteger("gRemote.Anchor.Port", port);
            CVarSetString("gRemote.Anchor.RoomId", room);
            CVarSetString("gRemote.Anchor.TeamId", team);
            CVarSetString("gRemote.Anchor.Name", name);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            anchor->Connect();
        }
        ImGui::EndDisabled();
    }
    bool on = anchor->Sharing();
    ImGui::BeginDisabled(!anchor->CanSetSharing());
    if (ImGui::Checkbox("Sync permanent world progress", &on)) {
        anchor->SetSharing(on);
    }
    ImGui::EndDisabled();
#endif
}
#endif
