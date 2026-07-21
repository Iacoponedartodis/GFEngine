#pragma once

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Solo forward declaration: l'ECS non include header di gioco (ADR-002).
namespace mini { class ISystem; struct MapDef; class NavManager;
                 struct MissionDef; class DefinitionRegistry; }

namespace mini
{

class World
{
public:
    World()  = default;
    ~World() = default;
    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    void initialize();
    void tick(float dt);
    void registerSystem(std::unique_ptr<ISystem> system);

    EntityId createEntity();
    bool     destroyEntity(EntityId entity);
    [[nodiscard]] bool isValidEntity(EntityId entity) const;

    // Transform
    void addTransform(EntityId e, const TransformComponent& c);
    [[nodiscard]] bool hasTransform(EntityId e) const;
    TransformComponent*       getTransform(EntityId e);
    const TransformComponent* getTransform(EntityId e) const;

    // Team
    void addTeam(EntityId e, const TeamComponent& c);
    [[nodiscard]] bool hasTeam(EntityId e) const;
    TeamComponent*       getTeam(EntityId e);
    const TeamComponent* getTeam(EntityId e) const;

    // Velocity
    void addVelocity(EntityId e, const VelocityComponent& c);
    [[nodiscard]] bool hasVelocity(EntityId e) const;
    VelocityComponent*       getVelocity(EntityId e);
    const VelocityComponent* getVelocity(EntityId e) const;

    // Health
    void addHealth(EntityId e, const HealthComponent& c);
    [[nodiscard]] bool hasHealth(EntityId e) const;
    HealthComponent*       getHealth(EntityId e);
    const HealthComponent* getHealth(EntityId e) const;

    // MeshRenderer
    void addMeshRenderer(EntityId e, const MeshRendererComponent& c);
    [[nodiscard]] bool hasMeshRenderer(EntityId e) const;
    MeshRendererComponent*       getMeshRenderer(EntityId e);
    const MeshRendererComponent* getMeshRenderer(EntityId e) const;

    // Bullet
    void addBullet(EntityId e, const BulletComponent& c);
    [[nodiscard]] bool hasBullet(EntityId e) const;
    BulletComponent*       getBullet(EntityId e);
    const BulletComponent* getBullet(EntityId e) const;

    // Ai
    void addAi(EntityId e, const AiComponent& c);
    [[nodiscard]] bool hasAi(EntityId e) const;
    AiComponent*       getAi(EntityId e);
    const AiComponent* getAi(EntityId e) const;

    // Collider (AABB statico — solo oggetti ambiente)
    void addCollider(EntityId e, const ColliderComponent& c);
    [[nodiscard]] bool hasCollider(EntityId e) const;
    ColliderComponent*       getCollider(EntityId e);
    const ColliderComponent* getCollider(EntityId e) const;

    // Hitbox
    void addHitbox(EntityId e, const HitboxComponent& c);
    [[nodiscard]] bool hasHitbox(EntityId e) const;
    HitboxComponent*       getHitbox(EntityId e);
    const HitboxComponent* getHitbox(EntityId e) const;

    // Shield (ability "shield", 16_AiBehavior)
    void addShield(EntityId e, const ShieldComponent& c);
    [[nodiscard]] bool hasShield(EntityId e) const;
    ShieldComponent*       getShield(EntityId e);
    const ShieldComponent* getShield(EntityId e) const;

    // Comandante nemico (ability "command", Droide Tattico — ADR-024, doc 32)
    void addCommander(EntityId e, const CommanderComponent& c);
    [[nodiscard]] bool hasCommander(EntityId e) const;
    CommanderComponent*       getCommander(EntityId e);
    const CommanderComponent* getCommander(EntityId e) const;

    // Vehicle (19_Vehicles, Fase A)
    void addVehicle(EntityId e, const VehicleComponent& c);
    [[nodiscard]] bool hasVehicle(EntityId e) const;
    VehicleComponent*       getVehicle(EntityId e);
    const VehicleComponent* getVehicle(EntityId e) const;

