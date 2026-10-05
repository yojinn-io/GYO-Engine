package framing

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strconv"
	"testing"
)

// Synthetic transport vectors shared with the engine C++ codec
// (engine/net, tests/common/net).
var vectorFile = filepath.Join("..", "..", "..", "tests", "common", "net", "fixtures", "gyop_datagram_vectors.json")

type datagramVector struct {
	Name        string `json:"name"`
	Version     uint16 `json:"version"`
	Type        uint16 `json:"type"`
	Session     string `json:"session"`
	Sequence    uint32 `json:"sequence"`
	Channel     uint16 `json:"channel"`
	Payload     string `json:"payload"`
	Datagram    string `json:"datagram"`
	PayloadSize int    `json:"payload_size"`
}

type datagramVectors struct {
	HeaderSize     int              `json:"header_size"`
	MaxDatagram    int              `json:"max_datagram"`
	Valid          []datagramVector `json:"valid"`
	Malformed      []datagramVector `json:"malformed"`
	EncodeRejected []datagramVector `json:"encode_rejected"`
}

func loadVectors(t *testing.T) datagramVectors {
	t.Helper()
	raw, err := os.ReadFile(vectorFile)
	if err != nil {
		t.Fatal(err)
	}
	var vectors datagramVectors
	if err := json.Unmarshal(raw, &vectors); err != nil {
		t.Fatal(err)
	}
	return vectors
}

func (v datagramVector) header(t *testing.T) Header {
	t.Helper()
	session, err := strconv.ParseUint(v.Session, 16, 64)
	if err != nil {
		t.Fatalf("%s: session %q: %v", v.Name, v.Session, err)
	}
	return Header{Version: v.Version, Type: v.Type, SessionID: session, Sequence: v.Sequence, Channel: v.Channel}
}

func decodeHex(t *testing.T, name, text string) []byte {
	t.Helper()
	b, err := hex.DecodeString(text)
	if err != nil {
		t.Fatalf("%s: %v", name, err)
	}
	return b
}

func TestSharedVectorsRoundTrip(t *testing.T) {
	vectors := loadVectors(t)
	if vectors.HeaderSize != HeaderSize || vectors.MaxDatagram != MaxDatagram {
		t.Fatalf("contract sizes %d/%d differ from %d/%d", vectors.HeaderSize, vectors.MaxDatagram, HeaderSize, MaxDatagram)
	}
	for _, v := range vectors.Valid {
		header := v.header(t)
		payload := decodeHex(t, v.Name, v.Payload)
		want := decodeHex(t, v.Name, v.Datagram)
		got, err := EncodeDatagram(header, payload)
		if err != nil || !bytes.Equal(got, want) {
			t.Fatalf("%s: encode %x %v", v.Name, got, err)
		}
		decoded, body, err := DecodeDatagram(want)
		if err != nil || decoded != header || !bytes.Equal(body, payload) {
			t.Fatalf("%s: decode %+v %x %v", v.Name, decoded, body, err)
		}
	}
}

func TestSharedVectorsRejected(t *testing.T) {
	vectors := loadVectors(t)
	for _, v := range vectors.Malformed {
		if _, _, err := DecodeDatagram(decodeHex(t, v.Name, v.Datagram)); !errors.Is(err, ErrMalformed) {
			t.Fatalf("%s: accepted", v.Name)
		}
	}
	for _, v := range vectors.EncodeRejected {
		if _, err := EncodeDatagram(v.header(t), make([]byte, v.PayloadSize)); !errors.Is(err, ErrMalformed) {
			t.Fatalf("%s: encoded", v.Name)
		}
	}
}
