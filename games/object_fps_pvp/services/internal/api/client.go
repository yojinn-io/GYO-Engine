package api

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"time"
)

// System is the internal protocol's user for a request no player made (a
// service, a Match).
const System uint64 = 0

// Caller is who an internal request is for: a user (System included) or
// nobody (a request before sign-in, which carries no user header). Name is
// the name the user signed in with (empty: none, no name header).
type Caller struct {
	User      uint64
	Name      string
	Anonymous bool
}

// As is a request for user.
func As(user uint64) Caller { return Caller{User: user} }

// Nobody is a request before sign-in.
var Nobody = Caller{Anonymous: true}

// MaxAnswer is the largest answer a caller reads.
const MaxAnswer = 1 << 20

// Send is the one way an internal request is made (declaration D4): the
// caller's user header and idempotency key, the JSON body (nil: none). It
// returns the answer's status and body.
func Send(ctx context.Context, client *http.Client, method, url string, caller Caller, key string, body []byte) (int, []byte, error) {
	var reader io.Reader
	if body != nil {
		reader = bytes.NewReader(body)
	}
	request, err := http.NewRequestWithContext(ctx, method, url, reader)
	if err != nil {
		return 0, nil, err
	}
	if !caller.Anonymous {
		request.Header.Set(HeaderUser, strconv.FormatUint(caller.User, 10))
		if caller.Name != "" {
			request.Header.Set(HeaderName, base64.RawURLEncoding.EncodeToString([]byte(caller.Name)))
		}
	}
	if key != "" {
		request.Header.Set(HeaderIdempotency, key)
	}
	if body != nil {
		request.Header.Set("Content-Type", "application/json")
	}
	response, err := client.Do(request)
	if err != nil {
		return 0, nil, err
	}
	defer response.Body.Close()
	answer, err := io.ReadAll(io.LimitReader(response.Body, MaxAnswer))
	if err != nil {
		return 0, nil, err
	}
	return response.StatusCode, answer, nil
}

// Client calls one service with JSON values.
type Client struct {
	Base string
	HTTP *http.Client
}

func NewClient(base string) *Client {
	return &Client{Base: base, HTTP: &http.Client{Timeout: 10 * time.Second}}
}

// Refused is a service's answer that is not 2xx: its status and error code.
type Refused struct {
	Status int
	Code   string
}

func (e *Refused) Error() string { return fmt.Sprintf("%d %s", e.Status, e.Code) }

// Call sends in (nil: no body) and decodes a 2xx answer into out (nil:
// ignored). A non-2xx answer is a *Refused; anything else (the service
// unreachable, a body that does not decode) is the transport's error.
func (c *Client) Call(ctx context.Context, method, path string, caller Caller, key string, in, out any) error {
	var body []byte
	if in != nil {
		encoded, err := json.Marshal(in)
		if err != nil {
			return err
		}
		body = encoded
	}
	status, answer, err := Send(ctx, c.HTTP, method, c.Base+path, caller, key, body)
	if err != nil {
		return err
	}
	if status/100 != 2 {
		var refusal Error
		_ = json.Unmarshal(answer, &refusal)
		return &Refused{Status: status, Code: refusal.Error}
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(answer, out)
}
