// The ink Plugin's test story for inkcpp's defect D1: a once-only choice, once
// taken, is skipped; its tags go to the next choice.
-> hub

=== hub ===
Pick one. #line:once_pick
* [Once #line:once_once]
    -> hub
+ [Again #line:once_again]
    -> hub
+ [Stop #line:once_stop]
    -> END
