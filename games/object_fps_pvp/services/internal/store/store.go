// Package store writes the services' files.
package store

import (
	"errors"
	"os"
	"path/filepath"
)

// WriteFileAtomic writes a temporary file next to path and renames it over
// path, so a reader sees the old file or the new one, never half of one.
func WriteFileAtomic(path string, body []byte) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	temporary, err := os.CreateTemp(filepath.Dir(path), filepath.Base(path)+".*")
	if err != nil {
		return err
	}
	name := temporary.Name()
	_, writeErr := temporary.Write(body)
	closeErr := temporary.Close()
	if writeErr != nil || closeErr != nil {
		_ = os.Remove(name)
		return errors.Join(writeErr, closeErr)
	}
	if err := os.Rename(name, path); err != nil {
		_ = os.Remove(name)
		return err
	}
	return nil
}