    // Squad (26_SquadAndCommand, ADR-020): appartenenza + ordine corrente
    void addSquad(EntityId e, const SquadComponent& c);
    [[nodiscard]] bool hasSquad(EntityId e) const;
    SquadComponent*       getSquad(EntityId e);
    const SquadComponent* getSquad(EntityId e) const;

    // Abilities attive (16_AiBehavior est.: roll, ...)
    void addAbilities(EntityId e, const AbilityComponent& c);
    [[nodiscard]] bool hasAbilities(EntityId e) const;
    AbilityComponent*       getAbilities(EntityId e);
    const AbilityComponent* getAbilities(EntityId e) const;

    void setDebugLogging(bool enabled);
    [[nodiscard]] bool isDebugLoggingEnabled() const;
    [[nodiscard]] std::uint64_t                getTickCount() const;
    [[nodiscard]] const std::vector<EntityId>& getEntities()  const;

    // ── Eventi combat per il feedback HUD (hitmarker) ─────────────────
    // Scritti dal CombatSystem nel tick, consumati (e azzerati) da
    // Application dopo world.tick(). Mailbox minimale, niente event bus.
    struct CombatFeedback
    {
        bool team1Hit  = false;   // un proiettile del team 1 ha colpito
        bool team1Kill = false;   // ...e ha eliminato il bersaglio
        // Danno SUBITO dal giocatore in questo tick: direzione MONDO (XZ) della
        // SORGENTE (da dove è arrivato il colpo), per l'indicatore di danno
        // direzionale (feel/vulnerabilità, GDD 3.1). Normalizzata.
        bool  playerDamaged = false;
        float hitDirX = 0.0f, hitDirZ = 1.0f;
        void reset() { team1Hit = false; team1Kill = false; playerDamaged = false; }
    };
    CombatFeedback combatFeedback;

    // ── Entità eliminate in QUESTO tick (mailbox) ────────────────────────
    //    CombatSystem distrugge l'entità nello stesso update in cui la uccide:
    //    chi gira dopo (Squad/Ai) non può più interrogarla — getHealth() dà
    //    nullptr. Senza questa lista è IMPOSSIBILE distinguere "bersaglio
    //    ucciso" da "bersaglio sparito", e un ordine FocusFire (ADR-020)
    //    segnalerebbe come fallimento ciò che è un successo.
    //    Svuotata da Application a fine frame, come combatFeedback.
    //    Porta con sé il TEAM: l'entità è distrutta, quindi chi legge non può
    //    più interrogarla — un consumatore che volesse filtrare per squadra
    //    (es. ObjectiveSystem/EliminateTarget) conterebbe anche i propri morti.
    struct KilledUnit { EntityId entity = 0; int team = 0; };
    std::vector<KilledUnit> killedThisTick;

    // ── Log chat in-game (17_SandboxTools): messaggi leggibili degli
    //    eventi di gioco. Mailbox: i sistemi accodano, Application drena
    //    ogni frame verso la HUD. Non sostituisce la telemetria su file.
    std::vector<std::string> eventFeed;
    void pushEvent(std::string msg) { eventFeed.push_back(std::move(msg)); }

    // ── Mappa attiva (18_AiMapConsumption): puntatore opaco settato dal
    //    game mode in start(); l'AiSystem lo usa per cover/danger zone.
    //    L'ECS non include header di gioco: solo forward declaration.
    const MapDef* activeMap = nullptr;

    // ── Missione attiva (25_ObjectivesAndMissions, ADR-019): puntatore opaco
    //    settato da Application quando una missione è selezionata. nullptr =
    //    nessuna missione → ObjectiveSystem non fa nulla e i mode esistenti
    //    continuano a funzionare identici (il framework si affianca, non li
    //    riscrive). Il registry serve a risolvere gli id degli obiettivi.
    const MissionDef*         activeMission  = nullptr;
    const DefinitionRegistry* objectiveDefs  = nullptr;

