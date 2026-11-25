package stack

import (
	"bytes"
	"context"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"gyo.local/object_fps_pvp/services/internal/api"
	"gyo.local/object_fps_pvp/services/matches"
	"gyo.local/object_fps_pvp/services/token"
)

// fakeProvider starts no process: it remembers each Match's key, so the test
// verifies the tokens as that Match would.
type fakeProvider struct {
	mu     sync.Mutex
	keys   map[uint64][]byte
	specs  []matches.Spec
	refuse bool
	ended  chan struct{}
}

func newFakeProvider() *fakeProvider {
	return &fakeProvider{keys: map[uint64][]byte{}, ended: make(chan struct{})}
}

func (p *fakeProvider) Start(_ context.Context, spec matches.Spec) (matches.Running, error) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.refuse {
		return matches.Running{}, fmt.Errorf("refused")
	}
	p.keys[spec.MatchID] = spec.Key
	p.specs = append(p.specs, spec)
	return matches.Running{Address: "127.0.0.1:7801", Ended: p.ended, Stop: func() {}}, nil
}

// player is one Client of the gateway's external protocol.
type player struct {
	t       *testing.T
	base    string
	session string
	user    uint64
}

func (p *player) call(method, path, key string, in any) (int, map[string]any) {
	p.t.Helper()
	var body io.Reader
	if in != nil {
		encoded, _ := json.Marshal(in)
		body = bytes.NewReader(encoded)
	}
	request, _ := http.NewRequest(method, p.base+path, body)
	if p.session != "" {
		request.Header.Set("Authorization", "Bearer "+p.session)
	}
	if key != "" {
		request.Header.Set("Idempotency-Key", key)
	}
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		p.t.Fatalf("%s %s: %v", method, path, err)
	}
	defer response.Body.Close()
	var out map[string]any
	_ = json.NewDecoder(response.Body).Decode(&out)
	return response.StatusCode, out
}

func (p *player) must(method, path, key string, in any) map[string]any {
	p.t.Helper()
	status, out := p.call(method, path, key, in)
	if status != http.StatusOK {
		p.t.Fatalf("%s %s: %d %v", method, path, status, out)
	}
	return out
}

func (p *player) refused(method, path string, in any, wantStatus int, wantCode string) {
	p.t.Helper()
	status, out := p.call(method, path, "", in)
	if status != wantStatus || out["error"] != wantCode {
		p.t.Fatalf("%s %s: %d %v, want %d %s", method, path, status, out, wantStatus, wantCode)
	}
}

func signIn(t *testing.T, s *Stack, device string) *player {
	t.Helper()
	p := &player{t: t, base: s.GatewayURL}
	out := p.must(http.MethodPost, "/v1/session", "", map[string]string{"deviceId": device})
	p.session = out["sessionToken"].(string)
	p.user = uint64(out["userId"].(float64))
	return p
}

func start(t *testing.T, provider matches.Provider) *Stack {
	t.Helper()
	s, err := Start(Config{Provider: provider, Capacity: 2})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(s.Close)
	return s
}

func roomPath(room map[string]any, action string) string {
	path := "/v1/rooms/" + strconv.FormatUint(uint64(room["id"].(float64)), 10)
	if action != "" {
		path += "/" + action
	}
	return path
}

