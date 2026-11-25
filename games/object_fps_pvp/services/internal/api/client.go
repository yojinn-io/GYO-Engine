package api

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"time"
)

// Client calls another service over the internal protocol (declaration D4).
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

// Call sends in (nil: no body) as user with the idempotency key (empty: none)
// and decodes a 2xx answer into out (nil: ignored). A non-2xx answer is a
// *Refused; anything else (the service unreachable, a body that does not
// decode) is the transport's error.
func (c *Client) Call(ctx context.Context, method, path string, user uint64, key string, in, out any) error {
	var body io.Reader
	if in != nil {
		encoded, err := json.Marshal(in)
		if err != nil {
			return err
		}
		body = bytes.NewReader(encoded)
	}
	request, err := http.NewRequestWithContext(ctx, method, c.Base+path, body)
	if err != nil {
		return err
	}
	request.Header.Set(HeaderUser, strconv.FormatUint(user, 10))
	if key != "" {
		request.Header.Set(HeaderIdempotency, key)
	}
	if in != nil {
		request.Header.Set("Content-Type", "application/json")
	}
	response, err := c.HTTP.Do(request)
	if err != nil {
		return err
	}
	defer response.Body.Close()
	answer, err := io.ReadAll(io.LimitReader(response.Body, 1<<20))
	if err != nil {
		return err
	}
	if response.StatusCode/100 != 2 {
		var refusal Error
		_ = json.Unmarshal(answer, &refusal)
		return &Refused{Status: response.StatusCode, Code: refusal.Error}
	}
	if out == nil {
		return nil
	}
	return json.Unmarshal(answer, out)
}
