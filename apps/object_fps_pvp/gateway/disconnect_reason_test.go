package gateway

import (
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"testing"
)

// log-contract, family runtime_disconnected: the liveness deadline is one
// reason whether it is found before a read or during it; a write timeout is
// another; the original text stays.
func TestRuntimeDisconnectReasonsClassifyWrappedErrors(t *testing.T) {
	readTimeout := &net.OpError{Op: "read", Net: "tcp", Err: os.ErrDeadlineExceeded}
	writeTimeout := &net.OpError{Op: "write", Net: "tcp", Err: os.ErrDeadlineExceeded}
	cases := []struct {
		name   string
		err    error
		reason string
		text   string
	}{
		{"liveness before the read", fmt.Errorf("%w: %w", errRuntimeLiveness, os.ErrDeadlineExceeded), "liveness_timeout",
			"runtime liveness: i/o timeout"},
		{"liveness during the read", fmt.Errorf("%w: %w", errRuntimeLiveness, readTimeout), "liveness_timeout",
			"runtime liveness: read tcp: i/o timeout"},
		{"write timeout", runtimeWriteError(writeTimeout), "write_timeout", "runtime write timeout: write tcp: i/o timeout"},
		{"write other", runtimeWriteError(errors.New("broken pipe")), "other", "broken pipe"},
		{"version", fmt.Errorf("%w: expected 7, received 8", errRuntimeVersionMismatch), "version_mismatch",
			"runtime protocol version mismatch: expected 7, received 8"},
		{"protocol", protocolError{errors.New("unknown runtime message fields=[99]")}, "protocol",
			"unknown runtime message fields=[99]"},
		{"eof", io.EOF, "eof", "EOF"},
		{"unexpected eof", fmt.Errorf("frame: %w", io.ErrUnexpectedEOF), "eof", "frame: unexpected EOF"},
		{"other", errors.New("reset by peer"), "other", "reset by peer"},
	}
	for _, c := range cases {
		if got := disconnectReason(c.err); got != c.reason {
			t.Errorf("%s: reason %s, want %s", c.name, got, c.reason)
		}
		if c.err.Error() != c.text {
			t.Errorf("%s: text %q, want %q", c.name, c.err.Error(), c.text)
		}
	}
}
