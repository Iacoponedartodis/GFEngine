#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
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
    std::string type;    // "shield" | "roll" | "melee" | "jetpack" | "missile" | "command"
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

    // Zoom in mira (ADS): FOV della camera quando si mira (gradi). Più BASSO =
    // più zoom. Default 35 = la vecchia costante fissa `ADS_FOV` → armi esistenti
    // invariate; autorabile per-arma nel Weapon Editor (es. sniper ~15).
    float adsFov              = 35.0f;

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

    // ── Posa in mano (KI #49) ─────────────────────────────────────────────
    // Come l'arma sta nella mano, indipendente da CHI la impugna: la scala
    // compensa la dimensione nativa del mesh (Z-6 nativo minuscolo → ~80,
    // DC-15A → ~0.4), rot/offset allineano il grip. Prima stava su ogni ENTITÀ
    // (weapon_display), tarata su un'arma fissa: quando la classe cambiava
    // l'arma, la posa restava per quella sbagliata → arma gigante/minuscola.
    // `handScale <= 0` = NON autorata → si usa il fallback legacy weapon_display
    // dell'entità (transizione). Il personaggio fornisce solo l'attach point.
    float handScale = 0.0f;
    std::array<float,3> handRot    = {0.0f, 0.0f, 0.0f};
    std::array<float,3> handOffset = {0.0f, 0.0f, 0.0f};
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
    // Quanto insiste su un contatto perduto prima di degradare a Search (doc 16).
    // È una scelta di CARATTERE (un cecchino paziente insegue meno di un'unità
    // aggressiva), quindi vive nel profilo, non fra le costanti globali.
    float huntTimeout      = 20.0f;
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
    // `classId` (ADR-022, metà NPC): se impostato, la CLASSE fornisce loadout,
    // profilo AI e abilità — l'unità smette di ripeterli. È ciò che permette una
    // squadra Trooper+Heavy+Recon che si comporta davvero diversamente (GDD 12.3).
    // Vuoto = i campi sotto valgono come sempre → additivo, non breaking.
    std::string classId;
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
// Che COSA è questo box, secondo l'autore. Non è decorazione: distingue una
// superficie su cui si deve poter salire da un ostacolo su cui non si deve.
// Senza, il gate non può dire se un ripiano irraggiungibile è un difetto o un muro
// (su Training Ground produceva 4 falsi allarmi su cubi 2×2×2 messi come ostacoli).
enum class BoxType : std::uint8_t
{
    Floor,       // pavimento / ripiano: ci si cammina, DEVE essere raggiungibile
    Wall,        // ostacolo verticale: non ci si sale, e va bene così
    Platform,    // ripiano sopraelevato: ci si sale, DEVE avere un accesso
    Cover,       // riparo: ci si nasconde dietro, non ci si sale
    Decoration   // solo visivo: non partecipa alla verità tattica
};

inline BoxType parseBoxType(const std::string& s)
{
    if (s == "floor")      return BoxType::Floor;
    if (s == "platform")   return BoxType::Platform;
    if (s == "cover")      return BoxType::Cover;
    if (s == "decoration") return BoxType::Decoration;
    return BoxType::Wall;   // default storico dell'editor
}

inline const char* boxTypeName(BoxType t)
{
    switch (t) {
        case BoxType::Floor:      return "floor";
        case BoxType::Platform:   return "platform";
        case BoxType::Cover:      return "cover";
        case BoxType::Decoration: return "decoration";
        default:                  return "wall";
    }
}

// Su questo box ci si deve poter salire? Solo pavimenti e piattaforme: un muro
// irraggiungibile non è un difetto, è un muro.
inline bool boxShouldBeReachable(BoxType t)
{
    return t == BoxType::Floor || t == BoxType::Platform;
}

