add_executable(gyo_collision_tests CollisionTests.cpp CollisionCorpus.cpp CollisionCorpusTests.cpp)
target_link_libraries(gyo_collision_tests PRIVATE GYO::Collision gyo_test_support doctest::doctest)
add_test(NAME gyo_collision_tests COMMAND gyo_collision_tests)

# Writes the corpus records for a same-host two-tree comparison
# (docs/architecture/plans/foundation-followups/scripts/compare_collision_corpus.py).
add_executable(gyo_collision_corpus CollisionCorpusMain.cpp CollisionCorpus.cpp)
target_link_libraries(gyo_collision_corpus PRIVATE GYO::Collision gyo_test_support doctest::doctest)
