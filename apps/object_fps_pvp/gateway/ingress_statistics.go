package gateway

import (
	"fmt"
	"strings"
	"time"
)

// Ingress statistics v1 (v7 batch 08c): what became of every datagram the
// Gateway read and every command it handed to the runtime link. Two lines:
//
//	player ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> <key>=<int>...
//	gateway ingress statistics version=1 window_ms=<int> final=<0|1> <key>=<int>...
//
// Each line counts what happened since the previous line of the same player
// (or of the Gateway); final=1 marks the last one. Keys are
// "[fwd_|drop_]<reason>[_<kind>]_<unit>": fwd_ was handed on to the Match,
// which classifies it; drop_ is a discard the Gateway executed. New keys are
// only ever appended. The golden lines live in
// tests/object_fps_pvp/fixtures/ingress_v1.
const ingressStatisticsVersion = 1

// Whether the Match received the item.
type ingressOutcome uint8

const (
	ingressObserved  ingressOutcome = iota // neither: an input to a conservation
	ingressForwarded                       // fwd_
	ingressDropped                         // drop_
)

// Why the Gateway forwarded or dropped an item.
type ingressReason uint8

const (
	reasonReceived ingressReason = iota
	// Datagram outcomes of a session (IG).
	reasonMain
	reasonRejectedLane
	reasonWrittenDuplicate
	reasonStaleSequence
	reasonAdmitted
	reasonHanded
	reasonRateLimited
	reasonUnauthorized
	reasonMalformed
	reasonInactive
	reasonInvalid
	reasonLinkRejected
	reasonUnknownKind
	reasonAdmissionUnknown
	// Commands on the runtime link (I0).
	reasonHandedMain
	reasonHandedRejectedLane
	reasonLinkWrittenMain
	reasonLinkWrittenRejectedLane
	reasonMergedDuplicate
	reasonResolvedTrim
	reasonRejectedLaneOverflow
	reasonDiscardedOnLeave
	reasonClearedOnClose
	reasonAbandonedOnClose
	// Attributes of forwarded commands and windows, outside the conservations.
	reasonResolvedAtWrite
	reasonRetentionExpired
	reasonRoutedEpochMismatch
	reasonRoutedFutureLimit
	reasonRoutedConflict
	reasonRoutedLinkCap
	reasonRoutedWrittenConflict
	reasonRoutedRotation
	reasonRoutedLinkEpochMismatch
	reasonRoutedMalformed
	// Datagrams that belong to no session.
	reasonUndecodable
	reasonOtherVersion
	reasonRoomUnavailable
	reasonUnknownSession
)

// The datagram kind from the header; ingressAnyKind leaves it out of the key.
type ingressKind uint8

const (
	ingressAnyKind ingressKind = iota
	ingressHello
	ingressInput
	ingressActions
	ingressOther
)

type ingressUnit uint8

const (
	unitDatagrams ingressUnit = iota
	unitPackets
	unitInputs
	unitCommands
	unitBatches
	unitShots
)

type ingressKey struct {
	outcome ingressOutcome
	reason  ingressReason
	kind    ingressKind
	unit    ingressUnit
}

// The only spelling of each word in a key; the pv7 GatewayDropReport is to
// reuse these reason names.
var (
	ingressOutcomeNames = [...]string{ingressObserved: "", ingressForwarded: "fwd_", ingressDropped: "drop_"}
	ingressReasonNames  = [...]string{
		reasonReceived:                "received",
		reasonMain:                    "main",
		reasonRejectedLane:            "rejected_lane",
		reasonWrittenDuplicate:        "written_duplicate",
		reasonStaleSequence:           "stale_sequence",
		reasonAdmitted:                "admitted",
		reasonHanded:                  "handed",
		reasonRateLimited:             "rate_limited",
		reasonUnauthorized:            "unauthorized",
		reasonMalformed:               "malformed",
		reasonInactive:                "inactive",
		reasonInvalid:                 "invalid",
		reasonLinkRejected:            "link_rejected",
		reasonUnknownKind:             "unknown_kind",
		reasonAdmissionUnknown:        "admission_unknown",
		reasonHandedMain:              "handed_main",
		reasonHandedRejectedLane:      "handed_rejected_lane",
		reasonLinkWrittenMain:         "link_written_main",
		reasonLinkWrittenRejectedLane: "link_written_rejected_lane",
		reasonMergedDuplicate:         "merged_duplicate",
		reasonResolvedTrim:            "resolved_trim",
		reasonRejectedLaneOverflow:    "rejected_lane_overflow",
		reasonDiscardedOnLeave:        "discarded_on_leave",
		reasonClearedOnClose:          "cleared_on_close",
		reasonAbandonedOnClose:        "abandoned_on_close",
		reasonResolvedAtWrite:         "resolved_at_write",
		reasonRetentionExpired:        "retention_expired",
		reasonRoutedEpochMismatch:     "routed_epoch_mismatch",
		reasonRoutedFutureLimit:       "routed_future_limit",
		reasonRoutedConflict:          "routed_conflict",
		reasonRoutedLinkCap:           "routed_link_cap",
		reasonRoutedWrittenConflict:   "routed_written_conflict",
		reasonRoutedRotation:          "routed_rotation",
		reasonRoutedLinkEpochMismatch: "routed_link_epoch_mismatch",
		reasonRoutedMalformed:         "routed_malformed",
		reasonUndecodable:             "undecodable",
		reasonOtherVersion:            "other_version",
		reasonRoomUnavailable:         "room_unavailable",
		reasonUnknownSession:          "unknown_session",
	}
	ingressKindNames = [...]string{ingressAnyKind: "", ingressHello: "_hello", ingressInput: "_input",
		ingressActions: "_actions", ingressOther: "_other"}
	ingressUnitNames = [...]string{unitDatagrams: "_datagrams", unitPackets: "_packets", unitInputs: "_inputs",
		unitCommands: "_commands", unitBatches: "_batches", unitShots: "_shots"}
)

