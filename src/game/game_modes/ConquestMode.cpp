#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/systems/AiSystem.hpp"   // registrato qui per non appesantire Application.cpp

#include <iostream>

namespace mini
{

void ConquestMode::start(World& world, Mesh* mesh, Texture* tex)
{
    std::cout << "[ConquestMode] Modalita avviata." << std::endl;
    world.initialize(); // pulisce entita'/componenti, NON i sistemi

    // Registra AiSystem qui (dopo initialize, che non cancella i sistemi)
    world.registerSystem(std::make_unique<AiSystem>());

    // --- Player (hitbox invisibile che segue la camera) ---
    m_playerEntity = world.createEntity();
    world.addTransform(m_playerEntity, {0.0f, 0.0f, 5.0f});
    world.addTeam(m_playerEntity,      {1});
    world.addHealth(m_playerEntity,    {100.0f, 100.0f});
    // Nessun MeshRenderer: il giocatore e' la camera, non un cubo visibile

    // --- Nemico 1 (rosso) ---
    const EntityId enemy1 = world.createEntity();
    world.addTransform(enemy1, {3.0f, 0.0f, -2.0f, 0, 200, 0});
    world.addTeam(enemy1,      {2});
    world.addHealth(enemy1,    {80.0f, 80.0f});
    world.addMeshRenderer(enemy1, {mesh, tex, 1.0f, 0.25f, 0.15f});
    world.addAi(enemy1, AiComponent{
        .shootCooldown = 0.5f,
        .shootInterval = 2.5f,
        .aggroRange    = 14.0f,
        .bulletMesh    = mesh,
        .bulletTexture = nullptr,
        .bulletR = 1.0f, .bulletG = 0.4f, .bulletB = 0.0f
    });

    // --- Nemico 2 (viola) ---
    const EntityId enemy2 = world.createEntity();
    world.addTransform(enemy2, {-2.5f, 0.0f, -4.0f, 0, 30, 0});
    world.addTeam(enemy2,      {2});
    world.addHealth(enemy2,    {60.0f, 60.0f});
    world.addMeshRenderer(enemy2, {mesh, tex, 0.8f, 0.2f, 0.9f});
    world.addAi(enemy2, AiComponent{
        .shootCooldown = 1.5f,
        .shootInterval = 3.0f,
        .aggroRange    = 14.0f,
        .bulletMesh    = mesh,
        .bulletTexture = nullptr,
        .bulletR = 0.8f, .bulletG = 0.2f, .bulletB = 0.9f
    });

    // --- Alleato blu (team 1, non viene colpito dai propri proiettili) ---
    const EntityId ally = world.createEntity();
    world.addTransform(ally, {-4.0f, 0.0f, -1.0f, 0, 50, 0});
    world.addTeam(ally,      {1});
    world.addHealth(ally,    {50.0f, 50.0f});
    world.addMeshRenderer(ally, {mesh, tex, 0.3f, 0.5f, 1.0f});

    // --- Piattaforma (nessun team = scenografia inerte) ---
    const EntityId platform = world.createEntity();
    world.addTransform(platform, {0, -1.5f, 0, 0, 0, 0, 10, 0.2f, 10});
    world.addMeshRenderer(platform, {mesh, tex, 0.5f, 0.5f, 0.5f});

    // --- Coperture ambiente ---
    const EntityId c1 = world.createEntity();
    world.addTransform(c1, {1.5f, 0, 0.5f, 0, -15, 0, 0.4f, 1.2f, 2.0f});
    world.addMeshRenderer(c1, {mesh, tex, 0.45f, 0.45f, 0.45f});

    const EntityId c2 = world.createEntity();
    world.addTransform(c2, {-1.0f, 0, -1.5f, 0, 40, 0, 0.4f, 1.2f, 2.0f});
    world.addMeshRenderer(c2, {mesh, tex, 0.45f, 0.45f, 0.45f});

    std::cout << "[ConquestMode] Spawn completato." << std::endl;
    std::cout << "  Nemici: rosso HP=80, viola HP=60" << std::endl;
    std::cout << "  Alleato blu HP=50, coperture grigie = scenografia" << std::endl;
    std::cout << "  Giocatore HP=100 | Avvicinati ai nemici per farli sparare!" << std::endl;
}

void ConquestMode::update(World& world, float dt) { world.tick(dt); }

} // namespace mini