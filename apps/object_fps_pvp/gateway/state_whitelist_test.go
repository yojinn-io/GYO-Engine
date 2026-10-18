package gateway

import (
	"go/ast"
	"go/parser"
	"go/token"
	"path/filepath"
	"reflect"
	"sort"
	"strings"
	"testing"
)

// The guard of the Gateway's Design Invariants (network-architecture.zh-Hant.md
// §7 "Design Invariants", protocol-v7 §2a "Gateway 的約束", v7.1 D-14, D105):
// every field of the Gateway's state, every package variable and every map or
// slice a goroutine keeps in a local is in exactly one allowed class. A new
// field, variable or goroutine local fails this test until it is classified;
// adding one is an Architecture Delta that needs the user's consent, and no
// class holds a copy of the Match's decision state. What a field is used for
// (a judgement by a game item's identity or content) cannot be seen here: that
// is left to review.
const (
	classConnection = "connection and session"                // sockets, the runtime link, sessions, reservations, transport sequences
	classLifecycle  = "lifecycle"                             // phases, availability, once and close flags
	classConfig     = "configuration copy"                    // Config and the Match's Ready
	classQueue      = "bounded outbound or IPC queue"         // overflow counted and reported
	classResultSlot = "results slot"                          // the last results send and at most one waiting frame (pv7 §2a)
	classSync       = "synchronization"                       // mutexes and wake channels
	classDiagnostic = "diagnostic state"                      // clock sync, echo, drop ledger, statistics; never read by a decision
	classItemDiag   = "diagnostics carried with a queue item" // the identity a queued item's drop is reported with (N3)
	classPerCall    = "per-call context"                      // what one call keeps while it handles one datagram, frame or request
	classTestSeam   = "test seam"                             // package variables and an injected clock; production uses the default
	classTable      = "immutable table"                       // key lists, reason maps, sentinel errors
)

var allowedClasses = map[string]bool{classConnection: true, classLifecycle: true, classConfig: true,
	classQueue: true, classResultSlot: true, classSync: true, classDiagnostic: true, classItemDiag: true,
	classPerCall: true, classTestSeam: true, classTable: true}

// stateRoots holds one value of every struct type the package declares; the
// test also checks that this list names them all.
var stateRoots = []any{
	Config{}, Server{}, reservation{}, outbound{}, snapshotFrame{}, queuedControl{}, playerIngressLine{},
	joinRequest{}, runtimeLink{}, queuedMessage{}, linkWrite{}, dataKey{}, resultSlot{}, dropEvent{},
	dropLedger{}, dropStream{}, clockSync{}, clockPoint{}, clockStamp{}, clockWindow{}, sessionEcho{},
	intervalStats{}, intervalWindow{}, sendStatistics{}, ingressKey{},
}