func TestTwoPlayersCreateJoinReadyStartAndGetTheirTargets(t *testing.T) {
	provider := newFakeProvider()
	s := start(t, provider)
	host, guest := signIn(t, s, "device-host"), signIn(t, s, "device-guest")
	if host.user == guest.user {
		t.Fatalf("two devices got the same user %d", host.user)
	}

	room := host.must(http.MethodPost, "/v1/rooms", "create-1", map[string]string{"mode": "survival", "map": "pvp_training_v1"})
	listed := guest.must(http.MethodGet, "/v1/rooms", "", nil)["rooms"].([]any)
	if len(listed) != 1 || listed[0].(map[string]any)["mode"] != "survival" {
		t.Fatalf("the room list is %v, want the one survival room", listed)
	}
	guest.must(http.MethodPost, roomPath(room, "join"), "join-1", nil)
	host.must(http.MethodPost, roomPath(room, "ready"), "ready-1", map[string]bool{"ready": true})
	host.refused(http.MethodPost, roomPath(room, "start"), nil, http.StatusConflict, "not_all_ready")
	guest.must(http.MethodPost, roomPath(room, "ready"), "ready-2", map[string]bool{"ready": true})
	guest.refused(http.MethodPost, roomPath(room, "start"), nil, http.StatusForbidden, "not_owner")
	started := host.must(http.MethodPost, roomPath(room, "start"), "start-1", nil)
	if started["state"] != "started" {
		t.Fatalf("the started room is %v", started)
	}

	if len(provider.specs) != 1 || provider.specs[0].Mode != "survival" || provider.specs[0].Map != "pvp_training_v1" || provider.specs[0].Capacity != 2 {
		t.Fatalf("the provider was asked %+v, want one survival pvp_training_v1 Match of 2", provider.specs)
	}
	spec := provider.specs[0]
	if len(spec.Players) != 2 || spec.Players[0] == spec.Players[1] ||
		(spec.Players[0] != host.user && spec.Players[1] != host.user) || (spec.Players[0] != guest.user && spec.Players[1] != guest.user) {
		t.Fatalf("the Match waits for %v, want the room's members %d and %d", spec.Players, host.user, guest.user)
	}
	if spec.Saves != strings.TrimPrefix(s.SavesURL, "http://") {
		t.Fatalf("the Match is given the save service at %q, want %s", spec.Saves, s.SavesURL)
	}
	for _, p := range []*player{host, guest} {
		view := p.must(http.MethodGet, roomPath(room, ""), "", nil)
		connect, ok := view["connect"].(map[string]any)
		if !ok {
			t.Fatalf("user %d's started room has no connect: %v", p.user, view)
		}
		if connect["server"] != "127.0.0.1:7801" || uint64(connect["userId"].(float64)) != p.user {
			t.Fatalf("user %d's target is %v", p.user, connect)
		}
		signed, err := hex.DecodeString(connect["token"].(string))
		if err != nil {
			t.Fatal(err)
		}
		if err := token.VerifyAdmission(spec.Key, signed, time.Now(), spec.MatchID, p.user); err != nil {
			t.Fatalf("user %d's token is refused by its Match: %v", p.user, err)
		}
	}
	if s.Matches.Running() != 1 {
		t.Fatalf("running Matches %d, want 1", s.Matches.Running())
	}
	close(provider.ended)
	deadline := time.Now().Add(2 * time.Second)
	for s.Matches.Running() != 0 && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}
	if s.Matches.Running() != 0 {
		t.Fatal("an ended Match is still counted as running")
	}
}

func TestAStartedRoomCannotBeEntered(t *testing.T) {
	s := start(t, newFakeProvider())
	host, late := signIn(t, s, "host"), signIn(t, s, "late")
	room := host.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	host.must(http.MethodPost, roomPath(room, "ready"), "", map[string]bool{"ready": true})
	host.must(http.MethodPost, roomPath(room, "start"), "", nil)
	late.refused(http.MethodPost, roomPath(room, "join"), nil, http.StatusConflict, "room_started")
	if rooms := late.must(http.MethodGet, "/v1/rooms", "", nil)["rooms"].([]any); len(rooms) != 0 {
		t.Fatalf("a started room is listed: %v", rooms)
	}
	if view := late.must(http.MethodGet, roomPath(room, ""), "", nil); view["connect"] != nil {
		t.Fatalf("a non-member sees a ticket: %v", view)
	}
}

func TestAFullRoomRefusesAJoin(t *testing.T) {
	s := start(t, newFakeProvider())
	a, b, c := signIn(t, s, "a"), signIn(t, s, "b"), signIn(t, s, "c")
	room := a.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	b.must(http.MethodPost, roomPath(room, "join"), "", nil)
	c.refused(http.MethodPost, roomPath(room, "join"), nil, http.StatusConflict, "room_full")
}

