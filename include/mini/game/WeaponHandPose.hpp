#pragma once
// ── WeaponHandPose — LA formula "come l'arma sta in mano", in un posto solo ───
//
// PERCHÉ È UN HEADER A SÉ. La formula serve a tre consumatori:
//   · il RUNTIME (`weaponattach::resolve`, che disegna l'arma in partita);
//   · l'anteprima dell'**Entity Editor** (arma in mano su un'unità);
//   · l'anteprima del **Weapon Editor** (si tara la posa guardandola).
// Prima esisteva due volte — runtime ed Entity Editor — tenute allineate da un
// commento *"DEVE combaciare con..."*. Un commento non è un vincolo: bastava
// ritoccarne una perché l'anteprima smettesse di dire la verità sul gioco, che è
// il modo peggiore di rompere uno strumento di authoring (mostra una cosa, ne
// salva un'altra). Aggiungendo il terzo consumatore la copia andava chiusa.
//
// Sta qui e non in `WeaponAttach.hpp` perché quello include `IGameMode` (MeshCache)
// e il registry: pesi che l'editor non deve tirarsi dietro per fare una matrice.
// Qui dentro: solo glm.
//
// Parametri SCIOLTI, non `EnemyDef`/`WeaponDef`: gli editor lavorano su valori in
// corso di modifica, non su definizioni salvate — passare le def costringerebbe a
// costruirne di finte a ogni frame.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mini::weaponattach
{

struct HandPose
{
    glm::vec3 hand   {0.0f};   // attach point della mano, nel model space del personaggio
    glm::vec3 offset {0.0f};   // offset della posa
    glm::vec3 rot    {0.0f};   // rotazione della posa (gradi, ordine Y·X·Z)
    glm::vec3 grip   {0.0f};   // punto di presa sull'arma (model space dell'arma)
    float scale      = 1.0f;   // compensa la dimensione NATIVA del mesh (Z-6 ~80, DC-15A ~0.4)
    float charScale  = 1.0f;   // mesh_scale del personaggio: l'arma NON deve ereditarla
    float baseRotX   = 0.0f;   // correzione canonica del mesh arma (WeaponDef.meshRotX/Y)
    float baseRotY   = 0.0f;
};

// Matrice nel MODEL SPACE del personaggio: al render si compone con la matrice
// modello dell'entità.  local = T(mano+offset) · R(rot) · S(scala) · baseFix · T(-grip)
inline glm::mat4 handLocal(const HandPose& p)
{
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(p.rot.y), {0,1,0})
                      * glm::rotate(glm::mat4(1.0f), glm::radians(p.rot.x), {1,0,0})
                      * glm::rotate(glm::mat4(1.0f), glm::radians(p.rot.z), {0,0,1});
    // La local si compone con la matrice del PERSONAGGIO, che include la sua
    // mesh_scale: senza compensarla, su un clone scalato 0.011 l'arma diventa
    // microscopica.
    const float cs  = (p.charScale > 0.0001f) ? p.charScale : 1.0f;
    const float eff = p.scale / cs;
    const glm::mat4 baseFix =
          glm::rotate(glm::mat4(1.0f), glm::radians(p.baseRotY), {0,1,0})
        * glm::rotate(glm::mat4(1.0f), glm::radians(p.baseRotX), {1,0,0});
    return glm::translate(glm::mat4(1.0f), p.hand + p.offset)
         * R
         * glm::scale(glm::mat4(1.0f), glm::vec3(eff))
         * baseFix
         * glm::translate(glm::mat4(1.0f), -p.grip);
}

} // namespace mini::weaponattach