struct MapGeometryBox
{
    float x = 0, y = 0, z = 0;     // centro
    float ry = 0;                  // rotazione attorno a Y (gradi)
    float sx = 2, sy = 2, sz = 2;  // dimensioni totali
    float r = 0.35f, g = 0.32f, b = 0.28f;
    bool  collider = true;
    // Semantica autorata (doc 47 §3.5). L'editor la scriveva già in JSON dal primo
    // giorno — su Training Ground è compilata con criterio (75 floor, 74 wall,
    // 18 cover) — ma il runtime la SCARTAVA al parse. Ora la legge.
    BoxType type = BoxType::Wall;
    // DERIVATO (ADR-053): `true` = generato espandendo una primitiva parametrica,
    // quindi non si salva, si rigenera al load. Come `fromPrefab` per le posizioni.
    bool  fromStructure = false;
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

// ── Bersaglio strategico (doc 25, DestroyTarget) ─────────────────────────
// Struttura statica DISTRUTTIBILE sulla mappa (torre comunicazioni, generatore,
// deposito...). Un obiettivo DestroyTarget la referenzia per LABEL; distruggerla
// completa l'obiettivo e ne scatena la conseguenza (es. nemici disorganizzati →
// enemy_accuracy). È il "bersaglio strategico da distruggere" del GDD.
struct StrategicTargetDef
{
    std::string label = "Bersaglio";
    float x = 0, z = 0;          // posizione XZ
    // Altezza sopra il suolo (m). 0 = appoggiata a terra (comportamento storico,
    // retro-compatibile). >0 = struttura ALZATA (es. torre su una piattaforma):
    // il game mode la piazza a `groundHeightAt(x,z) + y`. È efficace perché la
    // struttura è STATICA (a differenza di un'unità, che cadrebbe per gravità).
    float y = 0.0f;
    float ry = 0.0f;             // rotazione attorno a Y (gradi)
    float hp = 300.0f;           // resistenza: si distrugge a fuoco
    // Fazione proprietaria: prima era CABLATA a 2, quindi una struttura dei
    // CLONI (torre comunicazioni/controllo) sarebbe comunque nata nemica.
    int   team = 2;              // 1 = Repubblica, 2 = Separatisti
    // Ruolo della struttura (doc 34). "generic" = solo bersaglio da distruggere;
    // "comms" = torre di comunicazione: finché è viva la sua fazione comunica
    // bene, quando cade informazioni/ordini/rinforzi RALLENTANO (mai bloccati).
    std::string role = "generic";
    // ── Valore tattico autorato (doc 35) ────────────────────────────────
    // `priority`: quanto la fazione avversaria vuole distruggerla — è il numero
    // che i livelli di comando confrontano con i settori.
    // `engageRadius`: entro quanto un'unità avversaria la ingaggia DI PROPRIA
    // INIZIATIVA. 0 = mai spontaneamente (default conservativo: accendere questo
    // sistema non deve cambiare le mappe già autorate e bilanciate).
    float priority     = 0.5f;
    float engageRadius = 0.0f;
    std::string meshPath;        // vuoto = box di fallback

    // ── Derivazioni geometriche (una sola fonte) ────────────────────────
    // Le usano il game mode (transform + collider), il navmesh (ostacolo) e
    // l'editor (anteprima). Tenerle qui è ciò che impedisce a collisione,
    // navigazione e viewport di divergere.
    // Il box di fallback ha una base di 2.5 m e `meshScale` la MOLTIPLICA: prima
    // per il fallback era ignorata, quindi la scala autorata non aveva alcun
    // effetto in gioco (segnalato dall'utente).
    float visualScale() const
    {
        const float base = meshPath.empty() ? 2.5f : 1.0f;
        return base * ((meshScale > 0.0001f) ? meshScale : 1.0f);
    }
    // Semiassi solidi: autorati se dati, altrimenti dalla scala visiva.
    // L'altezza usa la scala PIENA perché la mesh è alzata di mezza altezza per
    // appoggiare a terra mentre il collider è centrato sul transform.
    void solidHalfExtents(float sc, float& hx, float& hy, float& hz) const
    {
        hx = (halfX > 0.0f) ? halfX : sc * 0.5f;
        hy = (halfY > 0.0f) ? halfY : sc;
        hz = (halfZ > 0.0f) ? halfZ : sc * 0.5f;
    }
    float meshScale = 1.0f;
    // Semiassi di COLLISIONE (m). 0 = derivati dalla scala visiva. Prima queste
    // strutture non avevano collider: le AI e il giocatore ci passavano dentro.
    float halfX = 0.0f, halfY = 0.0f, halfZ = 0.0f;
    std::array<float,3> color = {0.7f, 0.5f, 0.2f};
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

// ── Posizione tattica (ADR-030) ──────────────────────────────────────────
// UN SOLO tipo per "un posto che conta": sostituisce CoverPointDef (ADR-026) e
// TacticalPointDef (ADR-027), che erano due concetti paralleli per la stessa
// cosa. Il `role` è DESCRITTIVO (aiuta l'authoring e le query per ruolo); le
// CAPACITÀ stanno nei campi: una copertura è una posizione con protection > 0,
// e una `vantage` che ripara vale anche come copertura senza casi speciali.
// Il settore di tiro (M1, doc 33 §5-bis) si aggiunge qui — una volta sola.
struct TacticalPositionDef
{
    float x = 0, y = 0, z = 0;
    float facingDeg  = 0.0f;      // fronte / direzione d'interesse (gradi, yaw)
    std::string role = "cover";   // cover | vantage | defensive | chokepoint | observation
    float height     = 1.0f;      // altezza della copertura (peek-over vs peek-around)
    float protection = 0.5f;      // quanto ripara (0..1); 0 = non ripara affatto
    bool  canShoot   = true;      // si può fare fuoco da qui (vs solo nascondersi)
    float importance = 0.5f;      // PESO tattico relativo (≥0, senza tetto; più alto = più
                                  // importante). NON [0,1]: l'autore lo grada per creare peso
                                  // (KI #81). I consumatori lo usano linearmente.
    float radius     = 4.0f;      // area d'influenza (defensive/chokepoint)

