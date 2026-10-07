#include "entity.h"
#include "component.h"
namespace
{
bool OwnsLoadedModel(const CModelAsset& asset)
{
    return !asset.m_IsProcedural &&
           asset.m_LoadedModel.meshCount > 0 &&
           asset.m_LoadedModel.meshes != nullptr;
}
} // anonymous

CModelAsset::CModelAsset() = default;

CModelAsset::~CModelAsset()
{
    Unload();
}

CModelAsset::CModelAsset(CModelAsset&& other) noexcept
    : m_Name(std::move(other.m_Name)),
      m_FilePath(std::move(other.m_FilePath)),
      m_Type(other.m_Type),
      m_IsProcedural(other.m_IsProcedural),
      pfnGenerator(std::move(other.pfnGenerator)),
      m_LoadedModel(other.m_LoadedModel)
{
    other.m_LoadedModel = {};
}

CModelAsset& CModelAsset::operator=(CModelAsset&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Unload();

    m_Name = std::move(other.m_Name);
    m_FilePath = std::move(other.m_FilePath);
    m_Type = other.m_Type;
    m_IsProcedural = other.m_IsProcedural;
    pfnGenerator = std::move(other.pfnGenerator);
    m_LoadedModel = other.m_LoadedModel;
    other.m_LoadedModel = {};

    return *this;
}

Model CModelAsset::TakeLoadedModel()
{
    Model model = m_LoadedModel;
    m_LoadedModel = {};
    return model;
}

void CModelAsset::Unload()
{
    if (OwnsLoadedModel(*this))
    {
        UnloadModel(m_LoadedModel);
    }

    m_LoadedModel = {};
}

const char* ObjectTypeName(EObjectType type)
{
    switch (type)
    {
        case OBJECT_CUBE:      return "Cube";
        case OBJECT_SPHERE:    return "Sphere";
        case OBJECT_CONE:      return "Cone";
        case OBJECT_CYLINDER:  return "Cylinder";
        case OBJECT_HEMISPHERE: return "Hemisphere";
        case OBJECT_TORUS:     return "Torus";
        default:               return "Cube";
    }
}

CEntity::CEntity()
    : m_Id(0),
      m_Name(""),
      m_ParentId(-1),
      m_IsGroup(false)
{
    m_pComponents = std::make_unique<CComponentManager>();

    auto pTransform = std::make_shared<CTransformComponent>();
    m_pComponents->AddComponent(pTransform);

    auto pMesh = std::make_shared<CMeshComponent>();
    m_pComponents->AddComponent(pMesh);
}

CEntity::CEntity(int entityId)
    : m_Id(entityId),
      m_Name(ObjectTypeName(OBJECT_CUBE)),
      m_ParentId(-1),
      m_IsGroup(false)
{
    m_pComponents = std::make_unique<CComponentManager>();

    auto pTransform = std::make_shared<CTransformComponent>();
    m_pComponents->AddComponent(pTransform);

    auto pMesh = std::make_shared<CMeshComponent>();
    m_pComponents->AddComponent(pMesh);
}

CEntity::~CEntity() = default;

CEntity::CEntity(const CEntity& other)
    : m_Id(other.m_Id),
      m_Name(other.m_Name),
      m_vTags(other.m_vTags),
      m_ParentId(other.m_ParentId),
      m_IsGroup(other.m_IsGroup),
      m_pComponents(std::make_unique<CComponentManager>())
{
    if (other.m_pComponents)
    {
        m_pComponents->CloneInto(*other.m_pComponents);
    }
}

CEntity& CEntity::operator=(const CEntity& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_Id = other.m_Id;
    m_Name = other.m_Name;
    m_vTags = other.m_vTags;
    m_ParentId = other.m_ParentId;
    m_IsGroup = other.m_IsGroup;

    m_pComponents = std::make_unique<CComponentManager>();
    if (other.m_pComponents)
    {
        m_pComponents->CloneInto(*other.m_pComponents);
    }

    return *this;
}

CComponentManager* CEntity::GetComponents()
{
    return m_pComponents.get();
}

const CComponentManager* CEntity::GetComponents() const
{
    return m_pComponents.get();
}

CTransformComponent* CEntity::GetTransformComponent()
{
    return m_pComponents ? m_pComponents->GetTransform() : nullptr;
}

const CTransformComponent* CEntity::GetTransformComponent() const
{
    return m_pComponents ? m_pComponents->GetTransform() : nullptr;
}

CMeshComponent* CEntity::GetMeshComponent()
{
    return m_pComponents ? m_pComponents->GetMesh() : nullptr;
}

const CMeshComponent* CEntity::GetMeshComponent() const
{
    return m_pComponents ? m_pComponents->GetMesh() : nullptr;
}

CLightComponent* CEntity::GetLightComponent()
{
    return m_pComponents ? m_pComponents->GetLight() : nullptr;
}

const CLightComponent* CEntity::GetLightComponent() const
{
    return m_pComponents ? m_pComponents->GetLight() : nullptr;
}

CMaterialComponent* CEntity::GetMaterialComponent()
{
    return m_pComponents ? m_pComponents->GetMaterial() : nullptr;
}

const CMaterialComponent* CEntity::GetMaterialComponent() const
{
    return m_pComponents ? m_pComponents->GetMaterial() : nullptr;
}

CCollisionComponent* CEntity::GetCollisionComponent()
{
    return m_pComponents ? m_pComponents->GetCollision() : nullptr;
}

const CCollisionComponent* CEntity::GetCollisionComponent() const
{
    return m_pComponents ? m_pComponents->GetCollision() : nullptr;
}
