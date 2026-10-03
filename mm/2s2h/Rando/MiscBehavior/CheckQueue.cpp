#include "MiscBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"
#include "Traps.h"

extern "C" {
#include "variables.h"
#include <functions.h>
extern TexturePtr gItemIcons[131];
extern s16 D_801CFF94[250];
}

#ifdef DIPTYCH_GAME_MODULE
void Diptych_RememberForeignActor(Actor* actor, RandoCheckId randoCheckId);
RandoCheckId Diptych_ForeignActorCheck(Actor* actor);
// 0: queue it; 1: not claimed yet, try again; 2: the inbox gives it
int Diptych_ClaimCheck(RandoCheckId rc);
void Diptych_GiveCheck(RandoCheckId rc, RandoItemId item);
std::string Diptych_ForeignItemMessage(RandoCheckId rc);
bool Diptych_ForeignTrap(RandoCheckId check);
std::string Diptych_ForeignTrapMessage();
#endif

static bool queued = false;

// This function handles queuing up item gives that the player has been marked as eligible for. If you are looking for
// the behavior of the actual giving itself, the heavy lifting is done by the GameInteractor queue. This function is
// currently called every frame, and loops through the entire list of checks, this works for now but as the check list
// grows we should keep an eye on performance.
//
// Though it seems counter-intuitive, we currently only allow one thing from randommizer to be queued at a time,
// primarily because of the way an item can be converted may change as you queue up multiple items. This is actually
// fine for both the giving/drawing, as we can call ConvertItem() inside the Give/Draw lambda, but the message we
// pass to the queue is static and would need to be updated if we allowed multiple items to be queued at once.
void Rando::MiscBehavior::CheckQueue() {
    if (queued) {
        return;
    }

    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        auto randoSaveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        if (randoSaveCheck.eligible) {
#ifdef DIPTYCH_GAME_MODULE
            const int diptychClaim = Diptych_ClaimCheck(randoCheckId);
            if (diptychClaim == 2) {
                RANDO_SAVE_CHECKS[randoCheckId].cycleObtained = true;
                RANDO_SAVE_CHECKS[randoCheckId].obtained = true;
                RANDO_SAVE_CHECKS[randoCheckId].eligible = false;
            }
            if (diptychClaim != 0) {
                continue;
            }
#endif
            queued = true;

            GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
                .showGetItemCutscene =
                    Rando::StaticData::ShouldShowGetItemCutscene(ConvertItem(randoSaveCheck.randoItemId, randoCheckId)),
                .param = (int16_t)randoCheckId,
                .giveItem =
                    [](Actor* actor, PlayState* play) {
                        auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                        RandoItemId randoItemId =
                            Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM);
                        std::string prefix = "You found";
                        std::string message =
                            Rando::StaticData::GetItemName(randoItemId, true, (RandoCheckId)CUSTOM_ITEM_PARAM);

                        if (randoItemId == RI_JUNK) {
                            randoItemId = Rando::CurrentJunkItem((RandoCheckId)CUSTOM_ITEM_PARAM);
                        }
                        if (randoItemId == RI_TRIFORCE_PIECE) {
                            if (gSaveContext.save.shipSaveInfo.rando.foundTriforcePieces + 1 >=
                                RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED]) {
                                prefix = "You";
                                message = "completed the Triforce";
                            }
                            randoItemId = RI_TRIFORCE_PIECE_PREVIOUS;
                        }

#ifdef DIPTYCH_GAME_MODULE
                        const bool foreignTrap = randoItemId == RI_DIPTYCH_FOREIGN &&
                                                 Diptych_ForeignTrap((RandoCheckId)CUSTOM_ITEM_PARAM);
                        if (foreignTrap) {
                            prefix = "";
                            message = Diptych_ForeignTrapMessage();
                            if (CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0) >= 2) {
                                message = CustomMessage::RemoveColorCodes(message);
                            }
                        }
