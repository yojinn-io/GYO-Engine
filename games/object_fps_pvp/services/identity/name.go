package identity

import (
	"strings"
	"unicode"
)

// MaxNameRunes is the most code points a player's name keeps (display-name
// declaration L1): at most 128 bytes of UTF-8.
const MaxNameRunes = 32

// NormalizeName is the one rule a player's name follows (display-name
// declaration 4.2): control and format characters and line or paragraph
// separators are dropped, each run of spaces becomes one space and the ends
// lose theirs, and a name longer than MaxNameRunes is cut after that many
// code points. Bytes that are not UTF-8 count as U+FFFD. An empty result is
// an empty name, which a display shows as its fallback.
func NormalizeName(candidate string) string {
	var name strings.Builder
	runes := 0
	space := false
	for _, r := range candidate {
		if unicode.In(r, unicode.Cc, unicode.Cf, unicode.Zl, unicode.Zp) {
			continue
		}
		if unicode.IsSpace(r) {
			space = true
			continue
		}
		if space && name.Len() > 0 {
			if runes+1 >= MaxNameRunes {
				break
			}
			name.WriteByte(' ')
			runes++
		}
		space = false
		if runes == MaxNameRunes {
			break
		}
		name.WriteRune(r)
		runes++
	}
	return name.String()
}
