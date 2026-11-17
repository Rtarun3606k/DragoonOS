package main

import (
	"fmt"
	"io"
	"os"
)

func copyFile(name string) bool {
	f, err := os.Open(name)
	if err != nil {
		fmt.Fprintf(os.Stderr, "cat: %s: %v\n", name, err)
		return false
	}

	_, err = io.Copy(os.Stdout, f)
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "cat: %s: %v\n", name, err)
		return false
	}

	return true
}

func main() {
	ok := true

	if len(os.Args) < 2 {
		// No file operands: read standard input until EOF (Ctrl+D).
		if _, err := io.Copy(os.Stdout, os.Stdin); err != nil {
			fmt.Fprintf(os.Stderr, "cat: stdin: %v\n", err)
			ok = false
		}
	} else {
		for _, name := range os.Args[1:] {
			if !copyFile(name) {
				ok = false
			}
		}
	}

	if !ok {
		os.Exit(1)
	}
}