func ingressKeyName(k ingressKey) string {
	return ingressOutcomeNames[k.outcome] + ingressReasonNames[k.reason] + ingressKindNames[k.kind] +
		ingressUnitNames[k.unit]
}

// One player's counters, in line order.
type playerIngressCounter uint8

const (
	// IG: received_datagrams equals the sum of every *_packets counter below,
	// because each datagram of a session ends in exactly one outcome.
	playerReceivedDatagrams playerIngressCounter = iota
	playerFwdMainInputPackets
	playerFwdRejectedLaneInputPackets
	playerFwdWrittenDuplicateInputPackets // every command was already written
	playerFwdStaleSequenceInputPackets    // forwarded without committing the sequence, on either lane
	playerFwdAdmittedHelloPackets
	playerFwdHandedActionsPackets
	playerDropRateLimitedHelloPackets
	playerDropRateLimitedInputPackets
	playerDropRateLimitedActionsPackets
	playerDropRateLimitedOtherPackets
	playerDropUnauthorizedHelloPackets // the claimed session: unauthenticated
	playerDropUnauthorizedInputPackets
	playerDropUnauthorizedActionsPackets
	playerDropUnauthorizedOtherPackets
	playerDropStaleSequenceActionsPackets
	playerDropStaleSequenceOtherPackets
	playerDropMalformedHelloPackets // the claimed session: decoded before authentication
	playerDropMalformedInputPackets
	playerDropMalformedActionsPackets
	playerDropInactiveInputPackets
	playerDropInactiveActionsPackets
	playerDropInvalidActionsPackets
	playerDropLinkRejectedActionsPackets
	playerDropUnknownKindPackets
	playerDropAdmissionUnknownPackets
	playerDropInvalidActionsShots
	playerDropLinkRejectedActionsShots
	// I0: the handed commands (counted once, when input() or reject() returns
	// nil; a move between lanes is not counted again) equal the sum of the
	// nine counters after them.
	playerFwdHandedMainCommands
	playerFwdHandedRejectedLaneCommands
	playerFwdLinkWrittenMainCommands // after l.conn.Write succeeded
	playerFwdLinkWrittenRejectedLaneCommands
	playerFwdMergedDuplicateCommands
	playerFwdWrittenDuplicateCommands
	playerDropResolvedTrimCommands
	playerDropRejectedLaneOverflowCommands
	playerDropDiscardedOnLeaveCommands
	playerDropClearedOnCloseCommands
	playerDropAbandonedOnCloseCommands // taken for a write that failed
	// Attributes outside the conservations.
	playerFwdResolvedAtWriteCommands  // main-lane commands at or below the link's resolved cursor when written
	playerFwdRetentionExpiredCommands // copies forwarded because the written set no longer held them
	playerFwdRoutedEpochMismatchInputs
	playerFwdRoutedFutureLimitInputs
	playerFwdRoutedConflictInputs
	playerFwdRoutedLinkCapInputs
	playerFwdRoutedWrittenConflictInputs
	playerFwdRoutedRotationInputs // pending main windows moved by an epoch or life change
	// The link refused a window the Server routed to the main lane: its epoch
	// or life disagreed with the Server's (unreachable while acknowledge keeps
	// both in step), or it broke the input schema (unreachable after
	// DecodeInput). Both still reach the Match on the rejected lane.
	playerFwdRoutedLinkEpochMismatchInputs
	playerFwdRoutedMalformedInputs
	playerIngressCounterCount
)