    // ── Settore di tiro (ADR-031) ─────────────────────────────────────────
    // Cosa questa posizione BATTE: arco centrato su facingDeg + gittata utile.
    // È ciò che la rende una posizione da cui ATTACCARE e non solo un riparo:
    // permette la query "posizione coperta da cui colpisco quella zona".
    // Filtro geometrico ed economico (niente raycast); la visibilità reale
    // contro gli ostacoli arriverà col precalcolo (M4, doc 33 §5-bis).
    float fireArcDeg = 120.0f;    // ampiezza totale del settore (gradi)
    float fireRange  = 25.0f;     // gittata utile dalla posizione (m)
    // Provenienza (ADR-048): `true` = generata espandendo un PREFAB, quindi DERIVATA
    // (non si salva, si rigenera al load). `false` = piazzata a mano nell'editor, quindi
    // AUTORATA (si salva). Serve a poter aggiornare un prefab senza cancellare il lavoro
    // manuale — è la distinzione che rende sicuro rigenerare.
    bool  fromPrefab = false;
    // DERIVATO (doc 41 B3): la posizione ha qualcosa SOPRA (soffitto/impalcato/tettoia)
    // entro `OVERHEAD_PROBE_HEIGHT`. Calcolato al load come il grafo dei link, mai
    // autorato, mai salvato → non può diventare stale.
    // ATTENZIONE al nome: è "coperto dall'alto", NON "interno". Un sottopasso lo è
    // quanto un bunker. L'interno vero richiede la CHIUSURA (pareti attorno) ed è
    // un'analisi diversa (stanze, B8): non usare questo campo per dedurlo.
    bool  hasOverhead = false;
};

// ── Settore / Combat Area (ADR-034) ──────────────────────────────────────
// Zona con significato tattico: è il livello su cui ragiona il comandante
// (importanza + chi la controlla + quanto è contesa) invece di guardare solo
// l'owner dei command post. Autorato a mano: i settori sono pochi e sono SCELTE
// DI DESIGN, non un dato derivabile. Lo STATO (presenze, controllo, pressione)
// non sta qui: è stato di partita e vive in World::sectorStates.
struct SectorDef
{
    std::string label = "Settore";
    float x = 0, z = 0;
    float radius     = 12.0f;   // area d'influenza (XZ)
    float importance = 0.5f;    // PESO strategico relativo (≥0, senza tetto; più alto = più
                                // conteso/prioritario). NON [0,1] (KI #81): l'autore lo grada.
};

// Percorso di pattuglia con nome (riusabile da più squadre in futuro).
struct PatrolRouteDef
{
    std::string id = "route";
    std::vector<std::array<float,3>> points;   // ordinati
};

// ── PREFAB tattico (ADR-048) ───────────────────────────────────────────────
// Un asset che porta con sé il proprio SIGNIFICATO TATTICO, non solo la mesh.
// Motivo: Training Ground ha 167 posizioni piazzate A MANO; le mappe profonde ne
// richiederebbero 1000+, che non scala per un team di una persona. E la generazione
// automatica dalla geometria è già stata provata e rimossa (ADR-026) perché produceva
// posizioni insensate. La terza via: **si autora una volta per ASSET, si moltiplica
// per ISTANZA**. Piazzare un bunker porta con sé le sue coperture, già pensate.
//
// La mesh è SOLO visiva: collisione, navmesh e LOS restano sui box (ADR-047), che è
// ciò che rende l'analisi tattica veloce e analitica.
// Coordinate di `collision`/`tactical`/`indoor`: LOCALI al prefab; il motore le
// trasforma all'espansione (posizione + rotazione dell'istanza).
// id = filename stem (ADR-001): data/prefabs/<id>.json
struct PrefabDef
{
    std::string id;
    std::string name;
    std::string meshPath;                       // visivo (GLB), opzionale
    std::vector<MapGeometryBox>       collision; // proxy: la verità fisica/tattica
    std::vector<TacticalPositionDef>  tactical;  // significato autorato una volta
    std::vector<std::string>          tags;      // "building", "hard_cover", ...
};

// ── ISTANZA di prefab in una mappa (ADR-048) ───────────────────────────────
// La mappa non duplica i dati del prefab: ne referenzia l'id e la trasformazione.
// L'espansione avviene AL LOAD → i dati espansi sono DERIVATI (mai salvati), come il
// grafo delle coperture (ADR-033): non possono diventare stale rispetto al prefab.
struct PrefabInstanceDef
{
    std::string prefabId;
    float x = 0, y = 0, z = 0;
    float ry = 0;                 // rotazione attorno a Y (gradi)
};

// Area esposta/pericolosa (hint morbido, NON un collider).
struct DangerZoneDef
{
    float x = 0, y = 0, z = 0;
    float radius      = 4.0f;
    float dangerLevel = 0.5f;   // 0..1, semantica del consumatore AI
};

// ── Comandante di mappa (Droide Tattico serie T — ADR-024, doc 32) ──────────
// UNO per mappa: l'autorità strategica separatista. NON è un tipo del roster
// (`enemy_types` ne spawnerebbe molti come truppa): è un'unità SINGOLA, piazzata
// in una posizione protetta nelle retrovie, che dirige i droidi (World::enemyCommand)
// e si difende soltanto (spawna stationary). `unit` vuoto = nessun comandante.
// ── CommanderDef (ADR-044): il comandante NON è una classe ─────────────────
// Il Droide Tattico non è una professione istanziabile (ADR-023): è un'unità
// UNICA a ruolo strategico — non combatte (si difende soltanto), sta al sicuro,
// dà ordini, e ucciderlo ha una conseguenza (come una torre). Vive quindi in una
// definizione propria (`data/commanders/<id>.json`), fuori dal roster delle
// classi giocabili. Riusa un `baseEntity` per il CORPO (mesh/hitbox/proiettile),
// applicandovi sopra i propri override; il `CommanderComponent` è implicito (non
// serve un'ability "command"). data/commanders/<id>.json
struct CommanderDef
{
    std::string id;
    std::string name;
    std::string baseEntity;          // corpo (EnemyDef): mesh, hitbox, proiettile
    std::string selfDefenseWeapon;   // arma per difendersi se attaccato
    std::string aiProfile;           // profilo AI (di norma poco aggressivo)
    std::vector<std::string> abilities;  // deve includerne una di tipo "command"
                                         // (→ CommanderComponent); es. anche "Shield"
    float hp        = 120.0f;         // ASSOLUTI (non un moltiplicatore): è un obiettivo
    float speedMult = 0.9f;           // sul corpo base (si muove poco, nel leash)
    float meshScale = 1.0f;
    std::array<float,3> colorMult = {1.0f, 1.0f, 1.0f};   // tinta sul corpo
    int   team      = 2;             // 1 Repubblica / 2 Separatisti
};

struct CommanderSpawnDef
{
    std::string unit;            // id CommanderDef (nuovo) o classe legacy (fallback)
    float x = 0.0f, z = 0.0f;    // posizione strategica nelle retrovie (XZ)
    // Raggio di LEASH (ADR-041): area circolare da cui il comandante non esce. Si
    // muove al suo interno per difendersi/coprirsi, mai fuori. 0 = fermo sul posto
    // (comportamento legacy `stationary`): retrocompatibile con le mappe esistenti.
    float leashRadius = 0.0f;
};

// ── MapDef ────────────────────────────────────────────────────────────────
// data/maps/<id>.json
// ── PRIMITIVA PARAMETRICA di costruzione (ADR-053, doc 47 §3) ───────────────
// La RICETTA di una forma, non la forma: l'autore dichiara "da qui, salendo di 3 m,
// larga 4", e il motore emette i `MapGeometryBox` rispettando `STEP_HEIGHT`.
// **L'alzata sbagliata diventa inesprimibile** — è il rimedio strutturale a KI #95.
// I parametri si SALVANO; i box espansi no, si rigenerano al load come i prefab
// (ADR-048) e come ogni dato derivato (ADR-033).
// L'espansione vive in `mini/game/MapStructures.hpp`, una sola implementazione per
// registry, editor e gate.
// La libreria (2026-08-05). I primi quattro sono del primo giro; gli altri cinque
// nascono da cosa serve DAVVERO a una mappa complessa, non da completismo:
//   · Switchback — una salita di 8 m occupa **12 m** di sviluppo diritto alle nostre
//     metriche: dentro un edificio è insostenibile. Il pianerottolo la dimezza, ed è
//     ciò che rende possibile la verticalità in uno spazio denso.
//   · Room e Doorway — l'apertura in un muro è l'elemento più ripetuto di qualunque
//     interno, e il guscio di stanza è il modulo canonico dei kit modulari (doc 47
//     §2.3). Farli a mano significa tre box per porta con la misura sbagliata.
//   · Catwalk — la passerella sopraelevata è un CORRIDOIO IN QUOTA: non decorazione,
//     ma una corsia tattica con dominio sul piano di sotto.
//   · Barricade — la linea di copertura è la struttura da campo di battaglia per
//     eccellenza, e produce box di tipo `cover` che la derivazione dei metadata
//     (doc 46) consuma direttamente.
enum class StructureKind : std::uint8_t
{
    Stair, Ramp, Wall, Platform,
    Switchback,   // scala con pianerottolo: dimezza l'ingombro in pianta
    Doorway,      // muro CON apertura (porta o finestra)
    Room,         // guscio: pavimento + 4 muri + soffitto opzionale, con aperture
    Catwalk,      // passerella sopraelevata, parapetti opzionali
    Barricade     // linea di coperture a intervalli
};

// I parametri dimensionali di una ricetta, enumerati (ADR-055). Servono a parlare
// di una misura **senza sapere quale primitiva sia**: l'editor la mostra, il tipo la
// vincola, il clamp la applica — tutti dalla stessa tabella (`mapstructures::paramsOf`).
// Senza questo elenco ogni parametro andrebbe ripetuto a mano in tre posti, ed è così
// che i tre divergono.
enum class StructureParam : std::uint8_t
{
    Rise, Width, Riser, Tread,
    Length, Height, Thickness,
    OpenW, OpenH, OpenSill, OpenOff,
    FlightRise, Spacing,
    SizeX, SizeZ, BaseY,
    // Quota del piano calpestabile sopra la base. Per una piattaforma o una
    // passerella è IL parametro che le rende quello che sono: senza, un tipo
    // "passerella in quota" non è esprimibile e nasce appoggiato a terra.
    Elev,
    Count
};

// Vincolo autorato su un parametro (ADR-055). `min`/`max` a 0 = non autorato, vale
// solo il **pavimento fisico** del codice, che un tipo non può allentare.
struct StructureParamRule
{
    bool  editable = true;
    float min = 0.0f;
    float max = 0.0f;
};

struct StructurePart;   // definita più sotto: contiene uno StructureDef per valore

struct StructureDef
{
    StructureKind kind = StructureKind::Stair;
    std::string   label;
    // Tipo di appartenenza (ADR-055). VUOTO = comportamento di prima, con i minimi
    // per primitiva: il fallback documentato della transizione (CLAUDE.md §2).
    std::string   type;

