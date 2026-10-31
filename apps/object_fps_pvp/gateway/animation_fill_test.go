package gateway

import runtime "gyo.local/object_fps_pvp/protocol/runtimev9"

// fillAnimation gives a test snapshot without animation one valid value per
// player (pv9 §4.5), as fakeRuntime.send fills the combat state. Tests of the
// animation checks themselves set Animation and are left as they are.
func fillAnimation(snapshot *runtime.WorldSnapshot) {
	if snapshot == nil || snapshot.Animation != nil {
		return
	}
	for _, p := range snapshot.Players {
		phase := runtime.JumpPhase_JUMP_PHASE_GROUNDED
		if !p.Grounded {
			phase = runtime.JumpPhase_JUMP_PHASE_AIRBORNE
		}
		snapshot.Animation = append(snapshot.Animation, &runtime.PlayerAnimation{PlayerId: p.PlayerId,
			LifeGeneration: p.LifeGeneration, JumpPhase: phase})
	}
}
