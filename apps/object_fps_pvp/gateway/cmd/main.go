package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"io"
	"log"
	"os"
	"os/signal"
	"runtime"
	"syscall"
	"time"

	"gyo.local/object_fps_pvp/gateway"
)

func main() {
	var config gateway.Config
	flag.StringVar(&config.HTTPAddress, "http", "0.0.0.0:8080", "HTTP lobby listen address")
	flag.StringVar(&config.UDPAddress, "udp", "0.0.0.0:27015", "UDP data listen address")
	flag.StringVar(&config.RuntimeAddress, "runtime", "127.0.0.1:27016", "local Match IPC address")
	flag.StringVar(&config.AdvertiseIP, "advertise-ip", "127.0.0.1", "IPv4 address reachable by clients; set the host LAN IP for LAN play")
	logPath := flag.String("log", "", "also append the log to this file")
	flag.Parse()
	// Local wall clock with microseconds: logs of different hosts are compared by wall time only.
	log.SetFlags(log.Ldate | log.Ltime | log.Lmicroseconds)
	if *logPath != "" {
		file, err := os.OpenFile(*logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
		if err != nil {
			log.Fatal(err)
		}
		defer file.Close()
		log.SetOutput(io.MultiWriter(os.Stderr, file))
	}
	logStartup()
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()
	server, err := gateway.New(ctx, config)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("Object FPS Gateway HTTP=%s UDP=%s advertise=%s runtime=%s", server.HTTPAddress(), server.UDPAddress(), config.AdvertiseIP, config.RuntimeAddress)
	if err := server.Serve(ctx); err != nil {
		log.Fatal(err)
	}
}

// The build is identified by the executable's SHA-256 (no version is compiled in).
func logStartup() {
	digest := "unavailable"
	if path, err := os.Executable(); err == nil {
		if bytes, err := os.ReadFile(path); err == nil {
			sum := sha256.Sum256(bytes)
			digest = hex.EncodeToString(sum[:])
		}
	}
	zone, offset := time.Now().Zone()
	log.Printf("Object FPS Gateway start executable_sha256=%s go=%s os=%s/%s time_zone=%s utc_offset_s=%d args=%q",
		digest, runtime.Version(), runtime.GOOS, runtime.GOARCH, zone, offset, os.Args[1:])
}