    // ── PARTI LOCALI: questa istanza è una versione MODIFICATA del suo tipo ──
    // (ADR-056 rivisto 2026-08-10, richiesta dell'utente: quattro Tactic Bunker in
    // mappa, e su UNO serve una modifica specifica.)
    // Non vuoto = si espandono QUESTE invece delle parti del tipo. Il campo `type`
    // resta: serve a dire da cosa deriva ("Tactic Bunker, modificata") e a poterci
    // tornare. Le modifiche vivono sull'ISTANZA, cioè nel file della mappa, dove
    // stanno le decisioni che valgono per quel punto e basta.
    //
    // Perché non creare invece un tipo nuovo in libreria: perché la libreria
    // diventerebbe un elenco di varianti quasi identiche di cui nessuno ricorda la
    // differenza — lo stesso motivo per cui i riferimenti hanno sostituito le copie.
    // È il modello degli override d'istanza di Unity, e per la stessa ragione.
    std::vector<StructurePart> localParts;
    [[nodiscard]] bool isModifiedInstance() const
    { return !type.empty() && !localParts.empty(); }

    // Origine e orientamento. `ry` = direzione di SALITA (scala/rampa), di sviluppo
    // (muro), del ripiano (piattaforma).
    float x = 0, y = 0, z = 0;
    float ry = 0;