// fieldClasses: "Type.field" for a field of a named type of this package, and
// "Type.field<path>" for a field of an anonymous struct inside one.
var fieldClasses = map[string]string{
	"Config.HTTPAddress": classConfig, "Config.UDPAddress": classConfig, "Config.RuntimeAddress": classConfig,
	"Config.AdvertiseIP": classConfig,

	"Server.config": classConfig, "Server.mu": classSync, "Server.available": classLifecycle,
	"Server.created": classLifecycle, "Server.ready": classConfig, "Server.link": classConnection,
	"Server.players": classConnection, "Server.sessions": classConnection, "Server.requests": classConnection,
	"Server.nextPlayer": classConnection, "Server.http": classConnection, "Server.listener": classConnection,
	"Server.udp": classConnection, "Server.controlOut": classQueue, "Server.snapshotOut": classQueue,
	"Server.resultWake": classSync, "Server.resultsForInactive": classDiagnostic, "Server.closeOnce": classLifecycle,
	"Server.diagnosticsOnce": classLifecycle, "Server.snapshotReplacements": classDiagnostic,
	"Server.datagram": classPerCall, "Server.rateAcceptedPackets": classDiagnostic,
	"Server.rateRejectedPackets": classDiagnostic, "Server.maxSessionWindowPackets": classDiagnostic,
	"Server.ingress": classDiagnostic, "Server.playerIngress": classDiagnostic,
	"Server.ingressWindowStart": classDiagnostic, "Server.ingressPlayers": classDiagnostic,
	"Server.ingressFinal": classDiagnostic, "Server.finalOnce": classLifecycle,

	"reservation.results": classResultSlot, "reservation.packetWindow": classDiagnostic,
	"reservation.packetCount": classDiagnostic, "reservation.session": classConnection,
	"reservation.playerID": classConnection, "reservation.requestID": classConnection,
	"reservation.phase": classLifecycle, "reservation.joinStarted": classLifecycle,
	"reservation.outSequence": classConnection, "reservation.received": classDiagnostic,
	"reservation.lastReceived": classDiagnostic, "reservation.sent": classDiagnostic,
	"reservation.echo": classDiagnostic,

	"outbound.actionPlayer": classPerCall, "outbound.peer": classPerCall, "outbound.packet": classPerCall,
	"outbound.player": classPerCall, "outbound.writeFailed": classPerCall, "outbound.sequence": classPerCall,
	"outbound.tick":         classPerCall,
	"snapshotFrame.payload": classQueue, "snapshotFrame.tick": classQueue,
	"queuedControl.p": classQueue, "queuedControl.kind": classQueue, "queuedControl.payload": classQueue,
	"playerIngressLine.player": classDiagnostic, "playerIngressLine.line": classDiagnostic,
	"joinRequest.RequestID": classPerCall, "joinRequest.ProtocolVersion": classPerCall,

	"runtimeLink.conn": classConnection, "runtimeLink.mu": classSync, "runtimeLink.wake": classSync,
	"runtimeLink.done": classLifecycle, "runtimeLink.closed": classLifecycle, "runtimeLink.once": classLifecycle,
	"runtimeLink.maxWriteAge": classDiagnostic, "runtimeLink.queue": classQueue,
	"runtimeLink.queuedControls": classQueue, "runtimeLink.queuedData": classQueue,
	"runtimeLink.ingress": classDiagnostic, "runtimeLink.actionStats": classDiagnostic,
	"runtimeLink.writerStarted": classLifecycle, "runtimeLink.writerExited": classLifecycle,
	"runtimeLink.writerOnce": classLifecycle, "runtimeLink.diagnostic": classDiagnostic,
	"runtimeLink.diagnosticWake": classSync, "runtimeLink.clockMu": classSync, "runtimeLink.clock": classDiagnostic,
	"runtimeLink.now": classTestSeam, "runtimeLink.takes": classDiagnostic, "runtimeLink.drops": classDiagnostic,
	"runtimeLink.dropPass": classDiagnostic, "runtimeLink.lastDropPass": classDiagnostic,
	"queuedMessage.message": classQueue, "queuedMessage.input": classQueue, "queuedMessage.actions": classQueue,
	"queuedMessage.player": classQueue, "queuedMessage.commands": classQueue, "queuedMessage.event": classItemDiag,
	"linkWrite.message": classQueue, "linkWrite.lane": classQueue, "linkWrite.player": classQueue,
	"linkWrite.commands": classQueue,
	"dataKey.player":     classQueue, "dataKey.actions": classQueue,
	"resultSlot.pending": classResultSlot, "resultSlot.nextSend": classResultSlot, "resultSlot.event": classItemDiag,

	"dropEvent.reason": classDiagnostic, "dropEvent.at": classDiagnostic, "dropEvent.transport": classDiagnostic,
	"dropEvent.epoch": classDiagnostic, "dropEvent.life": classDiagnostic, "dropEvent.firstCommand": classDiagnostic,
	"dropEvent.lastCommand": classDiagnostic, "dropEvent.firstAction": classDiagnostic,
	"dropEvent.lastAction": classDiagnostic, "dropEvent.tick": classDiagnostic, "dropEvent.items": classDiagnostic,
	"dropLedger.mu": classSync, "dropLedger.streams": classDiagnostic, "dropLedger.waiting": classDiagnostic,
	"dropLedger.ended": classDiagnostic, "dropLedger.merged": classDiagnostic, "dropLedger.closed": classDiagnostic,
	"dropStream.totals": classDiagnostic, "dropStream.samples": classDiagnostic, "dropStream.omitted": classDiagnostic,
	"dropStream.sequence": classDiagnostic, "dropStream.dirty": classDiagnostic, "dropStream.final": classDiagnostic,

	"clockSync.windowStartUs": classDiagnostic, "clockSync.lastTransmitUs": classDiagnostic,
	"clockSync.transmits": classDiagnostic, "clockSync.lastPeerTransmit": classDiagnostic,
	"clockSync.echoTransmit": classDiagnostic, "clockSync.echoReceive": classDiagnostic,
	"clockSync.echoed": classDiagnostic, "clockSync.best": classDiagnostic, "clockSync.delayMaxUs": classDiagnostic,
	"clockSync.samples": classDiagnostic, "clockSync.invalid": classDiagnostic, "clockSync.negative": classDiagnostic,
	"clockSync.replaced": classDiagnostic, "clockSync.segment": classDiagnostic,
	"clockPoint.atUs": classDiagnostic, "clockPoint.offsetUs": classDiagnostic, "clockPoint.delayUs": classDiagnostic,
	"clockStamp.origin": classDiagnostic, "clockStamp.receive": classDiagnostic, "clockStamp.transmit": classDiagnostic,
	"clockWindow.windowUs": classDiagnostic, "clockWindow.samples": classDiagnostic,
	"clockWindow.invalid": classDiagnostic, "clockWindow.negativeDelay": classDiagnostic,
	"clockWindow.replaced": classDiagnostic, "clockWindow.offsetUs": classDiagnostic,
	"clockWindow.delayMinUs": classDiagnostic, "clockWindow.delayMaxUs": classDiagnostic,
	"clockWindow.driftPpm": classDiagnostic, "clockWindow.jump": classDiagnostic, "clockWindow.final": classDiagnostic,
	"sessionEcho.sync":   classDiagnostic,
	"intervalStats.last": classDiagnostic, "intervalStats.count": classDiagnostic,
	"intervalStats.intervals": classDiagnostic,
	"intervalWindow.count":    classDiagnostic, "intervalWindow.summary": classDiagnostic,
	"intervalWindow.minimum": classDiagnostic,
	"sendStatistics.results": classDiagnostic, "sendStatistics.snapshots": classDiagnostic,
	"ingressKey.outcome": classTable, "ingressKey.reason": classTable, "ingressKey.kind": classTable,
	"ingressKey.unit": classTable,
}

