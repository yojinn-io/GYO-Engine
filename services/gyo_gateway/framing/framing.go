// Package framing owns byte transport only. It does not decode game payloads.
package framing

import (
	"encoding/binary"
	"errors"
	"io"
)

const (
	HeaderSize  = 24
	MaxDatagram = 1200
	MaxFrame    = 64 * 1024
)

var ErrMalformed = errors.New("malformed transport frame")

type Header struct {
	Version   uint16
	Type      uint16
	SessionID uint64
	Sequence  uint32
	Channel   uint16 // v1 has only channel 0 (unreliable sequenced).
}

func EncodeDatagram(h Header, payload []byte) ([]byte, error) {
	if len(payload) > MaxDatagram-HeaderSize || h.Channel != 0 {
		return nil, ErrMalformed
	}
	p := make([]byte, HeaderSize+len(payload))
	copy(p, "GYOP")
	binary.BigEndian.PutUint16(p[4:], h.Version)
	binary.BigEndian.PutUint16(p[6:], h.Type)
	binary.BigEndian.PutUint64(p[8:], h.SessionID)
	binary.BigEndian.PutUint32(p[16:], h.Sequence)
	binary.BigEndian.PutUint16(p[20:], uint16(len(payload)))
	binary.BigEndian.PutUint16(p[22:], h.Channel)
	copy(p[HeaderSize:], payload)
	return p, nil
}

func DecodeDatagram(p []byte) (Header, []byte, error) {
	if len(p) < HeaderSize || len(p) > MaxDatagram || string(p[:4]) != "GYOP" ||
		binary.BigEndian.Uint16(p[22:]) != 0 || int(binary.BigEndian.Uint16(p[20:])) != len(p)-HeaderSize {
		return Header{}, nil, ErrMalformed
	}
	return Header{binary.BigEndian.Uint16(p[4:]), binary.BigEndian.Uint16(p[6:]),
		binary.BigEndian.Uint64(p[8:]), binary.BigEndian.Uint32(p[16:]), binary.BigEndian.Uint16(p[22:])}, p[HeaderSize:], nil
}

// NewerSequence implements serial-number ordering, including uint32 wrap.
func NewerSequence(next, previous uint32) bool { return int32(next-previous) > 0 }

func ReadFrame(r io.Reader) ([]byte, error) {
	var header [4]byte
	if _, err := io.ReadFull(r, header[:]); err != nil {
		return nil, err
	}
	n := binary.BigEndian.Uint32(header[:])
	if n == 0 || n > MaxFrame {
		return nil, ErrMalformed
	}
	p := make([]byte, int(n))
	_, err := io.ReadFull(r, p)
	return p, err
}

func WriteFrame(w io.Writer, payload []byte) error {
	if len(payload) == 0 || len(payload) > MaxFrame {
		return ErrMalformed
	}
	var header [4]byte
	binary.BigEndian.PutUint32(header[:], uint32(len(payload)))
	if err := writeAll(w, header[:]); err != nil {
		return err
	}
	return writeAll(w, payload)
}

func writeAll(w io.Writer, p []byte) error {
	for len(p) > 0 {
		n, err := w.Write(p)
		if n < 0 || n > len(p) {
			return io.ErrShortWrite
		}
		p = p[n:]
		if err != nil {
			return err
		}
		if n == 0 {
			return io.ErrShortWrite
		}
	}
	return nil
}