var playerIngressKeys = [playerIngressCounterCount]ingressKey{
	playerReceivedDatagrams:                  {ingressObserved, reasonReceived, ingressAnyKind, unitDatagrams},
	playerFwdMainInputPackets:                {ingressForwarded, reasonMain, ingressInput, unitPackets},
	playerFwdRejectedLaneInputPackets:        {ingressForwarded, reasonRejectedLane, ingressInput, unitPackets},
	playerFwdWrittenDuplicateInputPackets:    {ingressForwarded, reasonWrittenDuplicate, ingressInput, unitPackets},
	playerFwdStaleSequenceInputPackets:       {ingressForwarded, reasonStaleSequence, ingressInput, unitPackets},
	playerFwdAdmittedHelloPackets:            {ingressForwarded, reasonAdmitted, ingressHello, unitPackets},
	playerFwdHandedActionsPackets:            {ingressForwarded, reasonHanded, ingressActions, unitPackets},
	playerDropRateLimitedHelloPackets:        {ingressDropped, reasonRateLimited, ingressHello, unitPackets},
	playerDropRateLimitedInputPackets:        {ingressDropped, reasonRateLimited, ingressInput, unitPackets},
	playerDropRateLimitedActionsPackets:      {ingressDropped, reasonRateLimited, ingressActions, unitPackets},
	playerDropRateLimitedOtherPackets:        {ingressDropped, reasonRateLimited, ingressOther, unitPackets},
	playerDropUnauthorizedHelloPackets:       {ingressDropped, reasonUnauthorized, ingressHello, unitPackets},
	playerDropUnauthorizedInputPackets:       {ingressDropped, reasonUnauthorized, ingressInput, unitPackets},
	playerDropUnauthorizedActionsPackets:     {ingressDropped, reasonUnauthorized, ingressActions, unitPackets},
	playerDropUnauthorizedOtherPackets:       {ingressDropped, reasonUnauthorized, ingressOther, unitPackets},
	playerDropStaleSequenceActionsPackets:    {ingressDropped, reasonStaleSequence, ingressActions, unitPackets},
	playerDropStaleSequenceOtherPackets:      {ingressDropped, reasonStaleSequence, ingressOther, unitPackets},
	playerDropMalformedHelloPackets:          {ingressDropped, reasonMalformed, ingressHello, unitPackets},
	playerDropMalformedInputPackets:          {ingressDropped, reasonMalformed, ingressInput, unitPackets},
	playerDropMalformedActionsPackets:        {ingressDropped, reasonMalformed, ingressActions, unitPackets},
	playerDropInactiveInputPackets:           {ingressDropped, reasonInactive, ingressInput, unitPackets},
	playerDropInactiveActionsPackets:         {ingressDropped, reasonInactive, ingressActions, unitPackets},
	playerDropInvalidActionsPackets:          {ingressDropped, reasonInvalid, ingressActions, unitPackets},
	playerDropLinkRejectedActionsPackets:     {ingressDropped, reasonLinkRejected, ingressActions, unitPackets},
	playerDropUnknownKindPackets:             {ingressDropped, reasonUnknownKind, ingressAnyKind, unitPackets},
	playerDropAdmissionUnknownPackets:        {ingressDropped, reasonAdmissionUnknown, ingressAnyKind, unitPackets},
	playerDropInvalidActionsShots:            {ingressDropped, reasonInvalid, ingressActions, unitShots},
	playerDropLinkRejectedActionsShots:       {ingressDropped, reasonLinkRejected, ingressActions, unitShots},
	playerFwdHandedMainCommands:              {ingressForwarded, reasonHandedMain, ingressAnyKind, unitCommands},
	playerFwdHandedRejectedLaneCommands:      {ingressForwarded, reasonHandedRejectedLane, ingressAnyKind, unitCommands},
	playerFwdLinkWrittenMainCommands:         {ingressForwarded, reasonLinkWrittenMain, ingressAnyKind, unitCommands},
	playerFwdLinkWrittenRejectedLaneCommands: {ingressForwarded, reasonLinkWrittenRejectedLane, ingressAnyKind, unitCommands},
	playerFwdMergedDuplicateCommands:         {ingressForwarded, reasonMergedDuplicate, ingressAnyKind, unitCommands},
	playerFwdWrittenDuplicateCommands:        {ingressForwarded, reasonWrittenDuplicate, ingressAnyKind, unitCommands},
	playerDropResolvedTrimCommands:           {ingressDropped, reasonResolvedTrim, ingressAnyKind, unitCommands},
	playerDropRejectedLaneOverflowCommands:   {ingressDropped, reasonRejectedLaneOverflow, ingressAnyKind, unitCommands},
	playerDropDiscardedOnLeaveCommands:       {ingressDropped, reasonDiscardedOnLeave, ingressAnyKind, unitCommands},
	playerDropClearedOnCloseCommands:         {ingressDropped, reasonClearedOnClose, ingressAnyKind, unitCommands},
	playerDropAbandonedOnCloseCommands:       {ingressDropped, reasonAbandonedOnClose, ingressAnyKind, unitCommands},
	playerFwdResolvedAtWriteCommands:         {ingressForwarded, reasonResolvedAtWrite, ingressAnyKind, unitCommands},
	playerFwdRetentionExpiredCommands:        {ingressForwarded, reasonRetentionExpired, ingressAnyKind, unitCommands},
	playerFwdRoutedEpochMismatchInputs:       {ingressForwarded, reasonRoutedEpochMismatch, ingressAnyKind, unitInputs},
	playerFwdRoutedFutureLimitInputs:         {ingressForwarded, reasonRoutedFutureLimit, ingressAnyKind, unitInputs},
	playerFwdRoutedConflictInputs:            {ingressForwarded, reasonRoutedConflict, ingressAnyKind, unitInputs},
	playerFwdRoutedLinkCapInputs:             {ingressForwarded, reasonRoutedLinkCap, ingressAnyKind, unitInputs},
	playerFwdRoutedWrittenConflictInputs:     {ingressForwarded, reasonRoutedWrittenConflict, ingressAnyKind, unitInputs},
	playerFwdRoutedRotationInputs:            {ingressForwarded, reasonRoutedRotation, ingressAnyKind, unitInputs},
	playerFwdRoutedLinkEpochMismatchInputs:   {ingressForwarded, reasonRoutedLinkEpochMismatch, ingressAnyKind, unitInputs},
	playerFwdRoutedMalformedInputs:           {ingressForwarded, reasonRoutedMalformed, ingressAnyKind, unitInputs},
}

