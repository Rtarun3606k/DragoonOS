package main

import (
	"io"
	"os"
)

func baseName(path string) string {
	end := len(path)
	for end > 1 && path[end-1] == '/' {
		end--
	}
	for i := end - 1; i >= 0; i-- {
		if path[i] == '/' {
			return path[i+1 : end]
		}
	}
	return path[:end]
}

func joinPath(dir, name string) string {
	if len(dir) == 0 {
		return name
	}
	if dir[len(dir)-1] == '/' {
		return dir + name
	}
	return dir + "/" + name
}

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, in)
	return err
}

func copyRecursive(src, dst string) error {
	info, err := os.Stat(src)
	if err != nil {
		return err
	}

	if !info.IsDir() {
		return copyFile(src, dst)
	}

	if err := os.Mkdir(dst, 0755); err != nil {
		if dstInfo, statErr := os.Stat(dst); statErr != nil || !dstInfo.IsDir() {
			return err
		}
	}

	entries, err := os.ReadDir(src)
	if err != nil {
		return err
	}

	for _, e := range entries {
		if err := copyRecursive(joinPath(src, e.Name()), joinPath(dst, e.Name())); err != nil {
			return err
		}
	}
	return nil
}

func main() {
	if len(os.Args) < 3 {
		os.Stderr.WriteString("usage: cp [-r] <src> <dst>\n")
		os.Exit(1)
	}

	recursive := false
	argIdx := 1
	if os.Args[1] == "-r" || os.Args[1] == "-R" {
		recursive = true
		argIdx = 2
	}

	if len(os.Args)-argIdx != 2 {
		os.Stderr.WriteString("usage: cp [-r] <src> <dst>\n")
		os.Exit(1)
	}

	src := os.Args[argIdx]
	dst := os.Args[argIdx+1]

	srcInfo, err := os.Stat(src)
	if err != nil {
		os.Stderr.WriteString("cp: " + src + ": " + err.Error() + "\n")
		os.Exit(1)
	}

	if srcInfo.IsDir() && !recursive {
		os.Stderr.WriteString("cp: -r not specified; omitting directory '" + src + "'\n")
		os.Exit(1)
	}

	// If destination is an existing directory, copy into dst/<basename(src)>
	if dstInfo, err := os.Stat(dst); err == nil && dstInfo.IsDir() {
		dst = joinPath(dst, baseName(src))
	}

	if recursive {
		err = copyRecursive(src, dst)
	} else {
		err = copyFile(src, dst)
	}

	if err != nil {
		os.Stderr.WriteString("cp: " + dst + ": " + err.Error() + "\n")
		os.Exit(1)
	}
}
