package framing

import (
	"bytes"
	"encoding/binary"
	"errors"
	"io"
	"testing"
)

func TestDatagramRoundTripAndMalformed(t *testing.T) {
	header := Header{Version: 7, Type: 19, SessionID: 0x1020304050607080, Sequence: 0xfffffffe}
	packet, err := EncodeDatagram(header, []byte{0, 1, 255})
	if err != nil {
		t.Fatal(err)
	}
	got, payload, err := DecodeDatagram(packet)
	if err != nil || got != header || !bytes.Equal(payload, []byte{0, 1, 255}) {
		t.Fatalf("roundtrip: %+v %v %v", got, payload, err)
	}
	for _, size := range []int{0, 3, HeaderSize - 1, len(packet) - 1} {
		if _, _, err := DecodeDatagram(packet[:size]); !errors.Is(err, ErrMalformed) {
			t.Fatalf("size %d accepted", size)
		}
	}
	bad := bytes.Clone(packet)
	bad[22] = 1
	if _, _, err := DecodeDatagram(bad); err == nil {
		t.Fatal("unsupported channel accepted")
	}
	bad = bytes.Clone(packet)
	bad[0] = 'X'
	if _, _, err := DecodeDatagram(bad); err == nil {
		t.Fatal("wrong magic accepted")
	}
	if _, err := EncodeDatagram(header, make([]byte, MaxDatagram)); err == nil {
		t.Fatal("oversize payload accepted")
	}
}

type shortWriter struct{ bytes.Buffer }

func (w *shortWriter) Write(p []byte) (int, error) {
	if len(p) > 2 {
		p = p[:2]
	}
	return w.Buffer.Write(p)
}
func TestLengthFramesHandleFragmentationAndLimits(t *testing.T) {
	var writer shortWriter
	if err := WriteFrame(&writer, []byte("first")); err != nil {
		t.Fatal(err)
	}
	if err := WriteFrame(&writer, []byte("second")); err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{"first", "second"} {
		got, err := ReadFrame(&writer)
		if err != nil || string(got) != want {
			t.Fatalf("%q: %v", got, err)
		}
	}
	if _, err := ReadFrame(&writer); !errors.Is(err, io.EOF) {
		t.Fatal(err)
	}
	for _, size := range []uint32{0, MaxFrame + 1} {
		var h [4]byte
		binary.BigEndian.PutUint32(h[:], size)
		if _, err := ReadFrame(bytes.NewReader(h[:])); !errors.Is(err, ErrMalformed) {
			t.Fatal("bad length accepted")
		}
	}
	if err := WriteFrame(io.Discard, make([]byte, MaxFrame+1)); err == nil {
		t.Fatal("oversize write accepted")
	}
	if _, err := ReadFrame(bytes.NewReader([]byte{0, 0, 0, 2, 1})); !errors.Is(err, io.ErrUnexpectedEOF) {
		t.Fatalf("truncation: %v", err)
	}
}
func TestSerialWrap(t *testing.T) {
	if !NewerSequence(0, 0xffffffff) || NewerSequence(42, 42) || NewerSequence(41, 42) || NewerSequence(0xffffffff, 0) {
		t.Fatal("bad serial ordering")
	}
}