type playerIngressCounts [playerIngressCounterCount]uint64

// The Gateway's counters, in line order.
type gatewayIngressCounter uint8

const (
	// received_datagrams equals the four drops below plus the players'
	// received_datagrams: every datagram read either belongs to a session or
	// is dropped here.
	gatewayReceivedDatagrams gatewayIngressCounter = iota
	gatewayDropUndecodableDatagrams
	gatewayDropOtherVersionDatagrams
	gatewayDropRoomUnavailableDatagrams
	gatewayDropUnknownSessionDatagrams
	// T3: the players' drop_rate_limited_*_packets sum to this. It is the
	// window's share of the Session limiter's rate_limited_packets.
	gatewayDropRateLimitedPackets
	gatewayIngressCounterCount
)

var gatewayIngressKeys = [gatewayIngressCounterCount]ingressKey{
	gatewayReceivedDatagrams:            {ingressObserved, reasonReceived, ingressAnyKind, unitDatagrams},
	gatewayDropUndecodableDatagrams:     {ingressDropped, reasonUndecodable, ingressAnyKind, unitDatagrams},
	gatewayDropOtherVersionDatagrams:    {ingressDropped, reasonOtherVersion, ingressAnyKind, unitDatagrams},
	gatewayDropRoomUnavailableDatagrams: {ingressDropped, reasonRoomUnavailable, ingressAnyKind, unitDatagrams},
	gatewayDropUnknownSessionDatagrams:  {ingressDropped, reasonUnknownSession, ingressAnyKind, unitDatagrams},
	gatewayDropRateLimitedPackets:       {ingressDropped, reasonRateLimited, ingressAnyKind, unitPackets},
}

type gatewayIngressCounts [gatewayIngressCounterCount]uint64

func playerIngressStatisticsLine(player uint64, window time.Duration, final bool, counts *playerIngressCounts) string {
	head := fmt.Sprintf("player ingress statistics version=%d player=%d window_ms=%d final=%d",
		ingressStatisticsVersion, player, window.Milliseconds(), finalFlag(final))
	return ingressLine(head, playerIngressKeys[:], counts[:])
}

func gatewayIngressStatisticsLine(window time.Duration, final bool, counts *gatewayIngressCounts) string {
	head := fmt.Sprintf("gateway ingress statistics version=%d window_ms=%d final=%d",
		ingressStatisticsVersion, window.Milliseconds(), finalFlag(final))
	return ingressLine(head, gatewayIngressKeys[:], counts[:])
}

func ingressLine(head string, keys []ingressKey, values []uint64) string {
	var b strings.Builder
	b.WriteString(head)
	for i, key := range keys {
		fmt.Fprintf(&b, " %s=%d", ingressKeyName(key), values[i])
	}
	return b.String()
}

func finalFlag(final bool) int {
	if final {
		return 1
	}
	return 0
}
