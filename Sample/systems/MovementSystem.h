#pragma once
/*
  MovementSystem.h (SampleApp-side)
  ---------------------------------
  Purpose:
    - Moves entities: position += velocity * dt for any archetype store that has both Position and Velocity
      and does not contain excluded tags.

  How to customize:
    - Change required/excluded component names in the constructor to reflect the game rules.
    - Modify update() to implement your movement logic (e.g., acceleration).
*/

#include "ECS/SystemFormat.h" // IGameplaySystem, SystemBase
#include "ECS/Components.h"
class MovementSystem : public Engine::ECS::SystemBase
{
public:
    MovementSystem()
    {
        // Declare which components this system needs/excludes by name.
        // Build masks from these names in buildMasks(ComponentRegistry&).
        setRequiredNames({"Position", "Velocity"});

        // Optional excluded tags/components (define them in your registry if you use them).
        // Comment out if not used.
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "MovementSystem"; }

    // Called once after creation: registry will resolve names to IDs and build masks.
    // buildMasks is inherited from SystemBase; no override needed unless custom behavior is required.

    // Per-frame update over all matching stores.
    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;
            const auto &store = *ptr;

            // Fast store-level filter: must have required, must NOT have excluded
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            // Row-level filter and update
            const uint32_t n = store.size();
            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());
            const auto &masks = store.rowMasks();

            for (uint32_t i = 0; i < n; ++i)
            {
                if (!masks[i].matches(required(), excluded()))
                    continue;

                positions[i].x += velocities[i].x * dt;
                positions[i].y += velocities[i].y * dt;
                positions[i].z += velocities[i].z * dt;
            }
        }
    }
};