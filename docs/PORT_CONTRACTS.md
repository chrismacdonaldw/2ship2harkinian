# 2ship2harkinian maintained-layer contracts

Native MM rules, save decoding, randomizer checks and draw hooks belong here. The target is an independently useful native port with supported optional embedding; Diptych retains cross-game identity/admission and journals. The following are current source contracts; broader progress replication and an accepted-draw wrapper API remain unfinished.

## Separate permanent, cycle and player-local state

[`CheckQueue`](../mm/2s2h/Rando/MiscBehavior/CheckQueue.cpp) grants through native item semantics and updates `obtained`, `cycleObtained` and `eligible`. [`OnCycleSave`](../mm/2s2h/Rando/MiscBehavior/OnCycleSave.cpp) resets cycle check/scene state while preserving selected native persistent state. Do not replace this with a blanket OR of all flags: some flags are reversible, some enable rewards, and some only describe this player or cycle.

[`Sram_ActivateOwl`](../mm/src/code/z_sram_NES.c) activates a warp; [`Rando::GiveItem`](../mm/2s2h/Rando/GiveItem.cpp) uses it for owl rewards. Awarded activation and a shuffled statue check's obtained state are separate invariants. `eventInf` and `bButtonStatus` are legitimate owl-resume/local control state, serialized by [`BenJsonConversions.hpp`](../mm/2s2h/BenJsonConversions.hpp). Excluding them from shared progress does not justify deleting them from native saves.

[`OwlAccess`](../mm/2s2h/Network/Anchor/OwlAccess.h) captures only the ten earned warp-access bits. Local `Sram_ActivateOwl` emits a change-only `FLAG_OWL_ACTIVATION` event and retains its preferred-destination behavior. Remote known access unions those bits without changing reserved bits, destinations, entrances or shuffled statue check/reward state; unknown and known-zero never erase earned access. Statue physical openness can still follow `cycleObtained` independently of access.

Standalone Anchor capability 4 retains owl access in the existing room/seed/team epoch and ordered snapshot barrier. Capability 3 still shares switches but cannot admit owl authority. Server retention is bounded in-memory state, not disk persistence across server restart; returning native saves can seed a fresh epoch. Native disk persistence remains normal saving. In a combined session, separate typed owl callbacks use the existing manager owl journal as sole durable authority, including OFF/offline restoration. A failed commit keeps a bounded pending mask for same-owner retry; ordinary park retains it, while final teardown reports and clears unadmitted facts rather than claiming they were persisted.

Current shared Diptych progress is a narrow projection, not whole MM save replication. Native award eligibility, cycles, controls, equipment and minigame state must not become shared merely because they are persisted. Normal play may change clock/location/bookkeeping; verify protected identity, inventory, receipts and randomizer fields separately rather than requiring whole-save byte equality.

## One decode kernel, two callers

[`SaveManager.cpp`](../mm/2s2h/SaveManager/SaveManager.cpp) owns `ReadSaveJson`, `MigrateSave`, `ReadAndMigrateSave` and `DecodeSaveHalf`. Migrations operate on owned JSON and visit each present `newCycleSave`/`owlSave` as required; they do not create a missing half to make validation pass.

`SaveManager_ProbeSaveFile` quietly validates every present half and returns structured status and decoded native creation evidence. It does not update globals, save/quarantine a file, display a popup or repair it. Normal `SaveManager_SysFlashrom_ReadData` shares that kernel, decodes the requested flash half, then copies native data/checksum into the caller; it owns normal error/quarantine behavior. Do not make the host parse native JSON again or duplicate migrations in a separate probe.

[`Rando::Compatibility`](../mm/2s2h/Rando/Compatibility.h) validates bounded original creation hashes and the supported persisted schema. Standalone loading requires exact current creation-build hash. Integrated loading accepts its explicit supported schema/reviewed legacy hashes; an explicit unsupported schema cannot fall through to the current-build check. Saving may add the supported marker while retaining original creation identity. Change the marker only when persisted IDs/layout/meanings change, not for documentation or build-only commits.