// interfaceFields: the interface-typed fields, whose dynamic value this test
// cannot walk; each names what it holds.
var interfaceFields = map[string]string{
	"Server.listener": "the HTTP listener (net.Listener): a connection handle, no game state",
}

// packageVariables: every package-level variable of the Gateway's packages.
var packageVariables = map[string]string{
	"gateway.diagnosticInterval": classTestSeam, "gateway.gracefulWriterWait": classTestSeam,
	"gateway.runtimeLiveness": classTestSeam, "gateway.runtimeWrite": classTestSeam,
	"gateway.sessionAdmit": classTestSeam, "gateway.sessionHello": classTestSeam, "gateway.udpWrite": classTestSeam,
	"gateway.writerIdleWait":     classTestSeam,
	"gateway.gatewayClockOrigin": classDiagnostic, // the G clock domain's one reference point (pv7 §2)
	"gateway.dropReasonKey":      classTable, "gateway.dropReasons": classTable, "gateway.gatewayDropReason": classTable,
	"gateway.playerDropReason": classTable, "gateway.gatewayIngressKeys": classTable,
	"gateway.playerIngressKeys": classTable, "gateway.ingressKindNames": classTable,
	"gateway.ingressOutcomeNames": classTable, "gateway.ingressReasonNames": classTable,
	"gateway.ingressUnitNames": classTable, "gateway.noIntervals": classTable,
	"gateway.rateLimitedPackets": classTable, "gateway.staleSequencePackets": classTable,
	"gateway.unauthorizedPackets": classTable, "gateway.errLinkClosing": classTable,
	"gateway.errRuntimeDisconnected": classTable,
	"adapter.ErrActions":             classTable, "adapter.ErrInput": classTable,
}

// goroutineLocals: "function.variable" for a map or slice a function started
// with go (or a closure started with go, by its enclosing function) declares.
var goroutineLocals = map[string]string{
	"receiveUDP.buffer": classPerCall, // the read buffer, reused for each datagram
	"sendUDP.players":   classPerCall, // the active players of one snapshot frame, rebuilt for each frame
}

