package framing

import (
	"context"
	"net"
	"time"
)

// TCPConnection exchanges bounded byte frames. The caller supplies timeouts,
// interprets payloads and decides what a transport error means for its product.
// One reader and one writer may run concurrently; Close unblocks both.
type TCPConnection struct{ conn net.Conn }

func DialTCP(ctx context.Context, address string, timeout time.Duration) (*TCPConnection, error) {
	conn, err := (&net.Dialer{Timeout: timeout}).DialContext(ctx, "tcp", address)
	if err != nil {
		return nil, err
	}
	return &TCPConnection{conn: conn}, nil
}

func deadline(timeout time.Duration) time.Time {
	if timeout <= 0 {
		return time.Time{}
	}
	return time.Now().Add(timeout)
}

func (c *TCPConnection) Read(timeout time.Duration) ([]byte, error) {
	if err := c.conn.SetReadDeadline(deadline(timeout)); err != nil {
		return nil, err
	}
	return ReadFrame(c.conn)
}

func (c *TCPConnection) Write(payload []byte, timeout time.Duration) error {
	if err := c.conn.SetWriteDeadline(deadline(timeout)); err != nil {
		return err
	}
	return WriteFrame(c.conn, payload)
}

func (c *TCPConnection) Close() error { return c.conn.Close() }