func TestARefusedAllocationOpensTheRoomAgain(t *testing.T) {
	provider := newFakeProvider()
	provider.refuse = true
	s := start(t, provider)
	host := signIn(t, s, "host")
	room := host.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	host.must(http.MethodPost, roomPath(room, "ready"), "", map[string]bool{"ready": true})
	host.refused(http.MethodPost, roomPath(room, "start"), nil, http.StatusServiceUnavailable, "no_match_available")
	if view := host.must(http.MethodGet, roomPath(room, ""), "", nil); view["state"] != "open" {
		t.Fatalf("after a refused allocation the room is %v, want open", view["state"])
	}
}

func TestTheOwnerLeavingHandsTheRoomOn(t *testing.T) {
	s := start(t, newFakeProvider())
	host, guest := signIn(t, s, "host"), signIn(t, s, "guest")
	room := host.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	guest.must(http.MethodPost, roomPath(room, "join"), "", nil)
	host.must(http.MethodPost, roomPath(room, "leave"), "", nil)
	if view := guest.must(http.MethodGet, roomPath(room, ""), "", nil); uint64(view["owner"].(float64)) != guest.user {
		t.Fatalf("the owner after the host left is %v, want %d", view["owner"], guest.user)
	}
	guest.must(http.MethodPost, roomPath(room, "leave"), "", nil)
	guest.refused(http.MethodGet, roomPath(room, ""), nil, http.StatusNotFound, "room_not_found")
}

func TestJoiningAnotherRoomMovesTheUser(t *testing.T) {
	s := start(t, newFakeProvider())
	a, b := signIn(t, s, "a"), signIn(t, s, "b")
	first := a.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	b.must(http.MethodPost, roomPath(first, "join"), "", nil)
	second := b.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "survival", "map": "pvp_training_v1"})
	if view := a.must(http.MethodGet, roomPath(first, ""), "", nil); len(view["members"].([]any)) != 1 {
		t.Fatalf("the first room keeps the user who made another: %v", view["members"])
	}
	if roomPath(first, "") == roomPath(second, "") {
		t.Fatal("the second room has the first room's id")
	}
}

func TestAnIdempotencyKeyRepeatsTheFirstAnswer(t *testing.T) {
	s := start(t, newFakeProvider())
	host := signIn(t, s, "host")
	create := map[string]string{"mode": "coop", "map": "pvp_training_v1"}
	first := host.must(http.MethodPost, "/v1/rooms", "same", create)
	again := host.must(http.MethodPost, "/v1/rooms", "same", create)
	if first["id"] != again["id"] {
		t.Fatalf("the same key made rooms %v and %v", first["id"], again["id"])
	}
	other := host.must(http.MethodPost, "/v1/rooms", "other", create)
	if other["id"] == first["id"] {
		t.Fatal("another key repeated the first answer")
	}
	// Another user's same key is their own.
	guest := signIn(t, s, "guest")
	if mine := guest.must(http.MethodPost, "/v1/rooms", "same", create); mine["id"] == first["id"] {
		t.Fatal("another user's key repeated the host's answer")
	}
}

func TestTheGatewayRefusesWithoutAValidSession(t *testing.T) {
	s := start(t, newFakeProvider())
	anonymous := &player{t: t, base: s.GatewayURL}
	anonymous.refused(http.MethodGet, "/v1/rooms", nil, http.StatusUnauthorized, "no_session")
	anonymous.session = "forged.token"
	anonymous.refused(http.MethodGet, "/v1/rooms", nil, http.StatusUnauthorized, "invalid_session")
	signedIn := signIn(t, s, "someone")
	signedIn.refused(http.MethodGet, "/v1/rooms/1%2F..%2F..", nil, http.StatusBadRequest, "invalid_room")
	signedIn.refused(http.MethodPost, "/v1/rooms", map[string]string{"mode": "deathmatch", "map": "pvp_training_v1"}, http.StatusBadRequest, "unknown_mode")
	signedIn.refused(http.MethodPost, "/v1/rooms", map[string]any{"mode": "coop", "map": "pvp_training_v1", "extra": 1}, http.StatusBadRequest, "invalid_room")
}

