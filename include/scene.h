#ifndef __SCENE_H__
#define __SCENE_H__
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "entity.h"
#include "lighting.h"

class CComponentFactoryRegistry;

struct SSceneRevisionCounter
{
    std::atomic<std::uint64_t> m_Value{ 1 };

    SSceneRevisionCounter() = default;
    SSceneRevisionCounter(const SSceneRevisionCounter& other)
        : m_Value(other.m_Value.load(std::memory_order_relaxed))
    {
    }
    SSceneRevisionCounter& operator=(const SSceneRevisionCounter& other)
    {
        if (this != &other)
        {
            m_Value.store(other.m_Value.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }
    SSceneRevisionCounter(SSceneRevisionCounter&& other) noexcept
        : m_Value(other.m_Value.load(std::memory_order_relaxed))
    {
    }
    SSceneRevisionCounter& operator=(SSceneRevisionCounter&& other) noexcept
    {
        if (this != &other)
        {
            m_Value.store(other.m_Value.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    std::uint64_t Load() const
    {
        return m_Value.load(std::memory_order_relaxed);
    }

    std::uint64_t Increment()
    {
        return m_Value.fetch_add(1, std::memory_order_relaxed) + 1;
    }
};

class CScene
{
public:
    CEntity* GetSelected();
    bool IsSelected(int entityIndex) const;
    void SelectEntity(int entityIndex, bool additive);
    std::string MakeUniqueName(const std::string& baseName) const;
    std::string MakeDefaultNameFor(const CEntity& entity) const;
    void ReleaseResources();

    void MarkHierarchyDirty() const
    {
        m_HierarchyCacheDirty = true;
    }

    std::uint64_t BumpTransformRevision()
    {
        return m_TransformRevision.Increment();
    }

    std::uint64_t GetTransformRevision() const
    {
        return m_TransformRevision.Load();
    }

    void EnsureHierarchyCache() const
    {
        const size_t entityCount = m_vEntities.size();
        bool needsRebuild = m_HierarchyCacheDirty ||
            m_vChildrenIndices.size() != entityCount ||
            m_vCachedEntityIds.size() != entityCount ||
            m_vCachedParentIds.size() != entityCount;

        if (!needsRebuild)
        {
            for (size_t i = 0; i < entityCount; ++i)
            {
                if (m_vCachedEntityIds[i] != m_vEntities[i].m_Id ||
                    m_vCachedParentIds[i] != m_vEntities[i].m_ParentId)
                {
                    needsRebuild = true;
                    break;
                }
            }
        }

        if (!needsRebuild)
        {
            return;
        }

        m_IdToIndex.clear();
        m_IdToIndex.reserve(entityCount);
        m_vCachedEntityIds.resize(entityCount);
        m_vCachedParentIds.resize(entityCount);
        m_vChildrenIndices.resize(entityCount);
        for (size_t i = 0; i < entityCount; ++i)
        {
            m_vChildrenIndices[i].clear();
            m_vCachedEntityIds[i] = m_vEntities[i].m_Id;
            m_vCachedParentIds[i] = m_vEntities[i].m_ParentId;
            m_IdToIndex[m_vEntities[i].m_Id] = static_cast<int>(i);
        }

        for (size_t i = 0; i < entityCount; ++i)
        {
            const int parentId = m_vEntities[i].m_ParentId;
            if (parentId < 0)
            {
                continue;
            }
            int parentIndex = -1;
            if (parentId < static_cast<int>(entityCount))
            {
                parentIndex = parentId;
            }
            else
            {
                const auto it = m_IdToIndex.find(parentId);
                if (it != m_IdToIndex.end())
                {
                    parentIndex = it->second;
                }
            }
            if (parentIndex >= 0 && parentIndex < static_cast<int>(entityCount) &&
                parentIndex != static_cast<int>(i))
            {
                m_vChildrenIndices[static_cast<size_t>(parentIndex)].push_back(static_cast<int>(i));
            }
        }

        m_HierarchyCacheDirty = false;
    }

    int FindEntityIndexById(int entityId) const
    {
        if (entityId < 0)
        {
            return -1;
        }
        EnsureHierarchyCache();
        const auto it = m_IdToIndex.find(entityId);
        return it != m_IdToIndex.end() ? it->second : -1;
    }

    const std::vector<int>& GetChildrenIndices(int entityIndex) const
    {
        static const std::vector<int> s_vEmpty;
        if (entityIndex < 0 || entityIndex >= static_cast<int>(m_vEntities.size()))
        {
            return s_vEmpty;
        }
        EnsureHierarchyCache();
        return m_vChildrenIndices[static_cast<size_t>(entityIndex)];
    }

    std::vector<CEntity> m_vEntities;

    int m_Selected = -1;
    std::vector<int> m_vSelectedEntities;

    CComponentFactoryRegistry* m_pComponentRegistry = nullptr;
    SSceneRevisionCounter m_TransformRevision;

private:
    mutable std::unordered_map<int, int> m_IdToIndex;
    mutable std::vector<std::vector<int>> m_vChildrenIndices;
    mutable std::vector<int> m_vCachedEntityIds;
    mutable std::vector<int> m_vCachedParentIds;
    mutable bool m_HierarchyCacheDirty = true;
};

#endif // __SCENE_H__
