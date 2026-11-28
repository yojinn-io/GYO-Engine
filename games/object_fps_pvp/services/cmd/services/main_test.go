package main

import (
	"path/filepath"
	"slices"
	"testing"
)

// The shipped content: the rooms offer its modes (modes/modes.json, in order) and its arenas.
func TestTheRoomsOfferTheShippedModesAndArenas(t *testing.T) {
	content := filepath.Join("..", "..", "..", "..", "..", "assets", "object_fps_pvp")
	modes, err := modesOf("", content)
	if err != nil {
		t.Fatal(err)
	}
	if !slices.Equal(modes, []string{"core_pvp", "sandbox"}) {
		t.Fatalf("the shipped modes are %v, want [core_pvp sandbox]", modes)
	}
	maps, err := mapsOf("", content)
	if err != nil {
		t.Fatal(err)
	}
	if len(maps) == 0 || maps[0] != "pvp_training_v1" {
		t.Fatalf("the shipped arenas are %v", maps)
	}
	// --modes wins over the content; without either, any mode.
	if given, _ := modesOf("a,b", content); !slices.Equal(given, []string{"a", "b"}) {
		t.Fatalf("--modes a,b gives %v", given)
	}
	if none, _ := modesOf("", ""); none != nil {
		t.Fatalf("no --modes and no --content gives %v, want none (any)", none)
	}
}
