package identity

import (
	"strings"
	"testing"
	"unicode/utf8"
)

func TestNormalizeName(t *testing.T) {
	long := strings.Repeat("名", MaxNameRunes)
	cases := []struct{ name, candidate, want string }{
		{"kept", "Kuro-MacBook-Pro", "Kuro-MacBook-Pro"},
		{"Japanese and Chinese", "からすの電腦", "からすの電腦"},
		{"control characters", "a\x00b\x07c\x1bd\x7fe", "abcde"},
		{"tab and newline are control characters", "a\tb\nc\r\nd", "abcd"},
		{"right-to-left override", "evil\u202eexe.txt", "evilexe.txt"},
		{"zero-width and BOM", "a\u200bb\u200dc\ufeffd", "abcd"},
		{"line and paragraph separators", "a\u2028b\u2029c", "abc"},
		{"runs of spaces", "a  \u3000 b\u00a0\u00a0c", "a b c"},
		{"ends", "  \u3000 name \u00a0 ", "name"},
		{"spaces only", " \u3000\u00a0 ", ""},
		{"nothing visible", "\u200b\u202e\x00", ""},
		{"empty", "", ""},
		{"long Latin", strings.Repeat("x", 40), strings.Repeat("x", MaxNameRunes)},
		{"long CJK, cut by code point", long + "名名", long},
		{"long with four-byte runes", strings.Repeat("😀", MaxNameRunes+3), strings.Repeat("😀", MaxNameRunes)},
		{"a space at the limit is dropped", strings.Repeat("x", MaxNameRunes-1) + " y", strings.Repeat("x", MaxNameRunes-1)},
		{"a space before the limit is kept", strings.Repeat("x", MaxNameRunes-2) + " y", strings.Repeat("x", MaxNameRunes-2) + " y"},
		{"not UTF-8", "a\xffb", "a�b"},
	}
	for _, c := range cases {
		got := NormalizeName(c.candidate)
		if got != c.want {
			t.Errorf("%s: NormalizeName(%q) = %q, want %q", c.name, c.candidate, got, c.want)
		}
		if n := utf8.RuneCountInString(got); n > MaxNameRunes || len(got) > 4*MaxNameRunes {
			t.Errorf("%s: %d code points, %d bytes", c.name, n, len(got))
		}
	}
}
