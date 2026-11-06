add_executable(gyo_collision_tests CollisionTests.cpp CollisionCorpus.cpp CollisionCorpusTests.cpp StaticAabbGridTests.cpp)
target_link_libraries(gyo_collision_tests PRIVATE GYO::Collision gyo_test_support doctest::doctest)
add_test(NAME gyo_collision_tests COMMAND gyo_collision_tests)

# Writes the corpus records for a same-host two-tree comparison
# (docs/architecture/plans/foundation-followups/scripts/compare_collision_corpus.py).
add_executable(gyo_collision_corpus CollisionCorpusMain.cpp CollisionCorpus.cpp)
target_link_libraries(gyo_collision_corpus PRIVATE GYO::Collision gyo_test_support doctest::doctest)

# Collision links only Math and, privately, Base (docs/architecture.md, module
# table); the static broadphase adds no dependency (v8-arch-big-map-engine-grid,
# judgment J7; gyo_expect_links is defined with the time tests). Checked once
# the whole project is configured.
function(gyo_expect_collision_links)
    gyo_expect_links(gyo_collision GYO::Math GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_collision_links)