    // Scala / rampa: `y` è il piede della salita.
    float rise  = 2.0f;
    float width = 2.0f;              // larghezza: libera, non tocca i gradini
    float riser = 0.0f;              // 0 = alzata normativa del tipo
    // Pedata: allunga o accorcia la scala **senza cambiare l'alzata**. È la leva per
    // "questa scala è troppo ripida/troppo lunga" che non può rompere nulla — il
    // numero di gradini dipende solo dal dislivello.
    float tread = 0.0f;              // 0 = pedata normativa del tipo

    // Muro: `x,y,z` = centro della base.
    float length    = 4.0f;
    float height    = 0.0f;          // 0 = WALL_HEIGHT normativa
    float thickness = 0.0f;          // 0 = spessore normativo

    // Apertura (Doorway, e ogni lato aperto di Room). 0 = misure normative.
    // `openSill` > 0 la trasforma in FINESTRA: sotto resta il parapetto, che è
    // copertura vera — una finestra non è un buco, è un riparo con la vista.
    float openW    = 0.0f;
    float openH    = 0.0f;
    float openSill = 0.0f;
    float openOff  = 0.0f;           // scostamento dal centro del muro

    // Vano scala: dislivello massimo di UNA rampa (0 = 3,0 m, un piano).
    // Il numero di rampe si ricava dal dislivello totale, e l'ingombro in pianta
    // resta lo STESSO comunque: è ciò che rende il vano scala impilabile in una
    // torre invece che un pezzo unico da riprogettare a ogni altezza.
    float flightRise = 0.0f;