    // ── Stato dei command post (ADR-009 → ADR-019) ───────────────────────
    //    I post vivono nel game mode (`CommandPosts`), che `ecs/` non può
    //    includere: Application pubblica qui lo stato ogni frame, come già fa
    //    verso l'HUD. È ciò che permette a ObjectiveSystem di esprimere
    //    CaptureZone/DefendZone **avvolgendo** ADR-009 invece di riscriverne la
    //    logica (doc 25). Vuoto = mode senza post.
    struct CommandPostState
    {
        std::string label;
        int   owner      = 0;    // 0 = neutrale
        float progress01 = 0.0f; // avanzamento cattura in corso
    };
    std::vector<CommandPostState> commandPostStates;

    // ── Bersagli strategici distruttibili (doc 25, DestroyTarget) ────────
    //    Il game mode spawna le strutture come entità e registra QUI la mappa
    //    entità→label; l'ObjectiveSystem collega una distruzione (killedThisTick)
    //    alla label senza conoscere il codice di gioco (mailbox, doc 10). Popolata
    //    a inizio partita, immutata dopo (le entità muoiono, la voce resta: serve
    //    a riconoscere QUALE bersaglio è caduto).
    // `team`/`isComms` (doc 34): servono a ricostruire lo stato della rete di
    // comunicazione senza rileggere il MapDef ogni tick.
    // Posizione + valore tattico (doc 35): questa lista è la SORGENTE UNICA di
    // intel sulle strutture. La leggono l'AI (ingaggio opportunistico), il
    // comando nemico (scelta dell'obiettivo) e — quando esisterà — la torre di
    // controllo dei cloni. Nessuno se le ricostruisce per conto proprio.
    struct StrategicTargetEntity { EntityId entity = 0; std::string label;
                                   int team = 2; bool isComms = false;
                                   bool isControl = false;
                                   float x = 0.0f, z = 0.0f;
                                   float priority = 0.5f;
                                   float engageRadius = 0.0f; };
    std::vector<StrategicTargetEntity> strategicTargets;

    // ── Stato della battaglia (doc 25: conseguenze degli obiettivi) ──────
    //    Gli obiettivi non sono caselle da spuntare: completarli **cambia la
    //    battaglia**. ObjectiveSystem scrive QUI; ogni sistema competente legge
    //    solo ciò che lo riguarda (ConquestMode i rinforzi, AiSystem la
    //    precisione nemica...). È questo canale a permettere che le conseguenze
    //    restino DATI dichiarativi invece di `if (objectiveId == ...)` sparsi.
    //    Azzerato da `initialize()`: è stato per-missione.
    struct BattleState
    {
        bool  enemyReinforcementsBlocked = false;  // il nemico non rimpiazza le perdite
        float enemyAccuracyMult = 1.0f;            // <1 = nemici disorganizzati
        std::string allySpawnPost;                 // post dove rinasce la squadra ("" = spawn mappa)
        // Riserve da aggiungere alla squadra: un DELTA, perché i ticket li possiede
        // il game mode. Il mode lo consuma e lo azzera.
        int   pendingAllyReinforcements = 0;
    };
    BattleState battleState;

    // ── Rete di comunicazione per fazione (doc 34, ADR-038) ──────────────
    //    La torre di comunicazione non è un interruttore: finché è viva la sua
    //    fazione comunica bene, quando cade informazioni/ordini/rinforzi
    //    RALLENTANO — non si fermano mai. Scritto dal game mode (che possiede le
    //    strutture), letto da AiSystem (contatti, comandante) e dal mode stesso
    //    (rimpiazzi). Mailbox: `ecs/` non deve conoscere i game mode.
    struct CommsState
    {
        // `hadTower` distingue "questa fazione ha PERSO la torre" da "questa
        // mappa non ne autora nessuna". Senza, ogni mappa esistente degraderebbe
        // in silenzio: chi non ha mai avuto una torre comunica normalmente.
        bool  hadTower  = false;
        bool  towerAlive = false;
        float shareRangeMult   = 1.0f;   // raggio di propagazione di un avvistamento
        float shareDelay       = 0.0f;   // s prima che l'informazione sia utilizzabile
        float orderPeriodMult  = 1.0f;   // il comando ri-decide più di rado
        float reinforceMult    = 1.0f;   // i rimpiazzi tardano (MAI bloccati)
        bool  degraded() const { return hadTower && !towerAlive; }
    };
    CommsState comms[3];   // indicizzato per team: [1] Repubblica, [2] Separatisti

