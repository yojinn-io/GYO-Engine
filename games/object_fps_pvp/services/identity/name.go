package identity

import (
	"strings"
	"unicode"
)

// MaxNameRunes is the most code points a player's name keeps (display-name
// declaration L1): at most 128 bytes of UTF-8.
const MaxNameRunes = 32

// SanitizeName is the one rule a player's name follows (display-name
// declaration 4.2), in steps: every space (tab, line break and separator
// included) becomes a plain space and other control and format characters
// are dropped; runs of spaces become one and the ends lose theirs; a name
// longer than MaxNameRunes is cut after that many code points (and loses a
// space it then ends with). Bytes that are not UTF-8 count as U+FFFD. An
// empty result is an empty name, which a display shows as its fallback.
func SanitizeName(candidate string) string {
	kept := strings.Map(func(r rune) rune {
		if unicode.IsSpace(r) {
			return ' '
		}
		if unicode.In(r, unicode.Cc, unicode.Cf) {
			return -1
		}
		return r
	}, candidate)
	name := strings.Join(strings.Fields(kept), " ")
	if runes := []rune(name); len(runes) > MaxNameRunes {
		name = strings.TrimRight(string(runes[:MaxNameRunes]), " ")
	}
	return name
}
