// potato_game — the windowed shell (Story 12.9): creation
// screen + overworld loop on real hardware. The only TU that
// pulls the full stack together: content registries →
// WorldState → RenderOverworld → GlShell blit + ImGui panel.
//
// Determinism note: the sim is unchanged — the shell only reads
// state and issues the same canonical calls (IssueMarch,
// ResolveBeats) a scripted caller would.

#include "Game/Shell/GlShell.h"
#include "Game/Render/OverworldView.h"
#include "Game/Render/SpriteAtlas.h"
#include "Game/Ui/CreationForm.h"
#include "Game/Ui/OrderMenu.h"
#include "Campaign/Characters/Character.h"
#include "Campaign/Characters/CommanderBind.h"
#include "Campaign/Ledger/Ledger.h"
#include "Campaign/Narrative/IntelLedger.h"
#include "Campaign/World/Encounter.h"
#include "Campaign/World/March.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"

#include <imgui.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

using namespace Potato;

// First loadable atlas in the dir — atlases are per-context
// content; the overworld ships one (assets/atlases/overworld).
Game::SpriteAtlas LoadAtlas(const std::filesystem::path& dir) {
    if (std::filesystem::exists(dir)) {
        for (const auto& e :
             std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() != ".json") continue;
            auto a = Game::SpriteAtlas::Load(e.path().string());
            if (a.ok()) return std::move(a.value);
        }
    }
    // Empty-but-valid fallback: every DrawTile misses → fallback
    // rects carry the frame.
    Game::SpriteAtlas a;
    return a;
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    Campaign::WorldLibrary worlds;
    const auto wl = Campaign::WorldLibrary::Load(
        "assets/worlds", worlds);
    if (!wl.ok || worlds.Size() == 0) {
        std::fprintf(stderr, "no world in assets/worlds\n");
        return 1;
    }
    const Campaign::WorldMap& map = worlds.Worlds().front();

    auto wsr = Campaign::WorldState::Init(map, 0x9d2c5680ull);
    if (!wsr.ok()) {
        std::fprintf(stderr, "world init: %s\n",
                     wsr.reason.c_str());
        return 1;
    }
    Campaign::WorldState ws = std::move(wsr.value);

    Campaign::CharacterLibrary chars;
    Campaign::CharacterLibrary::Load("assets/characters", chars);
    Campaign::EncounterLibrary encs;
    Campaign::EncounterLibrary::Load("assets/encounters", encs);
    Game::SpriteAtlas atlas = LoadAtlas("assets/atlases");

    Campaign::Ledger ledger;
    Campaign::IntelLedger intel;
    Campaign::Character commander;
    bool commanderPicked = false;

    Game::GlShell shell;
    if (!shell.Init(960, 600, "MingGoRTS — 民國史詩")) {
        std::fprintf(stderr, "shell init failed\n");
        return 1;
    }

    Game::OverworldInput viewIn;
    viewIn.encounters = &encs;
    int rosterSel = -1;
    char nameBuf[128] = "";
    char originBuf[128] = "";
    int priorSel = 0;
    long long seedIn = 0;

    while (!shell.ShouldClose()) {
        shell.NewFrame();

        if (!commanderPicked) {
            // --- creation screen: roster pick OR custom form ---
            ImGui::Begin("Commander");
            const auto roster = Game::RosterList(chars);
            if (ImGui::BeginCombo("Roster",
                                  rosterSel >= 0
                                      ? roster[rosterSel].name
                                             .c_str()
                                      : "— choose —")) {
                for (int i = 0;
                     i < static_cast<int>(roster.size()); ++i) {
                    if (ImGui::Selectable(
                            roster[i].name.c_str(),
                            rosterSel == i)) {
                        rosterSel = i;
                    }
                }
                ImGui::EndCombo();
            }
            if (rosterSel >= 0 &&
                ImGui::Button("Take command")) {
                const Campaign::Character* c =
                    chars.Find(roster[rosterSel].id);
                if (c != nullptr) {
                    commander = *c;
                    commanderPicked = true;
                }
            }
            ImGui::Separator();
            ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
            ImGui::InputText("Origin", originBuf,
                             sizeof(originBuf));
            static const char* kPriors[] = {
                "aggressive", "defensive", "cunning"};
            ImGui::Combo("Prior", &priorSel, kPriors, 3);
            ImGui::InputScalar("Deck seed",
                               ImGuiDataType_S64, &seedIn);
            if (ImGui::Button("Create commander")) {
                Game::CreationForm form;
                form.name = nameBuf;
                form.origin = originBuf;
                form.prior =
                    static_cast<Campaign::RivalPrior>(priorSel);
                form.deckSeed = static_cast<std::uint64_t>(seedIn);
                auto c = Game::BuildCharacter(form);
                if (c.ok()) {
                    commander = std::move(c.value);
                    commanderPicked = true;
                }
            }
            ImGui::End();
        } else {
            // --- overworld loop ---
            ImGui::Begin("Orders");
            ImGui::Text("Day %lld — %s",
                        static_cast<long long>(ws.Day()),
                        ws.WarbandAt().c_str());
            if (ImGui::Button("Advance day")) {
                ws.ResolveBeats(map, 1);
            }
            ImGui::Separator();
            for (const auto& o :
                 Game::MarchOptions(map, ws,
                                    /*supplyAvailable=*/100,
                                    Campaign::MarchRules{})) {
                if (!o.affordable) ImGui::BeginDisabled();
                if (ImGui::Button(o.name.c_str())) {
                    Campaign::IssueMarch(ws, map, ledger,
                                         o.nodeId);
                }
                if (!o.affordable) ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled("%lldd %lldc",
                                    static_cast<long long>(
                                        o.days),
                                    static_cast<long long>(
                                        o.supply));
            }
            if (ws.Marching()) {
                ImGui::Text("→ %s (eta day %lld)",
                            ws.MarchDest().c_str(),
                            static_cast<long long>(
                                ws.MarchEta()));
            }
            ImGui::Separator();
            ImGui::Text("Encounters:");
            for (const auto* e : Campaign::PendingEncounters(
                     ws, map, encs)) {
                ImGui::BulletText("%s @ %s", e->id.c_str(),
                                  e->node.c_str());
            }
            ImGui::End();
        }

        auto fb = Game::RenderOverworld(map, ws, atlas, viewIn);
        if (fb.ok()) shell.Present(fb.value);
        shell.Swap();
    }

    return 0;
}