    // ── Torre di controllo dei cloni (doc 36, ADR-040) ───────────────────
    //    NON è il gemello del Droide Tattico, ed è importante che non lo
    //    diventi. Il comandante droide pubblica UN focus e tutti i droidi vi
    //    convergono (`enemyCommand`). La torre di controllo pubblica una LISTA
    //    di posti che contano e si ferma lì: **nessun ordine, nessuna
    //    destinazione imposta**. È ogni clone a scegliere per conto suo quale
    //    segnale seguire — ed è così che i cloni restano più indipendenti dei
    //    droidi (direttiva utente 2026-07-20).
    struct AllyIntel
    {
        bool active = false;   // esiste una torre di controllo VIVA di team 1
        struct Signal
        {
            float x = 0.0f, z = 0.0f;
            float radius = 8.0f;
            float weight = 0.5f;      // quanto quel posto conta
            int   crowd  = 0;         // truppe team-1 già presenti (saturazione, KI #73)
            std::string label;
        };
        std::vector<Signal> signals;
    };
    AllyIntel allyIntel;

    // ── Statistiche di missione (doc 25, GDD 9.6) ────────────────────────
    //    Accumulate DURANTE la missione da chi conosce il fatto — nessuno le
    //    ricostruisce a posteriori: gli eventi (una morte, una kill) esistono
    //    solo nell'istante in cui accadono.
    //    Servono al **debrief**: il giudizio non è un voto unico ma l'INSIEME dei
    //    fattori — è la combinazione a raccontare com'è andata la missione, ed è
    //    ciò che rende reale l'esperienza (progressione, doc 27).
    //    Azzerate da `initialize()`: sono per-missione, non per-sessione.
    struct MissionStats
    {
        int   playerKills   = 0;   // nemici uccisi DAL giocatore (bullet.fromPlayer)
        int   teamKills     = 0;   // nemici uccisi dalla squadra (giocatore incluso)
        int   alliesLost    = 0;   // alleati caduti (costo della missione)
        int   playerDeaths  = 0;   // quante volte è caduto il giocatore
        float missionTime   = 0.0f;
        int   objectivesDone   = 0;
        int   objectivesFailed = 0;
    };
    MissionStats missionStats;

    // ── Navigazione (ADR-017 Phase B): puntatore opaco al NavManager, settato
    //    da Application dopo la build del navmesh. AiSystem lo usa per il
    //    pathfinding, CrowdSystem per tick+write-back. nullptr = fallback aiMove.
    NavManager* nav = nullptr;

    // ── Squadra (ADR-020, doc 26): entità del giocatore, settata dal game mode
    //    via Application. Il SquadSystem la usa come leader della squadra alleata
    //    quando è valida e di team 1 (in simulazione il player è neutro/parcheggiato
    //    → il leader diventa un'AI). 0 = nessun giocatore attivo.
    EntityId playerEntity = 0;

    // ── Ordine contestuale del giocatore (ADR-020 Phase B): l'Application
    //    risolve il contesto col raycast del mirino e deposita qui l'intenzione;
    //    SquadSystem la consuma e azzera `pending`. Mailbox, non chiamata diretta:
    //    è ciò che tiene `ecs/` indipendente dal codice di gioco.
    SquadOrderRequest squadOrder;

