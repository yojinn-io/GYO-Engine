package main

import (
	"context"
	"flag"
	"log"
	"os"
	"os/signal"
	"syscall"

	"gyo.local/object_fps_pvp/gateway"
)

func main() {
	var config gateway.Config
	flag.StringVar(&config.HTTPAddress, "http", "0.0.0.0:8080", "HTTP lobby listen address")
	flag.StringVar(&config.UDPAddress, "udp", "0.0.0.0:27015", "UDP data listen address")
	flag.StringVar(&config.RuntimeAddress, "runtime", "127.0.0.1:27016", "local Match IPC address")
	flag.StringVar(&config.AdvertiseIP, "advertise-ip", "127.0.0.1", "IPv4 address reachable by clients; set the host LAN IP for LAN play")
	flag.Parse()
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
