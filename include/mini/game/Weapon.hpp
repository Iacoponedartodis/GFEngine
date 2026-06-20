#pragma once

namespace mini
{

// Descrive le statistiche di un'arma.
// Ispirato al sistema Battlefront: le armi non hanno munizioni tradizionali
// ma si riscaldano. Se raggiungono il 100% di calore, entrano in overheat
// e devono raffreddarsi completamente prima di sparare di nuovo.
struct Weapon
{
    // ── Identità ─────────────────────────────────────────────────────
    const char* name      = "DC-15A";

    // ── Statistiche di fuoco ─────────────────────────────────────────
    float fireRate        = 4.0f;    // colpi al secondo
    float bulletSpeed     = 18.0f;   // m/s
    float bulletDamage    = 25.0f;   // HP per colpo
    float bulletLifetime  = 3.0f;    // secondi prima che il proiettile scompaia
    float bulletScale     = 0.12f;   // dimensione visiva del proiettile

    // Colore proiettile (blu clone)
    float bulletR = 0.3f, bulletG = 0.6f, bulletB = 1.0f;

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

// ── Preset armi ──────────────────────────────────────────────────────────

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

inline Weapon makeBlasterPistol()
{
    return Weapon{
        .name = "DC-17 Pistol",
        .fireRate = 6.0f, .bulletSpeed = 20.0f, .bulletDamage = 17.0f,
        .bulletLifetime = 2.5f, .bulletScale = 0.08f,
        .bulletR = 0.3f, .bulletG = 0.65f, .bulletB = 1.0f,
        .heatPerShot = 0.10f, .cooldownRate = 0.45f, .overheatPenalty = 1.2f
    };
}

inline Weapon makeHeavyBlaster()
{
    return Weapon{
        .name = "Z-6 Rotary",
        .fireRate = 11.0f, .bulletSpeed = 20.0f, .bulletDamage = 13.0f,
        .bulletLifetime = 2.0f, .bulletScale = 0.09f,
        .bulletR = 0.3f, .bulletG = 0.65f, .bulletB = 1.0f,
        .heatPerShot = 0.03f, .cooldownRate = 0.20f, .overheatPenalty = 3.0f
    };
}

inline Weapon makeSniperRifle()
{
    return Weapon{
        .name = "DC-15x Sniper",
        .fireRate = 1.0f, .bulletSpeed = 35.0f, .bulletDamage = 70.0f,
        .bulletLifetime = 4.0f, .bulletScale = 0.06f,
        .bulletR = 0.2f, .bulletG = 0.5f, .bulletB = 1.0f,
        .heatPerShot = 0.30f, .cooldownRate = 0.25f, .overheatPenalty = 2.5f
    };
}

} // namespace mini