    // Barricata: passo fra un elemento e il successivo (0 = continua).
    float spacing = 0.0f;

    // Guscio e passerella.
    bool ceiling = false;            // Room: soffitto chiuso
    bool railing = false;            // Catwalk/Platform: parapetto

    // Piattaforma: `x,y,z` = centro del ripiano, `y` = quota CALPESTABILE.
    float sizeX = 6.0f, sizeZ = 6.0f;
    float baseY = 0.0f;              // quota da cui partono gli accessi
    // ACCESSI OBBLIGATORI: la piattaforma dichiara da dove ci si sale e le scale
    // nascono con lei. È il punto in cui "percorribile per costruzione" smette di
    // essere uno slogan: il difetto non si verifica dopo, si rende impossibile prima.
    // Ordine dei lati nel frame locale: -Z, +Z, -X, +X.
    bool access[4] = { true, false, false, false };

    std::array<float,3> color = { 0.35f, 0.32f, 0.28f };
};

// Una PARTE di un assemblaggio (ADR-056): o una primitiva parametrica, o un box
// libero. Le coordinate della parte sono LOCALI all'origine dell'assemblaggio, e
// vengono trasformate dalla posa dell'istanza al momento dell'espansione.
//
// Perché due casi e non uno: le primitive garantiscono le misure (un'alzata
// sbagliata resta inesprimibile, ADR-053), ma non esprimono tutto — un parapetto
// storto, un contrafforte, una feritoia sono box. Ammettere solo primitive avrebbe
// reso inesprimibili proprio le "strutture un po' più complesse" che sono il motivo
// per cui l'assemblaggio esiste.
struct StructurePart
{
    bool             isBox = false;   // false = primitiva
    StructureDef     prim;            // valida se !isBox (x,y,z,ry = posa LOCALE)
    MapGeometryBox   box;             // valida se isBox  (idem)
    std::string      label;
    // RIFERIMENTO a un altro tipo composito (ADR-056 rivisto 2026-08-08 su richiesta
    // dell'utente: *"preferirei le lasciassi normali"*). Non vuoto = questa parte è
    // un'altra struttura intera, non una copia delle sue parti: modificando
    // l'originale cambiano anche gli usi.
    // La posa sta in `prim.x/y/z/ry` — riusare quei campi evita un terzo blocco di
    // coordinate che poi qualcuno dimentica di leggere o di scrivere.
    std::string      refType;
    [[nodiscard]] bool isRef() const { return !refType.empty(); }