#endif
                        if (randoItemId == RI_TRAP) {
                            prefix = "";
                            message = GetTrapMessage();
                            // We need to remove the Color Codes if the player is skipping Item Get Cutscenes as the
                            // Notification Emit doesnt support it.
                            if (CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0) >= 2) {
                                message = CustomMessage::RemoveColorCodes(message);
                            }
                        }

#ifdef DIPTYCH_GAME_MODULE
                        const std::string boxMessage = randoItemId == RI_DIPTYCH_FOREIGN && !foreignTrap
                                                           ? Diptych_ForeignItemMessage((RandoCheckId)CUSTOM_ITEM_PARAM)
                                                           : message;
                        const bool trapBox = randoItemId == RI_TRAP || foreignTrap;
#else
                        const std::string& boxMessage = message;
                        const bool trapBox = randoItemId == RI_TRAP;
#endif
                        CustomMessage::Entry entry = {
                            .textboxType = 2,
                            .icon = Rando::StaticData::GetIconForZMessage(trapBox ? RI_TRAP : randoItemId),
                            .msg = (prefix == "" ? "" : prefix + " ") + boxMessage + (trapBox ? "" : "!"),
                        };

                        if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                            CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                        } else if (Rando::StaticData::ShouldShowGetItemCutscene(randoItemId)) {
                            CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
                        } else {
                            if (Rando::StaticData::Items[randoItemId].randoItemType != RITYPE_JUNK) {
                                Notification::Emit({
                                    .itemIcon = Rando::StaticData::GetIconTexturePath(randoItemId),
                                    .message = prefix,
                                    .suffix = message,
                                });
                            }
                        }
#ifdef DIPTYCH_GAME_MODULE
                        Diptych_GiveCheck((RandoCheckId)CUSTOM_ITEM_PARAM, randoItemId);
#else
                        Rando::GiveItem(randoItemId);
#endif
                        randoSaveCheck.cycleObtained = true;
                        randoSaveCheck.obtained = true;
                        randoSaveCheck.eligible = false;
                        queued = false;
#ifdef DIPTYCH_GAME_MODULE
                        if (randoItemId == RI_DIPTYCH_FOREIGN) {
                            Diptych_RememberForeignActor(actor, (RandoCheckId)CUSTOM_ITEM_PARAM);
                        }
#endif
                        CUSTOM_ITEM_PARAM = randoItemId;
                    },
                .drawItem =
                    [](Actor* actor, PlayState* play) {
                        RandoItemId randoItemId;

                        // If the item has been given, the CUSTOM_ITEM_PARAM is set to the RI, prior to that it's the RC
                        if (CUSTOM_ITEM_FLAGS & CustomItem::CALLED_ACTION) {
                            if ((RandoItemId)CUSTOM_ITEM_PARAM == RI_TRAP) {
                                randoItemId = RI_MAX_TRAP;
                            } else {
                                randoItemId = (RandoItemId)CUSTOM_ITEM_PARAM;
                            }
                        } else {
                            auto& randoSaveCheck = RANDO_SAVE_CHECKS[CUSTOM_ITEM_PARAM];
                            randoItemId =
                                Rando::ConvertItem(randoSaveCheck.randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM);
                        }

                        Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
#ifdef DIPTYCH_GAME_MODULE
                        if ((CUSTOM_ITEM_FLAGS & CustomItem::CALLED_ACTION) && randoItemId == RI_DIPTYCH_FOREIGN) {
                            Rando::DrawItem(randoItemId, Diptych_ForeignActorCheck(actor), actor);
                            return;
                        }
#endif
                        Rando::DrawItem(randoItemId, (RandoCheckId)CUSTOM_ITEM_PARAM, actor);
                    } });
            return;
        }
    }
}

void Rando::MiscBehavior::CheckQueueReset() {
    queued = false;
    GameInteractor::Instance->currentEvent = GIEventNone{};
    GameInteractor::Instance->events.clear();
}
