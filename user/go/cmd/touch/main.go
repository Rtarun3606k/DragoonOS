package main

import "os"

func main() {
	if len(os.Args) < 2 {
		os.Stderr.WriteString("usage: touch <file>...\n")
		os.Exit(1)
	}

	exitCode := 0
	for _, path := range os.Args[1:] {
		f, err := os.OpenFile(path, os.O_CREATE|os.O_RDWR, 0644)
		if err != nil {
			os.Stderr.WriteString("touch: " + path + ": " + err.Error() + "\n")
			exitCode = 1
			continue
		}
		f.Close()
	}
	os.Exit(exitCode)
}