func TestTheSameDeviceIsTheSameUser(t *testing.T) {
	s := start(t, newFakeProvider())
	if signIn(t, s, "device").user != signIn(t, s, "device").user {
		t.Fatal("one device signed in as two users")
	}
}

func TestAPlayerReadsTheirOwnSaveThroughTheGateway(t *testing.T) {
	s := start(t, newFakeProvider())
	p := signIn(t, s, "saver")
	// The Match writes (the internal protocol, user 0 = the system).
	write, _ := http.NewRequest(http.MethodPut, s.SavesURL+"/internal/v1/saves/"+strconv.FormatUint(p.user, 10),
		bytes.NewReader([]byte(`{"version":0,"body":{"saveVersion":1,"kills":3}}`)))
	write.Header.Set("X-GYO-User", "0")
	response, err := http.DefaultClient.Do(write)
	if err != nil || response.StatusCode != http.StatusOK {
		t.Fatalf("the Match's write: %v %v", response, err)
	}
	response.Body.Close()
	save := p.must(http.MethodGet, "/v1/saves/me", "", nil)
	if save["version"].(float64) != 1 || save["body"].(map[string]any)["kills"].(float64) != 3 {
		t.Fatalf("the player's save is %v", save)
	}
	// The external protocol has no write.
	p.refused(http.MethodPut, "/v1/saves/me", map[string]any{"version": 1, "body": map[string]int{"kills": 99}}, http.StatusNotFound, "unknown_route")
}

func TestAMemberGetsAFreshTokenUntilTheMatchEnds(t *testing.T) {
	provider := newFakeProvider()
	now := time.Unix(1_900_000_000, 0)
	var mu sync.Mutex
	clock := func() time.Time { mu.Lock(); defer mu.Unlock(); return now }
	s, err := Start(Config{Provider: provider, Capacity: 2, Now: clock})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(s.Close)
	host := signIn(t, s, "host")
	room := host.must(http.MethodPost, "/v1/rooms", "", map[string]string{"mode": "coop", "map": "pvp_training_v1"})
	host.must(http.MethodPost, roomPath(room, "ready"), "", map[string]bool{"ready": true})
	host.must(http.MethodPost, roomPath(room, "start"), "", nil)
	spec := provider.specs[0]
	mu.Lock()
	now = now.Add(time.Hour) // long past the first token's expiry
	mu.Unlock()
	view := host.must(http.MethodGet, roomPath(room, ""), "", nil)
	signed, _ := hex.DecodeString(view["connect"].(map[string]any)["token"].(string))
	if err := token.VerifyAdmission(spec.Key, signed, clock(), spec.MatchID, host.user); err != nil {
		t.Fatalf("an hour into the Match the member's token is refused: %v", err)
	}
	// Only the Match's players get one.
	stranger := api.NewClient(s.MatchesURL)
	err = stranger.Call(context.Background(), http.MethodPost, "/internal/v1/matches/"+strconv.FormatUint(spec.MatchID, 10)+"/tickets",
		api.As(api.System), "", map[string]uint64{"userId": host.user + 100}, nil)
	var refused *api.Refused
	if !errors.As(err, &refused) || refused.Code != "not_player" {
		t.Fatalf("a token for a user the Match was not started for: %v, want not_player", err)
	}
	close(provider.ended)
	deadline := time.Now().Add(2 * time.Second)
	for s.Matches.Running() != 0 && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}
	host.refused(http.MethodGet, roomPath(room, ""), nil, http.StatusGone, "match_ended")
	host.refused(http.MethodGet, roomPath(room, ""), nil, http.StatusNotFound, "room_not_found")
}

func TestAnotherVersionIsToldTheVersions(t *testing.T) {
	s := start(t, newFakeProvider())
	p := signIn(t, s, "old-client")
	status, out := p.call(http.MethodGet, "/v2/rooms", "", nil)
	if status != http.StatusNotFound || out["error"] != "unsupported_version" || len(out["versions"].([]any)) != 1 {
		t.Fatalf("another version's path: %d %v", status, out)
	}
	p.refused(http.MethodGet, "/v1/nothing", nil, http.StatusNotFound, "unknown_route")
}
