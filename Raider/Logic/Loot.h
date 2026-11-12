#pragma once

#include <cstdlib>

#include "../UE4.h"
#include "Spawners.h"

namespace Loot
{
    // Cap on how many floor-loot pickups to spawn per match, so a bad
    // container count can never flood the map.
    inline constexpr int MAX_FLOOR_LOOT_PICKUPS = 100;

    // The loot tier group used for floor loot. "Loot_Floor" is the
    // floor-loot group in the game's loot tier data. If it ever comes
    // back empty we fall back to the chest tier ("Loot_Treasure"), so a
    // bad group name never means no loot.
    inline FName FloorLootTierGroup()
    {
        return GetKismetString()->STATIC_Conv_StringToName(L"Loot_Floor");
    }

    inline FName ChestLootTierGroupFallback()
    {
        return GetKismetString()->STATIC_Conv_StringToName(L"Loot_Treasure");
    }

    // Pick loot drops for a tier group. Tries the requested group, then
    // falls back to the chest tier so a bad group name never means no loot.
    inline TArray<FFortItemEntry> PickDrops(const FName& TierGroup)
    {
        TArray<FFortItemEntry> OutDrops;
        if (GetFortKismet()->STATIC_PickLootDrops(TierGroup, -1, 0, &OutDrops) && OutDrops.Num() > 0)
            return OutDrops;

        TArray<FFortItemEntry> FallbackDrops;
        if (GetFortKismet()->STATIC_PickLootDrops(ChestLootTierGroupFallback(), -1, 0, &FallbackDrops))
            return FallbackDrops;

        return OutDrops;
    }

    // Called when a player searches a chest / ammo box. Rolls the loot
    // the container should drop and tosses it out of the container.
    // (This used to be commented out — chests opened but gave nothing.)
    inline void OnContainerSearched(ABuildingContainer* Container)
    {
        if (!Container)
            return;

        auto Drops = PickDrops(Container->SearchLootTierGroup);
        if (Drops.Num() == 0)
        {
            LOG_INFO("Loot: container gave no drops (tier {}).", Container->SearchLootTierGroup.ToString());
            return;
        }

        // Where the loot comes out of the container.
        FVector SpawnLocation = Container->LootSpawnLocation_Athena;
        if (SpawnLocation.X == 0.0f && SpawnLocation.Y == 0.0f && SpawnLocation.Z == 0.0f)
            SpawnLocation = Container->K2_GetActorLocation();

        int Spawned = 0;
        for (int i = 0; i < Drops.Num(); i++)
        {
            auto& Drop = Drops[i];
            if (!Drop.ItemDefinition)
                continue;

            Spawners::SummonPickupFromChest(Drop.ItemDefinition, Drop.Count, SpawnLocation);
            Spawned++;
        }

        LOG_INFO("Loot: container dropped {} item(s).", Spawned);
    }

    // Spawn floor loot once per match: a pickup near a subset of the
    // containers (chests live inside buildings, so loot near them lands
    // on the ground where floor loot belongs). Also fills in any empty
    // AFortPickup actors already placed in the level. Only sets the
    // "spawned" flag when it actually placed loot, so a call that runs
    // before the level is loaded can be retried.
    inline void SpawnFloorLoot()
    {
        if (bSpawnedFloorLoot)
            return;

        int Spawned = 0;

        // 1) Fill in any empty AFortPickup actors already in the level.
        {
            TArray<AActor*> Pickups = GetAllActorsOfClass(AFortPickup::StaticClass());
            for (int i = 0; i < Pickups.Num() && Spawned < MAX_FLOOR_LOOT_PICKUPS; i++)
            {
                auto Pickup = (AFortPickup*)Pickups[i];
                if (!Pickup || Pickup->bPickedUp)
                    continue;
                if (Pickup->PrimaryPickupItemEntry.ItemDefinition)
                    continue; // already has an item

                auto Drops = PickDrops(FloorLootTierGroup());
                if (Drops.Num() == 0)
                    break;

                auto& Drop = Drops[0];
                if (!Drop.ItemDefinition)
                    continue;

                Pickup->PrimaryPickupItemEntry.Count = Drop.Count;
                Pickup->PrimaryPickupItemEntry.ItemDefinition = Drop.ItemDefinition;
                Pickup->OnRep_PrimaryPickupItemEntry();
                Spawned++;
            }
        }

        // 2) Spawn new pickups near containers (chests are inside
        // buildings, so this puts floor loot on the ground in sensible
        // places). Only a subset, so the map is not flooded.
        {
            TArray<AActor*> Containers = GetAllActorsOfClass(ABuildingContainer::StaticClass());
            for (int i = 0; i < Containers.Num() && Spawned < MAX_FLOOR_LOOT_PICKUPS; i += 3)
            {
                auto Container = (ABuildingContainer*)Containers[i];
                if (!Container)
                    continue;

                auto Base = Container->K2_GetActorLocation();
                // Small random offset so the loot is on the floor next
                // to the chest, not inside it.
                float OffsetX = (float)(std::rand() % 200) - 100.0f;
                float OffsetY = (float)(std::rand() % 200) - 100.0f;
                FVector Location = FVector { Base.X + OffsetX, Base.Y + OffsetY, Base.Z + 10.0f };

                auto Drops = PickDrops(FloorLootTierGroup());
                if (Drops.Num() == 0)
                    break;

                auto& Drop = Drops[0];
                if (!Drop.ItemDefinition)
                    continue;

                Spawners::SpawnPickupFromFloorLoot(Drop.ItemDefinition, Drop.Count, Location);
                Spawned++;
            }
        }

        if (Spawned > 0)
            bSpawnedFloorLoot = true;

        LOG_INFO("Loot: spawned {} floor-loot pickup(s).", Spawned);
    }
}