    // Le PARTI LOCALI di un riferimento: stessa idea di `StructureDef::localParts`,
    // un livello più dentro. Non vuoto = questa copia della struttura riferita è
    // stata modificata **qui e solo qui** (comando "Isola e modifica"). Il tipo
    // riferito resta scritto: si sa da cosa deriva e ci si può tornare.
    std::vector<StructurePart> localParts;
    [[nodiscard]] bool isModifiedRef() const
    { return !refType.empty() && !localParts.empty(); }
};

// Un TIPO di struttura: preset nominato di una primitiva, con i suoi vincoli
// (ADR-055, doc 48). id = filename stem (ADR-001). Sta DOPO `StructureDef` perché lo
// contiene per valore: i predefiniti sono una ricetta completa, non un frammento.
//
// ASSEMBLAGGIO (ADR-056): se `parts` non è vuoto il tipo è un assemblaggio e
// `defaults` non viene espanso. Se è vuoto, il tipo resta quello di ADR-055 — una
// sola primitiva — e nulla cambia per i tipi già scritti.
//
// ANNIDAMENTO (ADR-056 rivisto 2026-08-08): un assemblaggio PUÒ contenere altri
// assemblaggi, per riferimento (`StructurePart::refType`). Il divieto originale
// veniva dalle famiglie annidate di Revit ed era motivato — l'annidamento moltiplica
// i modi in cui il navmesh si rompe — ma il rimedio (copiare le parti) creava una
// libreria di varianti divergenti: correggere una torre non correggeva i suoi usi.
// Il rischio si paga in tre modi invece che col divieto:
//   · catena anti-ciclo + tetto `kMaxAssemblyDepth` in `expandAssembly`;
//   · si possono riferire solo composite già VERIFICATE (navmesh percorribile);
//   · il gate `--validate` segnala riferimenti rotti, cicli e annidamento eccessivo.
struct StructureTypeDef
{
    std::string   id;
    std::string   label;
    std::string   note;
    // Categoria libera, decisa dall'autore ("Torri", "Difese", "Interni"...). Serve
    // a raggruppare la libreria quando i tipi saranno decine. Vuota = "Senza
    // categoria". Libera e non enum: un elenco fisso costringerebbe a ricompilare
    // per aggiungere un raggruppamento, cioè a chiedere a me un cambio di codice per
    // una decisione che è di chi costruisce.
    std::string   category;
    StructureKind kind = StructureKind::Stair;
    StructureDef  defaults;      // valori con cui nasce un'istanza (tipo semplice)
    std::vector<StructurePart> parts;   // non vuoto = ASSEMBLAGGIO (ADR-056)
    std::array<StructureParamRule, (std::size_t)StructureParam::Count> rules{};
    // `verified`: ha superato la verifica navmesh sulla struttura ISOLATA. Un tipo non
    // verificato si salva lo stesso, ma resta marcato tale nella libreria — un dato che
    // può essere sbagliato in silenzio è ciò che il gate esiste per impedire (ADR-050).
    bool verified = false;
};

struct MapDef
{
    std::string id;
    std::string name;
    std::string meshPath;
    std::string metadataPath;
    // Nessun navmeshPath: ADR-004 — la navmesh la GENERA Recast a runtime dai box
    // di `geometry`, non si carica da file. Il campo esisteva scritto-e-mai-letto
    // (BalanceEditor lo salvava, nessun loader lo rileggeva): rimosso 2026-07-16.
    std::array<float,3> spawnTeam1 = {0.f, 0.86f,  8.f};
    std::array<float,3> spawnTeam2 = {0.f, 0.86f, -8.f};
    // Punti di spawn AGGIUNTIVI per fazione (multi-spawn): se non vuoti, le unità AI
    // si distribuiscono su questi punti → migliore distribuzione iniziale sulla mappa
    // (es. un gruppo per corsia). Vuoti = spawn singolo su spawnTeamN (retrocompat).
    std::vector<std::array<float,3>> spawnPointsTeam1, spawnPointsTeam2;
    // Istanze di prefab (ADR-048): referenziate, non duplicate. Espanse AL LOAD in
    // `geometry` e `tacticalPositions` → i dati espansi sono DERIVATI e non si salvano.
    std::vector<PrefabInstanceDef> prefabs;
    // Primitive parametriche (ADR-053): scale, rampe, muri, piattaforme-con-accessi.
    // Espanse AL LOAD in `geometry` → i box che ne escono sono DERIVATI e non si
    // salvano mai. Le mappe che non ne hanno funzionano identiche: è una sezione
    // nuova, non una sostituzione dei box a mano (fallback documentato, CLAUDE.md §2).
    std::vector<StructureDef> structures;
    int maxTickets = 10;
    int enemyCount = 6;
    int allyCount  = 1;
    std::vector<std::string> enemyTypes;
    std::vector<std::string> allyTypes;
    std::vector<MapGeometryBox> geometry;
    std::vector<CommandPostDef> commandPosts;
    std::vector<StrategicTargetDef> strategicTargets;   // DestroyTarget (doc 25)

