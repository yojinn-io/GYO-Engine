# Object_Connect: a 2D puzzle game ported from ~/Code/GitHome/Object_Connect (ca4482c).
# The porting inventory is docs/porting-inventory.zh-Hant.md in this directory.
#
# logic: the game's rules and simulation, with no window, input, renderer or audio
# device (the original object_connect_core). It links no Engine unit: the World
# layer that turns it into Systems and Resources comes in the next batch, and the
# main executable waits for the window, input and render batches.
gyo_game_project()

gyo_game_add_library(logic OUT_TARGET object_connect_logic SOURCES
    src/data/Csv.cpp
    src/data/PuzzleCatalogLoader.cpp
    src/game/FinalResults.cpp
    src/game/GameFlow.cpp
    src/geometry/Geometry2D.cpp
    src/puzzle/PuzzleBoard.cpp
    src/tentacle/BloodTentacle.cpp
    src/tentacle/RibbonStrip.cpp)
