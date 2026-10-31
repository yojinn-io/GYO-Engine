package adapter

import (
	client "gyo.local/object_fps_pvp/protocol/clientv9"
	runtime "gyo.local/object_fps_pvp/protocol/runtimev9"
)

// snapshotForClient is SnapshotForClient for tests written before pv9 §4.2:
// a snapshot without animation first gets one valid value per player. Tests
// of the animation checks call SnapshotForClient directly.
func snapshotForClient(in *runtime.WorldSnapshot, rules *runtime.CombatRules) (*client.WorldSnapshot, error) {
	if in != nil && in.Animation == nil {
		for _, p := range in.Players {
			if p == nil {
				continue
			}
			phase := runtime.JumpPhase_JUMP_PHASE_GROUNDED
			if !p.Grounded {
				phase = runtime.JumpPhase_JUMP_PHASE_AIRBORNE
			}
			in.Animation = append(in.Animation, &runtime.PlayerAnimation{PlayerId: p.PlayerId,
				LifeGeneration: p.LifeGeneration, JumpPhase: phase})
		}
	}
	return SnapshotForClient(in, rules)
}