func TestEveryGatewayStateFieldIsClassified(t *testing.T) {
	declared := declaredStructTypes(t)
	rooted := map[string]bool{}
	for _, value := range stateRoots {
		rooted[reflect.TypeOf(value).Name()] = true
	}
	for _, name := range declared {
		if !rooted[name] {
			t.Errorf("struct type %s is not in stateRoots: classify its fields (network-architecture §7 Design Invariants)", name)
		}
	}
	seen := map[string]bool{}
	pkg := reflect.TypeOf(Server{}).PkgPath()
	for _, value := range stateRoots {
		typ := reflect.TypeOf(value)
		walkState(t, typ, typ.Name(), pkg, seen, map[reflect.Type]bool{})
	}
	for key, class := range fieldClasses {
		if !seen[key] {
			t.Errorf("classified field %s no longer exists: remove it from fieldClasses", key)
		}
		if !allowedClasses[class] {
			t.Errorf("field %s has class %q, which is not an allowed class", key, class)
		}
	}
}

// walkState checks every field of a struct type of this package (or of an
// anonymous struct inside one) and follows pointers, maps (keys and elements),
// slices, arrays and channels into further types of this package.
func walkState(t *testing.T, typ reflect.Type, path, pkg string, seen map[string]bool, visiting map[reflect.Type]bool) {
	t.Helper()
	switch typ.Kind() {
	case reflect.Pointer, reflect.Slice, reflect.Array, reflect.Chan:
		walkState(t, typ.Elem(), path, pkg, seen, visiting)
	case reflect.Map:
		walkState(t, typ.Key(), path, pkg, seen, visiting)
		walkState(t, typ.Elem(), path, pkg, seen, visiting)
	case reflect.Struct:
		local := typ.Name() == "" || typ.PkgPath() == pkg
		if !local || visiting[typ] {
			return // another package's type (its owner classifies it), or a type already being walked
		}
		visiting[typ] = true
		defer delete(visiting, typ)
		for i := range typ.NumField() {
			field := typ.Field(i)
			key := path + "." + field.Name
			if typ.Name() != "" {
				key = typ.Name() + "." + field.Name
			}
			seen[key] = true
			if _, ok := fieldClasses[key]; !ok {
				t.Errorf("field %s (%s) is not classified: classify it in fieldClasses; a new Gateway state field "+
					"is an Architecture Delta (network-architecture §7 Design Invariants)", key, field.Type)
			}
			if field.Type.Kind() == reflect.Interface {
				if _, ok := interfaceFields[key]; !ok {
					t.Errorf("field %s is an interface: its dynamic state cannot be walked; use a concrete type or "+
						"name what it holds in interfaceFields", key)
				}
			}
			walkState(t, field.Type, key, pkg, seen, visiting)
		}
	}
}

func TestEveryGatewayPackageVariableIsClassified(t *testing.T) {
	found := map[string]bool{}
	for _, dir := range []string{".", "adapter"} {
		pkg := filepath.Base(mustAbs(t, dir))
		for _, file := range parsePackage(t, dir) {
			for _, decl := range file.Decls {
				gen, ok := decl.(*ast.GenDecl)
				if !ok || gen.Tok != token.VAR {
					continue
				}
				for _, spec := range gen.Specs {
					for _, name := range spec.(*ast.ValueSpec).Names {
						key := pkg + "." + name.Name
						found[key] = true
						if class, ok := packageVariables[key]; !ok {
							t.Errorf("package variable %s is not classified: classify it in packageVariables", key)
						} else if !allowedClasses[class] {
							t.Errorf("package variable %s has class %q, which is not an allowed class", key, class)
						}
					}
				}
			}
		}
	}
	for key := range packageVariables {
		if !found[key] {
			t.Errorf("classified package variable %s no longer exists: remove it", key)
		}
	}
}

