// The ink Plugin's test story for inkcpp's defect D5: a shuffle can show the
// same element twice within one pass. The seed is the story's own, as in the
// upstream report (JBenda/inkcpp#182).
~ SEED_RANDOM(21)
-> loop

=== loop ===
{shuffle once:
- x #line:shuffle_x
- y #line:shuffle_y
}
+ [Again #line:shuffle_again]
    -> loop
