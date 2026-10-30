package gateway

import (
	"log"
	"strings"
	"time"

	client "gyo.local/object_fps_pvp/protocol/clientv8"
)

// Diagnostics only (pv7 contract §3, time-echo): the clock sampling with each
// Client rides on Hello and Welcome; no new packet. It stays out of
// server.go, whose timeouts and admission must not name it (D32,
// TestClockStaysOutOfDecisions).

// sessionEcho is one session's sampling, started by its first Hello.
type sessionEcho struct{ sync *clockSync }

func (e *sessionEcho) clock(now time.Time) *clockSync {
	if e.sync == nil {
		e.sync = newClockSync(gatewayMicros(now))
	}
	return e.sync
}

// noteHello measures an authenticated Hello's sample (an invalid one is
// counted and ignored; the Hello is handled as before) and logs the session's
// window when it closes.
func (s *Server) noteHello(p *reservation, hello *client.Hello, now time.Time) {
	c := p.echo.clock(now)
	if sample := hello.GetClock(); sample != nil {
		c.observe(clockStamp{origin: sample.OriginUs, receive: sample.ReceiveUs, transmit: sample.TransmitUs}, gatewayMicros(now))
	}
	if window, ok := c.roll(gatewayMicros(now), false); ok {
		logClientWindow(p.playerID, window)
	}
}

// welcomeSample is a Welcome's echo, its transmit stamped now, as it is encoded.
func (s *Server) welcomeSample(p *reservation, now time.Time) *client.ClockSample {
	c := p.echo.clock(now)
	sample := &client.ClockSample{}
	if origin, receive, ok := c.echo(); ok {
		sample.OriginUs, sample.ReceiveUs = &origin, &receive
	}
	sample.TransmitUs = c.stamp(gatewayMicros(now))
	return sample
}

// endEcho logs the session's last window.
func (s *Server) endEcho(p *reservation, now time.Time) {
	if p.echo.sync != nil {
		window, _ := p.echo.sync.roll(gatewayMicros(now), true)
		logClientWindow(p.playerID, window)
	}
}

func logClientWindow(player uint64, window clockWindow) {
	log.Printf("client clock sync player=%d %s", player, strings.TrimPrefix(window.line(), "clock sync "))
}

// noteVerified restarts the session's clock window when a datagram that
// passed session verification ends a silence of the bound endpoint of at
// least connectionInterruptedAfter (a resume, network-architecture §4). before
// is the session's last verified time read before that datagram's admission;
// a datagram that verified nothing (another endpoint, a wrong token) leaves it
// as it was and restarts nothing. Diagnostics only: nothing decides on it.
func (s *Server) noteVerified(p *reservation, before, now time.Time) {
	after := p.session.LastVerified()
	if p.echo.sync == nil || before.IsZero() || !after.After(before) || after.Sub(before) < connectionInterruptedAfter {
		return
	}
	window := p.echo.sync.resume(gatewayMicros(now))
	log.Printf("client clock sync player=%d %s", p.playerID, strings.TrimPrefix(window.resumeLine(), "clock sync "))
}