Compatibility proves native layout acceptance, not paired ownership. The host separately checks manifest/half proof, final seed and exact recorded creation timestamp/type/commit. A doc-only native commit changes future generated Git identity even when executable sources do not change; this does not preserve all standalone randomizer save acceptance. Never edit saved version/identity bytes to manufacture proof.

## Manual tracker edits do not grant rewards

[`Rando::SetCheckSkipped`](../mm/2s2h/Rando/Rando.cpp) rejects invalid checks/non-randomizer state, emits nothing for a no-op, and sends `OnRandoSetIsSkipped` only after a real change. The native [`CheckTracker`](../mm/2s2h/Rando/CheckTracker/CheckTracker.cpp) uses that setter. Producers should subscribe to the native event rather than add another widget-specific callback or direct save-field mutation.

Inbound Diptych metadata uses the same setter under echo suppression and identity/ready gates. Skip/unskip changes must preserve obtained/cycle/eligible/item/price, native receipts and inventory. A hook observation proves mutation/event behavior; it does not prove that a particular tracker row was visible or clicked. Native per-game filters remain native, even when Diptych shares window placement/visibility.

## A draw veto is not an accepted-draw bracket

[`GameInteractor_ShouldActorDraw`](../mm/2s2h/GameInteractor/GameInteractor.cpp) executes normal, ID, pointer and filtered veto callbacks before returning the final decision. [`Actor_Draw`](../mm/src/code/z_actor.c) calls the draw function and `OnActorDraw` only if that final result permits it. A callback earlier in the veto chain cannot assume a draw will follow; post-draw cannot clean up state for a vetoed draw.

Scope material/renderer changes to the actual native callback/resource lifetime. A future accepted-draw pre/post API would need explicit exception/veto/lifetime semantics; the existing hooks do not provide it. Ordinary skeletal peer rendering also does not imply native curled-Goron, shield or detached-effect coverage.

## Standalone and optional integration

Root [`CMakeLists.txt`](../CMakeLists.txt) defaults `DIPTYCH_ROOT` empty and requires a valid explicit root/shared engine for module ON. A valid nonempty root includes the external source graph even with module OFF. Use a separate empty-root cache for a truly standalone build and this fork's [build instructions](BUILDING.md).

The shared [Diptych build contract](https://github.com/chrismacdonaldw/diptych/blob/docs/runtime-contracts/docs/NATIVE_BUILDS.md) owns integrated lexical paths, dependency overlays, source pins and publication caches. Select the matching Diptych revision; older pins may lack the document. Cross-game admission/journal tests belong there; native probe/migration/setter/draw consumers belong here. State exactly whether validation was source, compile, normal native save or runtime evidence.

Desktop [`MmSaveFile::Publish`](../mm/2s2h/SaveManager/SaveFile.h) owns adjacent temporary-file replacement; `SaveManager_WriteSaveFile` returns false on serialization/publication failure. It preserves the destination on publication failure and does not claim fsync/power-loss durability; console writing remains separate. The module SaveManager still includes `DiptychModule_Goals.h`, and core/UI deletion calls Diptych-specific helpers. These remaining transition couplings should become an independently usable native save/delete lifecycle; an optional manager seam retains paired backup, identity, goal deferral and deletion policy without native code reading private host headers. Removing these references must preserve actual operation results, callback lifetime and owl/new-cycle behavior, not merely relocate the helper.

`SaveManager_SysFlashrom_WriteData` returns 0/-1, including preservation-read/migration and primary/backup publication failures. Each synchronous write result is captured in transient `SramContext::writeResult`; status zero means completion, not success. Failed owl primary writes skip backup; failed writes skip readback and quitting, pause saves offer retry, file creation/copy reload actual disk metadata, and autosave shows its icon only for a successful attempt. Song of Time continues its native cycle reset with an explicit failure warning; this is not a rollback of cycle preparation or a transaction across primary and backup. Import and deletion completion remain separate limitations.
