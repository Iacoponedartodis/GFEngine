#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace mini
{

// ── Fazioni ───────────────────────────────────────────────────────────────
enum class Faction { Neutral = 0, Republic = 1, Separatist = 2 };
inline Faction factionFromString(const std::string& s)
{
    if (s == "republic")   return Faction::Republic;
    if (s == "separatist") return Faction::Separatist;
    return Faction::Neutral;
}
inline const char* factionToString(Faction f)
{
    switch (f) {
    case Faction::Republic:   return "republic";
    case Faction::Separatist: return "separatist";
    default:                  return "neutral";
    }
}
// Array statico per combo UI
inline const char* const* factionNames()
{
    static const char* names[] = { "neutral", "republic", "separatist" };
    return names;
}
inline int factionToIndex(Faction f) { return (int)f; }
inline Faction factionFromIndex(int i) { return (Faction)i; }

// ── AbilityDef ────────────────────────────────────────────────────────────
// data/abilities/<id>.json
struct AbilityDef
{
    std::string id;
    std::string name;
    std::string type;    // "shield" | "roll" | "melee" | "jetpack" | "missile" | "command_aura"
    float param1   = 0.0f;   // es. shield_hp, roll_speed, melee_range
    float param2   = 0.0f;   // es. regen_rate, roll_duration
    float param3   = 0.0f;   // es. regen_delay, cooldown
    float cooldown = 5.0f;
    bool  passive  = false;
};

// ── WeaponDef ─────────────────────────────────────────────────────────────
// data/weapons/<id>.json
struct WeaponDef
{
    std::string id;
    std::string name;
    Faction     faction        = Faction::Neutral;
    float damage              = 25.0f;
    float fireRate            = 4.5f;
    float bulletSpeed         = 25.0f;
    float bulletLifetime      = 3.0f;
    float bulletScale         = 0.12f;
    std::array<float,3> bulletColor = {0.3f, 0.65f, 1.0f};
    float heatPerShot         = 0.12f;
    float cooldownRate        = 0.30f;
    float overheatPenalty     = 2.0f;
    float effectiveRange      = 20.0f;
    float minRange            = 0.0f;

    // Precisione: 0=sempre al centro, 1=massima dispersione
    // Rappresenta il raggio di dispersione base (in gradi o unità angolari)
    float baseSpread          = 0.02f;  // fermo, non in mira
    float adsSpread           = 0.005f; // in mira (ADS)
    float moveSpread          = 0.06f;  // in movimento
    float sprintSpread        = 0.14f;  // in corsa
    float jumpSpread          = 0.20f;  // in aria

    std::string meshPath;
    std::string projectileMeshPath;

    // Trasformazione/attach del modello arma (autorati nel Weapon Editor).
    // gripAttach = punto impugnatura (attach "right_hand" o "grip") nel
    // model space GREZZO del GLB; usato per agganciare l'arma alla mano.
    float meshScale = 0.8f;
    float meshRotX  = 0.0f;
    float meshRotY  = 0.0f;   // raddrizza GLB orientati lateralmente (yaw)
    std::array<float,3> gripAttach   = {0.0f, 0.0f, 0.0f};
    std::array<float,3> muzzleAttach = {0.0f, 0.0f, 0.0f};
};

// ── AiProfileDef ─────────────────────────────────────────────────────────
// data/ai/<id>.json
struct AiProfileDef
{
    std::string id;
    std::string role;    // "infantry" | "heavy" | "sniper" | "elite" | "support"
    float sightRange       = 20.0f;
    float fovDeg           = 110.0f;
    float hearingRange     = 12.0f;
    float reactionTime     = 0.4f;
    float aggression       = 0.65f;
    float accuracy         = 0.55f;
    float coverPreference  = 0.75f;
    float retreatHpThresh  = 0.25f;
    float peekDurationMin  = 0.6f;
    float peekDurationMax  = 1.1f;
    float hideDurationMin  = 0.8f;
    float hideDurationMax  = 1.8f;
    float repositionChance = 0.30f;
    float flankChance      = 0.20f;
    float shootInterval    = 2.5f;
    float patrolSpeed      = 2.5f;
    float seekSpeed        = 4.0f;
    bool  jumpEnabled      = true;
};

// ── EnemyDef ─────────────────────────────────────────────────────────────
// Composizione completa di un'unità nemica. Solo dati, nessuna logica.
// data/enemies/<id>.json
// Nota: mesh/texture/color sono qui direttamente.
//       character_type era un'astrazione prematura senza UI dedicata.
struct EnemyDef
{
    std::string id;
    std::string name;
    Faction     faction   = Faction::Separatist;
    int         team      = 2;

    // Visuale
    std::string meshPath;
    std::string texturePath;
    std::array<float,3> color = {0.70f, 0.60f, 0.45f};
    float meshRotX  = 0.0f;
    float meshRotY  = 0.0f;
    float meshScale = 1.0f;

    // Punto piede nel model space (usato per posizionamento su suolo)
    std::array<float,3> footAttach = {0.0f, 0.0f, 0.0f};
    [[nodiscard]] float footY() const { return footAttach[1]; }

    // Tutti gli attach point autorati (right_hand, left_hand, eye, muzzle...)
    std::unordered_map<std::string, std::array<float,3>> attachPoints;

    // Posa dell'arma in mano (autorata nell'Entity Editor, "weapon_display")
    struct WeaponDisplay
    {
        std::string weaponId;               // vuoto = usa l'arma primaria
        std::string hand  = "right_hand";
        float       scale = 1.0f;
        std::array<float,3> rot    = {0.0f, 0.0f, 0.0f};
        std::array<float,3> offset = {0.0f, 0.0f, 0.0f};
    };
    WeaponDisplay weaponDisplay;

    // Composizione comportamentale
    std::string aiProfileId;
    std::string hitboxProfileId;
    std::vector<std::string> weaponIds;
    std::vector<std::string> abilityIds;

    // Stats
    float hp          = 80.0f;
    float moveSpeed   = 4.0f;
    float damageScale = 1.0f;

    // Colore proiettili
    std::array<float,3> bulletColor = {1.0f, 0.5f, 0.0f};

    [[nodiscard]] const std::string& primaryWeaponId() const
    {
        static const std::string empty;
        return weaponIds.empty() ? empty : weaponIds[0];
    }
    [[nodiscard]] bool hasAbility(const std::string& abilityId) const
    {
        for (auto& a : abilityIds) if (a == abilityId) return true;
        return false;
    }
};

// ── MapGeometryBox ──────────────────────────────────────────────────────────
// Un box di geometria della mappa, autorato nel Map Editor.
// Posizione = centro; dimensioni = estensione totale (full size).
struct MapGeometryBox
{
    float x = 0, y = 0, z = 0;     // centro
    float ry = 0;                  // rotazione attorno a Y (gradi)
    float sx = 2, sy = 2, sz = 2;  // dimensioni totali
    float r = 0.35f, g = 0.32f, b = 0.28f;
    bool  collider = true;
};

// ── CommandPostDef ──────────────────────────────────────────────────────────
// Punto di comando catturabile, autorato nel Map Editor (ADR-009).
// Riusato da tutte le modalità: Conquista (ticket bleed), future Assalto/Difesa.
struct CommandPostDef
{
    std::string label = "Post";
    float x = 0, y = 0, z = 0;   // posizione (y = suolo)
    float radius      = 4.0f;    // raggio di cattura (XZ)
    int   initialTeam = 0;       // 0 = neutrale, 1 = alleati, 2 = nemici
    float captureTime = 8.0f;    // secondi di presenza per catturare
};

// ── VehicleDef (19_Vehicles, Fase A) ─────────────────────────────────────
// data/vehicles/<id>.json — veicolo leggero guidabile dal giocatore.
struct VehicleDef
{
    std::string id;
    std::string name;
    float hp          = 150.0f;
    float maxSpeed    = 12.0f;   // m/s in avanti (metà in retro)
    float accel       = 10.0f;   // m/s^2
    float turnRateDeg = 90.0f;   // gradi/s a piena sterzata
    std::string meshPath;        // vuoto = box di fallback
    float meshScale  = 1.0f;
    float meshRotX   = 0.0f;
    float meshRotY   = 0.0f;
    float meshOffsetY = 0.0f;    // alza/abbassa la mesh (i GLB hanno base a Y=0)
    // Collisione a box: raggiunge il suolo per la guida (Todo #23 forme ricche)
    float halfX = 0.7f, halfY = 0.5f, halfZ = 1.2f;
    // Volume di DANNO (19_Vehicles Fase B): separato dalla collisione, così
    // lo spazio vuoto sotto uno speeder che fluttua non conta come bersaglio.
    // 0 = usa il box di collisione (retrocompatibile). offset rispetto al
    // centro fisico (= gy + halfY).
    float hitOffsetY = 0.0f;
    float hitHalfX = 0.0f, hitHalfY = 0.0f, hitHalfZ = 0.0f;
    std::array<float,3> color = {0.55f, 0.55f, 0.60f};
};

// Spawn di un veicolo in mappa (MapDef.vehicleSpawns)
struct VehicleSpawnDef
{
    std::string vehicleId;
    float x = 0, z = 0;
    float ry = 0;    // orientamento iniziale (gradi)
};

// ── Map Metadata (15_MapMetadata) ────────────────────────────────────────
// Hint spaziali opzionali autorati nel Map Editor. Solo dati: nessun
// sistema runtime li consuma ancora (sarà l'AI tattica, Todo #3 fase 2).

// Posizione + direzione da cui un'AI può coprirsi e sparare.
// height distingue copertura bassa (peek-over) da alta (peek-around).
struct CoverPointDef
{
    float x = 0, y = 0, z = 0;
    float facingDeg = 0.0f;   // direzione del fronte di copertura (gradi, yaw)
    float height    = 1.0f;   // altezza della copertura (m)
};

// Percorso di pattuglia con nome (riusabile da più squadre in futuro).
struct PatrolRouteDef
{
    std::string id = "route";
    std::vector<std::array<float,3>> points;   // ordinati
};

// Area esposta/pericolosa (hint morbido, NON un collider).
struct DangerZoneDef
{
    float x = 0, y = 0, z = 0;
    float radius      = 4.0f;
    float dangerLevel = 0.5f;   // 0..1, semantica del consumatore AI
};

// ── MapDef ────────────────────────────────────────────────────────────────
// data/maps/<id>.json
struct MapDef
{
    std::string id;
    std::string name;
    std::string meshPath;
    std::string metadataPath;
    std::string navmeshPath;
    std::array<float,3> spawnTeam1 = {0.f, 0.86f,  8.f};
    std::array<float,3> spawnTeam2 = {0.f, 0.86f, -8.f};
    int maxTickets = 10;
    int enemyCount = 6;
    int allyCount  = 1;
    std::vector<std::string> enemyTypes;
    std::vector<std::string> allyTypes;
    std::vector<MapGeometryBox> geometry;
    std::vector<CommandPostDef> commandPosts;

    // Map Metadata (15_MapMetadata) — opzionali, vuoti finché non autorati
    std::vector<CoverPointDef>  coverPoints;
    std::vector<PatrolRouteDef> patrolRoutes;
    std::vector<DangerZoneDef>  dangerZones;

    // Veicoli in mappa (19_Vehicles, Fase A) — opzionale
    std::vector<VehicleSpawnDef> vehicleSpawns;
};

// ── Obiettivi e missioni (25_ObjectivesAndMissions, ADR-019) ─────────────
// Il framework generico che sostituisce "un mode per ogni tipo di missione".
// L'IGameMode continua a decidere le REGOLE (ticket, outcome); gli obiettivi
// decidono COSA fare. I command post (ADR-009) restano validi e intoccati:
// vengono avvolti, non riscritti.

enum class ObjectiveType
{
    // Valutabili leggendo solo il World → implementati (Phase A)
    ReachArea,            // un'unità del team entra nella zona
    EliminateTarget,      // eliminare N unità del team bersaglio
    HoldAreaForDuration,  // presenza continuativa nella zona per holdSeconds
    // Command post (ADR-009) espressi come obiettivi: la logica di cattura resta
    // in `CommandPosts`, qui se ne legge solo l'esito — doc 25 dice di avvolgere
    // ADR-009, non riscriverlo.
    CaptureZone,          // il post `targetPost` diventa di actorTeam
    DefendZone,           // il post resta di actorTeam per holdSeconds; perderlo = fallito
    // Dichiarati dal doc 25 ma NON ancora eseguiti: falliscono con causa
    DestroyTarget, EscortEntity, SurviveWave, InteractHack
};

// La stratificazione chiesta dalla Fase 2 (00_Vision) è un CAMPO, non tre
// sistemi paralleli: tre sistemi sarebbero il fork che ADR-008/014 evitano.
enum class ObjectiveTier { Primary, Strategic, Tactical };

enum class ActivationType
{
    Immediate,        // attivo dall'inizio
    AfterObjective,   // dopo il completamento di activationObjective
    AfterTime         // dopo activationTime secondi dall'inizio missione
};

// ── Conseguenze (doc 25: "cosa cambia se riesce/fallisce") ───────────────
// Il punto di design: un obiettivo NON è una casella da spuntare, è una mossa che
// **cambia la battaglia**. Catturare un posto sblocca uno spawn, distruggere una
// torre disorganizza il nemico, prendere una base gli taglia i rinforzi.
//
// Regola architetturale (ADR-019): ogni conseguenza tocca un sistema DIVERSO, e
// va espressa come DATO dichiarativo che quel sistema legge da `World::battleState`
// — mai come `if (objectiveId == ...)`, o si reintroduce il fork che il framework
// obiettivi esiste per evitare.
//
// I VALORI sono segnaposto da bilanciare provando (direttiva utente 2026-07-16):
// aggiungere un tipo qui = un enum + un lettore nel sistema competente, niente
// tocca gli altri.
enum class ConsequenceType
{
    None = 0,
    BlockEnemyReinforcements,  // il nemico non rimpiazza più le perdite
    EnemyAccuracy,             // moltiplica la precisione nemica (<1 = disorganizzati)
    AllyReinforcements,        // aggiunge riserve alla squadra (value = quante)
    UnlockSpawn                // il team rinasce al post `target` invece che allo spawn mappa
};

struct ConsequenceDef
{
    ConsequenceType type  = ConsequenceType::None;
    float           value = 0.0f;   // parametro (moltiplicatore/quantità); ignorato da alcuni tipi
    std::string     target;         // label del post (UnlockSpawn); vuoto altrimenti
};

// data/objectives/<id>.json — id = filename stem (ADR-001)
struct ObjectiveDef
{
    std::string   id;
    std::string   name;
    ObjectiveType type = ObjectiveType::ReachArea;
    ObjectiveTier tier = ObjectiveTier::Tactical;

    // Bersaglio: zona (x/z/radius) e/o team di riferimento.
    float x = 0, y = 0, z = 0;
    float radius     = 5.0f;
    int   actorTeam  = 1;   // chi deve eseguire (ReachArea/HoldArea)
    int   targetTeam = 2;   // chi va eliminato (EliminateTarget)
    int   count      = 1;   // quante unità (EliminateTarget)
    float holdSeconds = 10.0f;   // HoldAreaForDuration / DefendZone
    // Command post bersaglio, per LABEL (CaptureZone/DefendZone). I post non hanno
    // un id: la label è il loro unico nome autorato nel MapEditor. Il gate ADR-018
    // verifica che esista nella mappa della missione e che sia univoca.
    std::string targetPost;

    ActivationType activation = ActivationType::Immediate;
    std::string    activationObjective;   // AfterObjective
    float          activationTime = 0.0f; // AfterTime

    // Fallimento OPZIONALE: non tutti gli obiettivi falliscono. Il fallimento
    // parziale è ciò che produce decisioni tattiche invece di firefight lineari.
    float timeLimit = 0.0f;   // 0 = non fallisce mai per tempo

    int reward = 0;           // punti comando (doc 26) — economia non ancora attiva
    std::vector<std::string> linkedObjectives;

    // Cosa cambia nella BATTAGLIA quando l'obiettivo si conclude (doc 25).
    // Entrambe opzionali: un obiettivo può non cambiare nulla (resta una casella).
    std::vector<ConsequenceDef> onSuccess;
    std::vector<ConsequenceDef> onFailure;
};

// Regole di missione: dichiarative, mai codice. "Nessun if (missionId == ...)".
enum class MissionRule
{
    AllPrimaryComplete,   // successo: tutti i primari completati
    AnyPrimaryComplete,   // successo: almeno un primario
    AnyPrimaryFailed,     // fallimento: un primario fallito
    TimeLimit             // fallimento: scaduto il tempo di missione
};

// data/missions/<id>.json — id = filename stem (ADR-001)
struct MissionDef
{
    std::string id, name, briefing;
    std::string mapId, modeId;
    std::vector<std::string> primaryObjectives;
    std::vector<std::string> optionalObjectives;

    MissionRule successRule = MissionRule::AllPrimaryComplete;
    MissionRule failureRule = MissionRule::AnyPrimaryFailed;
    float       failureTimeLimit = 0.0f;   // usato da failureRule == TimeLimit

    // Entrambe le regole sono OBBLIGATORIE (doc 25 + gate doc 24): una missione
    // che non sa dire quando è vinta o persa non è una missione. Questi flag
    // dicono se erano presenti nel JSON — il gate rifiuta la missione se mancano.
    bool hasSuccessRule = false;
    bool hasFailureRule = false;
};

// ── ClassDef (14_ClassSystem) ────────────────────────────────────────────
// Composizione autorabile di un loadout: "Trooper", "Heavy Gunner", "Marksman".
// Serve a smettere di cablare le armi una per una e a dare alla progressione
// (Fase 3, doc 27) un'unità di sblocco già esistente — retrofittarla dopo, sotto
// pressione, è precisamente ciò che questo doc vuole evitare.
// data/classes/<id>.json — id = filename stem (ADR-001).
struct ClassDef
{
    std::string id;
    std::string name;
    std::string primaryWeaponId;     // riferimento singolo (dropdown dal registry)
    std::string secondaryWeaponId;   // opzionale (vuoto = nessuna)
    std::vector<std::string> abilityIds;
    // Tag descrittivo ("assault", "support", "sniper"). NON è un enum e NESSUN
    // sistema AI lo consuma: accoppiarlo al comportamento senza un ADR
    // reintrodurrebbe l'accoppiamento che questo sistema esiste per evitare
    // (14_ClassSystem, Out of Scope).
    std::string role;
};

// ── PlayerDef ─────────────────────────────────────────────────────────────
// ATTENZIONE (verificato 2026-07-15): questo tipo è autorato dal BalanceEditor ma
// NESSUN sistema di gioco lo legge — l'hp del giocatore viene da
// MatchSettings.playerHp, non da qui. Vedi KI #35. Non appendere campi nuovi a
// questa struct aspettandoti che abbiano effetto: oggi non ne hanno.
// Stat BASE del personaggio giocabile. Non contiene equipaggiamento né
// visuale: armi, armatura e colori vengono scelti nel PreMatch/loadout.
// data/characters/<id>.json
struct PlayerDef
{
    std::string id;
    std::string name;
    std::string description;

    // Stat base (modificabili dall'editor, indipendenti dall'equipaggiamento)
    float hp          = 100.0f;
    float moveSpeed   = 5.0f;
    float jumpHeight  = 1.0f;   // moltiplicatore impulso base
    float sprintMult  = 1.65f;  // moltiplicatore scatto (= costante storica)
    float armorRating = 1.0f;   // riduzione danni passiva (1=standard)
};

} // namespace mini