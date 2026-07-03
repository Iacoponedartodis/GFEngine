#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "util/RigReader.hpp"
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace editor
{

class WeaponEditor
{
public:
    WeaponEditor();
    ~WeaponEditor() = default;

    void tick(float dt);
    void draw();

private:
    struct AttachPointEntry { float x=0, y=0, z=0; };

    struct WeaponEntry {
        std::string id;
        std::string name;
        std::string jsonPath;
        std::string meshPath;
        std::string projectileMeshPath;

        // Trasformazione mesh
        float meshRotX  = -90.0f;
        float meshScale =   0.8f;

        // Attach points (model space)
        std::unordered_map<std::string, AttachPointEntry> attachPoints;

        // Statistiche
        float damage         = 25.f;
        float fireRate       = 4.5f;
        float bulletSpeed    = 25.f;
        float bulletLifetime = 3.0f;
        float bulletScale    = 0.12f;
        std::array<float,3> bulletColor = {0.3f, 0.65f, 1.0f};
        float heatPerShot    = 0.12f;
        float cooldownRate   = 0.30f;
        float overheatPenalty= 2.0f;
        float effectiveRange = 20.f;
        float minRange       = 0.0f;
        float spreadBase     = 0.02f;
        float spreadAds      = 0.005f;
        float spreadMove     = 0.06f;
        float spreadSprint   = 0.14f;
        float spreadJump     = 0.20f;
        std::string faction  = "neutral";
    };

    std::vector<WeaponEntry> m_weapons;
    int   m_sel   = -1;
    bool  m_dirty = false;

    FreeCameraViewport m_viewport;

    // Valori slider live (separati dall'entry per aggiornare il preview al volo)
    float m_rotX  = -90.0f;
    float m_scale =   0.8f;

    // Vista corrente: false = arma, true = proiettile
    bool m_showProjectileMesh = false;

    // Attach points in editing
    std::unordered_map<std::string, AttachPointEntry> m_attachPoints;
    std::string m_selAttachPoint;

    // Ossa del rig (da tinygltf)
    std::vector<std::string> m_rigJoints;

    void loadWeapons();
    void selectWeapon(int idx);
    void saveSelected();
    void reloadPreview();
    bool browseForMesh(std::string& outPath);
    void loadRigJoints();

    void drawList(float w);
    void drawMeshTab(float w);
    void drawStatsTab(float w);
    void drawViewport(float w);
};

} // namespace editor
