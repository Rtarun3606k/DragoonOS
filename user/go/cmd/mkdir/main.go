package main

import (
	"os"
)

func main() {

	if len(os.Args) < 2 {
		os.Stderr.WriteString("usage: mkdir [-p] <dir>...\n")
		os.Exit(1)
	}

	parents := false
	start := 1

	if os.Args[1] == "-p" {
		parents = true
		start = 2
	}

	if len(os.Args) <= start {
		os.Stderr.WriteString("usage: mkdir [-p] <dir>...\n")
		os.Exit(1)
	}

	exitCode := 0

	for _, dir := range os.Args[start:] {

		var err error

		if parents {
			err = os.MkdirAll(dir, 0755)
		} else {
			err = os.Mkdir(dir, 0755)
		}
		if err != nil {
			os.Stderr.WriteString("mkdir: cannot create '" + dir + "': " + err.Error() + "\n")
			exitCode = 1
		}
	}

	os.Exit(exitCode)
}
