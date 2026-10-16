package gateway

import (
	"context"
	"log"
	"time"
)

// diagnosticInterval paces the Server's diagnostic timer (pv7 §2: 1 Hz).
var diagnosticInterval = time.Second

// runDiagnostics is the Server's diagnostic timer: each tick queues the runtime
// link heartbeat (pv7 §3) and logs the clock window when it closes. It only
// uses the link's diagnostic slot; drop-report adds its report here.
func (s *Server) runDiagnostics(ctx context.Context) {
	ticker := time.NewTicker(diagnosticInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			s.flushDiagnostics()
			return
		case <-s.link.done:
			s.flushDiagnostics()
			return
		case now := <-ticker.C:
			if window, ok := s.link.heartbeat(now); ok {
				log.Print(window.line())
			}
		}
	}
}

// flushDiagnostics logs the last clock window once: from the timer when the
// link or the context ends, or from Close, which the process waits for.
func (s *Server) flushDiagnostics() {
	s.diagnosticsOnce.Do(func() { log.Print(s.link.finalClockWindow(time.Now()).line()) })
}