    // ── Comando nemico (ADR-024, doc 32): la CONTROPARTE del comando giocatore.
    //    Un Droide Tattico (CommanderComponent) vivo di team 2 fa sì che AiSystem
    //    calcoli un "focus" strategico (il command post non-separatista più vicino
    //    al comandante) e lo pubblichi QUI; i droidi in pattuglia convergono sul
    //    focus (movimento, non combattimento). Ricalcolata ogni tick da AiSystem:
    //    nessun comandante vivo → `active=false` e i droidi tornano alla pattuglia.
    //    `commanderAlive` serve solo a distinguere "nessun bersaglio" da "comandante
    //    ucciso" per il messaggio-conseguenza. Vedi [[droide-tattico-concept]].
    // ── Stato dei settori (ADR-034) ──────────────────────────────────────
    //    Parallelo a `activeMap->sectors`. Ricalcolato ogni tick da AiSystem con
    //    una sola passata sulle entità. È STATO DI PARTITA (non dato di mappa):
    //    presenze contrapposte, chi controlla, quanto è conteso. È il livello su
    //    cui il comandante ragiona invece di guardare solo l'owner dei post.
    //    Dato puro e astratto: riusabile da una futura simulazione (doc 33 §9).
    struct SectorState
    {
        int   allies = 0, enemies = 0;
        int   controllingTeam = 0;   // 0 = conteso o vuoto
        float pressure = 0.0f;       // 0..1: quanto le due parti si contendono la zona
    };
    std::vector<SectorState> sectorStates;

    struct EnemyCommand
    {
        // v2 (ADR-042 / doc 32): il comandante gestisce PIÙ FRONTI insieme. Non un
        // intento unico globale ma una LISTA di direttive, ciascuna con la sua
        // stance derivata dal bilancio LOCALE del settore (spingi dove domini,
        // tieni dove sei pressato). I droidi si distribuiscono sulle direttive
        // (pesati dal bias) e scelgono da sé il COME (percorso, copertura,
        // ingaggio): il comandante gestisce RISORSE e PRIORITÀ, non il micro
        // (correzione utente 2026-07-20). Il micro fine andrà al grado intermedio.
        enum Stance { Hold = 0, Advance = 1, Retreat = 2 };
        struct Directive
        {
            float x = 0.0f, z = 0.0f;   // centro del settore-obiettivo (ZONA, non waypoint)
            float radius = 0.0f;        // raggio della zona
            int   stance = Hold;        // Advance = spingi, Hold = tieni, Retreat = ripiega
            float weight = 0.0f;        // priorità del fronte (per la distribuzione)
            std::string label;
        };
        bool  active         = false;  // c'è almeno una direttiva valida
        bool  commanderAlive = false;  // ≥1 comandante di team 2 vivo (edge → feed)
        std::vector<Directive> directives;   // i fronti gestiti insieme
    };
    EnemyCommand enemyCommand;

private:
    std::uint64_t m_tickCount    = 0;
    EntityId      m_nextEntityId = 1;

    std::vector<std::unique_ptr<ISystem>>               m_systems;
    std::vector<EntityId>                               m_entities;
    std::unordered_set<EntityId>                        m_aliveEntities;

    std::unordered_map<EntityId, TransformComponent>    m_transforms;
    std::unordered_map<EntityId, TeamComponent>         m_teams;
    std::unordered_map<EntityId, VelocityComponent>     m_velocities;
    std::unordered_map<EntityId, HealthComponent>       m_healths;
    std::unordered_map<EntityId, MeshRendererComponent> m_meshRenderers;
    std::unordered_map<EntityId, BulletComponent>       m_bullets;
    std::unordered_map<EntityId, AiComponent>           m_ais;
    std::unordered_map<EntityId, ColliderComponent>     m_colliders;
    std::unordered_map<EntityId, HitboxComponent>      m_hitboxes;
    std::unordered_map<EntityId, ShieldComponent>       m_shields;
    std::unordered_map<EntityId, CommanderComponent>    m_commanders;
    std::unordered_map<EntityId, VehicleComponent>      m_vehicles;
    std::unordered_map<EntityId, SquadComponent>        m_squads;
    std::unordered_map<EntityId, AbilityComponent>      m_abilities;

    bool m_debugLogging = false;
};

} // namespace mini