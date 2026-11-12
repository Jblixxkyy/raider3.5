#pragma once

#include "../UE4.h"

// How many AI bots to spawn in the match (plus the host player).
// The host's PC is the only human; the bots fill out the match so it
// is not empty. Kept modest so the extra AI pawns do not overload the
// host PC.
inline constexpr int BOT_COUNT = 9;

namespace Bots
{
    inline bool bSpawnedBots = false;

    // Spawn AI bot players using the game's own enemy spawner
    // (UFortCheatManager::SpawnEnemyPlayer). Called when the host jumps
    // from the battle bus, so the bots drop into the match alongside the
    // host. The game gives the spawned pawns their AI controller and
    // behavior, so they move, loot and shoot on their own.
    // Returns how many bots were spawned.
    inline int SpawnBots(AFortPlayerControllerAthena* HostPC)
    {
        if (bSpawnedBots)
            return 0;
        if (!HostPC)
            return 0;

        auto CheatManager = (UFortCheatManager*)HostPC->CheatManager;
        if (!CheatManager)
        {
            LOG_INFO("Bots: no CheatManager on the host controller, cannot spawn bots.");
            return 0;
        }

        bSpawnedBots = true;

        int Spawned = 0;
        for (int i = 0; i < BOT_COUNT; i++)
        {
            auto Pawn = CheatManager->SpawnEnemyPlayer(100.0f, 0.0f);
            if (Pawn)
                Spawned++;
        }

        LOG_INFO("Bots: spawned {} bot(s).", Spawned);
        return Spawned;
    }
}
