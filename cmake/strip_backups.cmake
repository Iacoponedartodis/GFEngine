# Rimuove i backup *.bak dalla data/ copiata nell'output.
#
# I .bak li genera saveJsonRMW (ADR-010) accanto al file salvato, quindi vivono
# nella data/ SORGENTE — che è dove l'editor scrive davvero (editorDataPath()
# risale a ../../../data). `cmake -E copy_directory` non sa escludere nulla, così
# se li portava dietro a ogni build: rumore nell'output e, il giorno che si
# impacchetta il gioco, backup di authoring spediti ai giocatori.
#
# Gira a BUILD time (non a configure time): deve vedere la cartella appena
# copiata, non quella che esisteva quando si è lanciato cmake.
file(GLOB_RECURSE gf_backups "${TARGET_DATA_DIR}/*.bak")
if(gf_backups)
    file(REMOVE ${gf_backups})
endif()