    // Map Metadata (15_MapMetadata) — opzionali, vuoti finché non autorati
    std::vector<PatrolRouteDef> patrolRoutes;
    std::vector<DangerZoneDef>  dangerZones;
    // Posizioni tattiche unificate (ADR-030): coperture, punti dominanti,
    // difensivi, strettoie, osservazione. Sostituisce coverPoints+tacticalPoints.
    std::vector<TacticalPositionDef> tacticalPositions;

    // Grafo "chi copre chi" (ADR-032) — DATO DERIVATO, non autorato e non salvato:
    // `positionCovers[i]` = indici delle posizioni coperte da `tacticalPositions[i]`
    // (dentro settore + gittata + linea di tiro libera). Ricalcolato a ogni load da
    // `worldintel::buildTacticalLinks`, quindi non può diventare incoerente.
    std::vector<std::vector<int>> positionCovers;

    // Esposizione (ADR-033) — DERIVATA invertendo il grafo: `positionExposure[i]`
    // è la frazione (0..1) delle altre posizioni che possono BATTERE la posizione i.
    // Alta = allo scoperto, bassa = riparata dagli angoli di tiro della mappa.
    // Serve a preferire approcci coperti e a segnalare al designer i punti esposti.
    std::vector<float> positionExposure;

    // Settori / Combat Areas (ADR-034) — autorati, opzionali. Vuoto = il
    // comandante usa la regola precedente (post più vicino).
    std::vector<SectorDef> sectors;

    // Veicoli in mappa (19_Vehicles, Fase A) — opzionale
    std::vector<VehicleSpawnDef> vehicleSpawns;

    // Comandante strategico (ADR-024, doc 32) — opzionale, uno per mappa
    CommanderSpawnDef commander;
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
    // Bersaglio strategico da distruggere, per LABEL (DestroyTarget). Stessa
    // convenzione di targetPost: il gate verifica che esista nella mappa.
    std::string targetStructure;

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
    // Il COMPORTAMENTO: è ciò che rende la classe una **professione** e non un
    // pacchetto di armi (GDD 12.1: "professioni militari, non semplici categorie
    // di armi"; 12.3: le classi definiscono il comportamento IA degli NPC, e una
    // squadra mista "deve comportarsi diversamente" da una monoclasse).
    // Vuoto = l'unità tiene il proprio profilo → additivo, niente cambia.
    std::string aiProfileId;
    // Tag descrittivo ("assault", "support", "sniper"). Ancora NON consumato da
    // nessun sistema: diventerà un enum quando il SquadSystem assegnerà i task
    // per ruolo (ADR-022).
    std::string role;

    // ── Corpo + moltiplicatori (ADR-023) ─────────────────────────────────────
    // `baseEntityId` = l'ENTITÀ-corpo da cui la classe prende modello, hitbox,
    // stat base, attach/metadata. Con questo, una classe è istanziabile da sola
    // (è un tipo-unità nei roster): Heavy/Sniper/Medic = un corpo clone + classe,
    // non entità separate. Vuoto = classe usata solo "sopra" un'entità esistente
    // (modello legacy, additivo).
    std::string baseEntityId;
    // Moltiplicatori applicati alle stat BASE del corpo (default 1.0 = invariato).
    float hpMult     = 1.0f;
    float speedMult  = 1.0f;
    float damageMult = 1.0f;
    // Tinta di colore della classe: MOLTIPLICA il colore del corpo (default
    // {1,1,1} = invariato). Serve a distinguere a colpo d'occhio le professioni
    // che condividono lo stesso corpo (es. Sniper verdino, Heavy ambrato).
    std::array<float,3> colorMult = {1.0f, 1.0f, 1.0f};
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