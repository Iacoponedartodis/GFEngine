#pragma once
#include <string>

namespace mini
{

// Descrive le statistiche di un'arma.
// Ispirato al sistema Battlefront: le armi non hanno munizioni tradizionali
// ma si riscaldano. Se raggiungono il 100% di calore, entrano in overheat
// e devono raffreddarsi completamente prima di sparare di nuovo.
struct Weapon
{
    // ── Identità ─────────────────────────────────────────────────────
    std::string name = "DC-15A";

    // Viewmodel (arma visibile in prima persona, Todo #11):
    // risolto dal WeaponDef; vuoto = nessun modello da mostrare.
    std::string meshPath;
    float meshScale = 0.8f;
    float meshRotY  = 0.0f;   // raddrizzamento yaw del GLB (dal WeaponDef)

    // ── Statistiche di fuoco ─────────────────────────────────────────
    float fireRate        = 4.0f;    // colpi al secondo
    float bulletSpeed     = 18.0f;   // m/s
    float bulletDamage    = 25.0f;   // HP per colpo
    float bulletLifetime  = 3.0f;    // secondi prima che il proiettile scompaia
    float bulletScale     = 0.12f;   // dimensione visiva del proiettile

    // Colore proiettile (blu clone)
    float bulletR = 0.3f, bulletG = 0.6f, bulletB = 1.0f;

    // ── Precisione e gittata (R1: consumati da PlayerController) ─────
    // Dispersione per stato (radianti circa, come da WeaponDef/editor)
    float baseSpread   = 0.02f;   // fermo
    float adsSpread    = 0.005f;  // in mira (tasto destro)
    float moveSpread   = 0.06f;   // in movimento
    float sprintSpread = 0.14f;   // in corsa
    float jumpSpread   = 0.20f;   // in aria
    float effectiveRange = 20.0f; // oltre ~2x il colpo si esaurisce

    // ── Sistema calore ───────────────────────────────────────────────
    float heatPerShot     = 0.12f;   // calore aggiunto per colpo (0-1)
    float cooldownRate    = 0.35f;   // calore perso al secondo (in idle)
    float overheatPenalty = 1.8f;    // secondi di lockout dopo overheat

    // ── Stato runtime (gestito dal gioco, non settare a mano) ────────
    float heat            = 0.0f;    // 0.0 = freddo, 1.0 = massimo
    float cooldownTimer   = 0.0f;    // tempo dalla fine dell'ultimo sparo
    float overheatTimer   = 0.0f;    // >0 = bloccata per surriscaldamento
    bool  overheated      = false;   // true = in lockout

    // ── Metodi ───────────────────────────────────────────────────────

    // Aggiorna calore ogni frame
    void update(float dt)
    {
        if (overheated)
        {
            overheatTimer -= dt;
            heat -= (1.0f / overheatPenalty) * dt; // raffredda durante lockout
            if (heat < 0.0f) heat = 0.0f;
            if (overheatTimer <= 0.0f)
            {
                overheated = false;
                overheatTimer = 0.0f;
                heat = 0.0f;
            }
            return;
        }

        cooldownTimer += dt;
        // Raffreddamento passivo (inizia subito)
        if (heat > 0.0f)
        {
            heat -= cooldownRate * dt;
            if (heat < 0.0f) heat = 0.0f;
        }
    }

    // Tenta di sparare. Ritorna true se il colpo è partito.
    bool tryFire()
    {
        if (overheated) return false;

        const float interval = 1.0f / fireRate;
        if (cooldownTimer < interval) return false;

        cooldownTimer = 0.0f;
        heat += heatPerShot;

        if (heat >= 1.0f)
        {
            heat = 1.0f;
            overheated = true;
            overheatTimer = overheatPenalty;
        }

        return true;
    }

    // Reset completo (nuova partita)
    void reset()
    {
        heat = 0.0f;
        cooldownTimer = 1.0f; // permette sparo immediato
        overheatTimer = 0.0f;
        overheated = false;
    }
};

// ── Fallback di ULTIMA istanza ───────────────────────────────────────────
// Usato solo quando l'arma richiesta non esiste nel registry (id orfano).
// NON è un'arma di gioco: le armi vere vivono in data/weapons/ (ADR-001).
// I preset gemelli (pistol/heavy/sniper) sono stati rimossi (KI #26):
// duplicavano i JSON e mascheravano id mancanti con stats stantie.

inline Weapon makeBlasterRifle()
{
    return Weapon{
        .name = "DC-15A Blaster",
        .fireRate = 4.5f, .bulletSpeed = 25.0f, .bulletDamage = 25.0f,
        .bulletLifetime = 3.0f, .bulletScale = 0.12f,
        .bulletR = 0.3f, .bulletG = 0.65f, .bulletB = 1.0f,
        .heatPerShot = 0.12f, .cooldownRate = 0.30f, .overheatPenalty = 2.0f
    };
}

} // namespace mini