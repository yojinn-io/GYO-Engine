package stack

import (
	"os"
	"os/exec"
	"regexp"
	"strings"
	"testing"

	"gyo.local/object_fps_pvp/services/matches"
)

// TestTheRealClientPlaysThroughTheGateway runs the whole line on this machine
// (game-flow declaration, segment 3): every service in this process, each
// Match a real process, and the game's Client (its client test program, the
// "network:" case) making a room, getting ready, starting it, playing in the
// Match over UDP, evacuating, and reading the save the Match wrote. It needs
// GYO_PVP_MATCH, GYO_PVP_CONTENT and GYO_PVP_CLIENT_TESTS (CTest's
// object_fps_pvp.gateway_client, label network, sets them).
func TestTheRealClientPlaysThroughTheGateway(t *testing.T) {
	match, content, client := os.Getenv("GYO_PVP_MATCH"), os.Getenv("GYO_PVP_CONTENT"), os.Getenv("GYO_PVP_CLIENT_TESTS")
	if match == "" || content == "" || client == "" {
		t.Skip("GYO_PVP_MATCH, GYO_PVP_CONTENT or GYO_PVP_CLIENT_TESTS is not set")
	}
	local := &matches.Local{Command: match, Arguments: []string{"--content", content}, Advertise: "127.0.0.1",
		FirstPort: 47850, LastPort: 47859, IdleSeconds: 5}
	s, err := Start(Config{Provider: local, Maps: []string{"pvp_training_v1", "pvp_corners_v1", "biome_trial"}})
	if err != nil {
		t.Fatal(err)
	}
	defer s.Close()
	command := exec.Command(client, "--test-case=network:*")
	command.Env = append(os.Environ(), "GYO_PVP_GATEWAY="+strings.TrimPrefix(s.GatewayURL, "http://"))
	output, err := command.CombinedOutput()
	t.Logf("%s", output)
	if err != nil {
		t.Fatalf("the Client's run failed: %v", err)
	}
	if !regexp.MustCompile(`test cases: +1 \| +1 passed`).Match(output) {
		t.Fatal("the Client's network case did not run")
	}
}
