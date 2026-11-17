package main

import (
	"fmt"
	"os"
)

func list(path string) bool {
	entries, err := os.ReadDir(path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "ls: %s: %v\n", path, err)
		return false
	}

	for _, entry := range entries {
		if entry.IsDir() {
			fmt.Printf("[DIR]  %s\n", entry.Name())
			continue
		}

		info, err := entry.Info()
		if err != nil {
			fmt.Printf("[FILE] %s\n", entry.Name())
			continue
		}

		fmt.Printf("[FILE] %s (%d B)\n", entry.Name(), info.Size())
	}

	return true
}

func main() {
	path := "."
	if len(os.Args) > 1 {
		path = os.Args[1]
	}

	if !list(path) {
		os.Exit(1)
	}
}
