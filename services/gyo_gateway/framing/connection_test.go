package framing

import (
	"context"
	"errors"
	"io"
	"net"
	"testing"
	"time"
)

func TestTCPConnectionFramesAndDisconnect(t *testing.T) {
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer listener.Close()
	served := make(chan error, 1)
	go func() {
		peer, err := listener.Accept()
		if err != nil {
			served <- err
			return
		}
		defer peer.Close()
		_ = peer.SetDeadline(time.Now().Add(2 * time.Second))
		payload, err := ReadFrame(peer)
		if err == nil {
			err = WriteFrame(peer, payload)
		}
		served <- err
	}()
	conn, err := DialTCP(context.Background(), listener.Addr().String(), time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	if err = conn.Write([]byte("synthetic bytes"), time.Second); err != nil {
		t.Fatal(err)
	}
	data, err := conn.Read(time.Second)
	if err != nil || string(data) != "synthetic bytes" {
		t.Fatalf("roundtrip %q: %v", data, err)
	}
	if err = <-served; err != nil {
		t.Fatal(err)
	}
	if _, err = conn.Read(time.Second); !errors.Is(err, io.EOF) {
		t.Fatalf("disconnect not reported: %v", err)
	}
}

func TestTCPConnectionTimeoutCancellationAndClose(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := DialTCP(ctx, "127.0.0.1:1", time.Second); !errors.Is(err, context.Canceled) {
		t.Fatalf("cancelled dial: %v", err)
	}
	a, b := net.Pipe()
	defer b.Close()
	conn := &TCPConnection{conn: a}
	defer conn.Close()
	if _, err := conn.Read(20 * time.Millisecond); err == nil {
		t.Fatal("silent peer did not time out")
	} else if timeout, ok := err.(net.Error); !ok || !timeout.Timeout() {
		t.Fatal(err)
	}
	if err := conn.Write([]byte("blocked write"), 20*time.Millisecond); err == nil {
		t.Fatal("blocked writer did not time out")
	}
	done := make(chan error, 1)
	go func() { _, err := conn.Read(0); done <- err }()
	_ = conn.Close()
	select {
	case err := <-done:
		if err == nil {
			t.Fatal("close was not reported")
		}
	case <-time.After(time.Second):
		t.Fatal("Close did not unblock reader")
	}
}
