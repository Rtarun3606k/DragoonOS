package main

import "os"

func removeAll(path string) error {

	info, err := os.Stat(path)

	if err != nil {
		return nil
	}

	if info.IsDir() {
		entries, err := os.ReadDir(path)

		if err != nil {
			return nil
		}
		for _, entry := range entries {
			child := path
			if child != "/" {
				child += "/"
			}
			child += entry.Name()
			if err := removeAll(child); err != nil {
				return err
			}
		}
	}

	return os.Remove(path)
}

func main() {

	if len(os.Args) < 2 {
		os.Stderr.WriteString("usage: rm [-r] <path>...\n")
		os.Exit(1)
	}

	recursive := false
	force := false
	start := 1

	for start < len(os.Args) && len(os.Args[start]) > 1 && os.Args[start][0] == '-' {
		switch os.Args[start] {
		case "-r":
			recursive = true
		case "-f":
			force = true
		case "-rf", "-fr":
			recursive = true
			force = true
		default:
			os.Stderr.WriteString("rm: unknown option " + os.Args[start] + "\n")
			os.Exit(1)
		}
		start++
	}

	if start >= len(os.Args) {
		os.Stderr.WriteString("usage: rm [-r] <path>...\n")
		os.Exit(1)
	}

	exitCode := 0

	for _, target := range os.Args[start:] {
		if target == "/" || target == "." || target == ".." {
			os.Stderr.WriteString("rm: refusing to remove '" + target + "'\n")
			exitCode = 1
			continue
		}

		var err error
		if recursive {
			err = removeAll(target)
		} else {
			err = os.Remove(target)
		}

		if err != nil {
			if force && os.IsNotExist(err) {
				continue
			}
			os.Stderr.WriteString("rm: cannot remove '" + target + "': " + err.Error() + "\n")
			exitCode = 1
		}
	}

	os.Exit(exitCode)

}
