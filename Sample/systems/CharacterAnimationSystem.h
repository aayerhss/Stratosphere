#pragma once

#include "ECS/SystemFormat.h"
#include "assets/AssetManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// CharacterAnimationSystem
// - Advances per-entity RenderAnimation time
// - Only applies to entities tagged as Selected (row mask)
class CharacterAnimationSystem : public Engine::ECS::SystemBase
{
public:
    CharacterAnimationSystem()
    {
        setRequiredNames({"RenderModel", "RenderAnimation"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "CharacterAnimationSystem"; }

    void setAssetManager(Engine::AssetManager *assets) { m_assets = assets; }

    void buildMasks(Engine::ECS::ComponentRegistry &registry) override
    {
        Engine::ECS::SystemBase::buildMasks(registry);
        m_selectedId = registry.ensureId("Selected");
    }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        if (!m_assets)
            return;

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;

            auto &store = *ptr;
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;
            if (!store.hasRenderModel() || !store.hasRenderAnimation())
                continue;

            auto &renderModels = store.renderModels();
            auto &renderAnimations = store.renderAnimations();
            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t row = 0; row < n; ++row)
            {
                if (!masks[row].matches(required(), excluded()))
                    continue;
                if (!masks[row].has(m_selectedId))
                    continue;

                const Engine::ModelHandle handle = renderModels[row].handle;
                Engine::ModelAsset *asset = m_assets->getModel(handle);
                if (!asset)
                    continue;

                auto &anim = renderAnimations[row];

                if (asset->animClips.empty())
                {
                    anim.clipIndex = 0;
                    anim.timeSec = 0.0f;
                    continue;
                }

                const uint32_t safeClip = std::min(anim.clipIndex, static_cast<uint32_t>(asset->animClips.size() - 1));
                anim.clipIndex = safeClip;

                const float duration = asset->animClips[safeClip].durationSec;
                if (!anim.playing || duration <= 1e-6f)
                    continue;

                anim.timeSec += dt * anim.speed;
                if (anim.loop)
                {
                    anim.timeSec = std::fmod(anim.timeSec, duration);
                    if (anim.timeSec < 0.0f)
                        anim.timeSec += duration;
                }
                else
                {
                    if (anim.timeSec < 0.0f)
                        anim.timeSec = 0.0f;
                    if (anim.timeSec > duration)
                        anim.timeSec = duration;
                }
            }
        }
    }

private:
    Engine::AssetManager *m_assets = nullptr;
    uint32_t m_selectedId = Engine::ECS::ComponentRegistry::InvalidID;
};