func TestEveryGoroutineLocalMapOrSliceIsClassified(t *testing.T) {
	files := parsePackage(t, ".")
	functions := map[string]*ast.FuncDecl{}
	for _, file := range files {
		for _, decl := range file.Decls {
			if fn, ok := decl.(*ast.FuncDecl); ok {
				functions[fn.Name.Name] = fn
			}
		}
	}
	found := map[string]bool{}
	for _, file := range files {
		for _, decl := range file.Decls {
			enclosing, ok := decl.(*ast.FuncDecl)
			if !ok || enclosing.Body == nil {
				continue
			}
			ast.Inspect(enclosing.Body, func(node ast.Node) bool {
				statement, ok := node.(*ast.GoStmt)
				if !ok {
					return true
				}
				name, body := enclosing.Name.Name, (*ast.BlockStmt)(nil)
				switch fn := statement.Call.Fun.(type) {
				case *ast.FuncLit:
					body = fn.Body
				case *ast.Ident:
					if decl := functions[fn.Name]; decl != nil {
						name, body = fn.Name, decl.Body
					}
				case *ast.SelectorExpr:
					if decl := functions[fn.Sel.Name]; decl != nil {
						name, body = fn.Sel.Name, decl.Body
					}
				}
				if body == nil {
					t.Errorf("go statement in %s starts a function this test cannot read", enclosing.Name.Name)
					return true
				}
				for _, local := range mapOrSliceLocals(body) {
					key := name + "." + local
					found[key] = true
					if class, ok := goroutineLocals[key]; !ok {
						t.Errorf("goroutine local %s (a map or slice) is not classified: classify it in goroutineLocals", key)
					} else if !allowedClasses[class] {
						t.Errorf("goroutine local %s has class %q, which is not an allowed class", key, class)
					}
				}
				return true
			})
		}
	}
	for key := range goroutineLocals {
		if !found[key] {
			t.Errorf("classified goroutine local %s no longer exists: remove it", key)
		}
	}
}

// mapOrSliceLocals: the variables a function body declares as a map or slice
// (make, a composite literal or a typed var declaration).
func mapOrSliceLocals(body *ast.BlockStmt) []string {
	isMapOrSlice := func(expr ast.Expr) bool {
		switch x := expr.(type) {
		case *ast.MapType, *ast.ArrayType:
			return true
		case *ast.CompositeLit:
			switch x.Type.(type) {
			case *ast.MapType, *ast.ArrayType:
				return true
			}
		case *ast.CallExpr:
			if id, ok := x.Fun.(*ast.Ident); ok && id.Name == "make" && len(x.Args) > 0 {
				switch x.Args[0].(type) {
				case *ast.MapType, *ast.ArrayType:
					return true
				}
			}
		}
		return false
	}
	var names []string
	ast.Inspect(body, func(node ast.Node) bool {
		switch x := node.(type) {
		case *ast.AssignStmt:
			for i, rhs := range x.Rhs {
				if i < len(x.Lhs) && isMapOrSlice(rhs) {
					if id, ok := x.Lhs[i].(*ast.Ident); ok {
						names = append(names, id.Name)
					}
				}
			}
		case *ast.ValueSpec:
			if x.Type != nil && isMapOrSlice(x.Type) {
				for _, name := range x.Names {
					names = append(names, name.Name)
				}
			}
		}
		return true
	})
	return names
}

func declaredStructTypes(t *testing.T) []string {
	t.Helper()
	var names []string
	for _, file := range parsePackage(t, ".") {
		for _, decl := range file.Decls {
			gen, ok := decl.(*ast.GenDecl)
			if !ok || gen.Tok != token.TYPE {
				continue
			}
			for _, spec := range gen.Specs {
				if typeSpec := spec.(*ast.TypeSpec); isStruct(typeSpec.Type) {
					names = append(names, typeSpec.Name.Name)
				}
			}
		}
	}
	sort.Strings(names)
	return names
}

func isStruct(expr ast.Expr) bool {
	_, ok := expr.(*ast.StructType)
	return ok
}

func parsePackage(t *testing.T, dir string) []*ast.File {
	t.Helper()
	paths, err := filepath.Glob(filepath.Join(dir, "*.go"))
	if err != nil {
		t.Fatal(err)
	}
	var files []*ast.File
	set := token.NewFileSet()
	for _, path := range paths {
		if strings.HasSuffix(path, "_test.go") {
			continue
		}
		file, err := parser.ParseFile(set, path, nil, 0)
		if err != nil {
			t.Fatal(err)
		}
		files = append(files, file)
	}
	return files
}

func mustAbs(t *testing.T, dir string) string {
	t.Helper()
	path, err := filepath.Abs(dir)
	if err != nil {
		t.Fatal(err)
	}
	return path
}
