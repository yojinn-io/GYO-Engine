// The ink Plugin's test story for inkcpp's defect D2b: a save taken right
// after a choice restores with every sequence started over.
-> loop

=== loop ===
stopping: {first|second|third} #line:sequence_stopping
cycle: {&one|two|three} #line:sequence_cycle
+ [Again #line:sequence_again]
    -> loop
