// Package httpserver supplies HTTP transport defaults. Products own all routes.
package httpserver

import (
	"net/http"
	"time"
)

func New(addr string, handler http.Handler) *http.Server {
	return &http.Server{Addr: addr, Handler: handler, ReadHeaderTimeout: 3 * time.Second,
		ReadTimeout: 5 * time.Second, WriteTimeout: 5 * time.Second, IdleTimeout: 30 * time.Second,
		MaxHeaderBytes: 8192}
}
