package main

import (
	"os"
	"strconv"
)

type LineEditor struct {
	hist    *PagedHistory
	cmdTrie *Trie
	lastLen int
}

func (ed *LineEditor) redraw(prompt string, buf []byte, cursor int) {
	out := "\r" + prompt + string(buf)
	if len(buf) < ed.lastLen {
		for i := 0; i < ed.lastLen-len(buf); i++ {
			out += " "
		}
	}
	ed.lastLen = len(buf)
	out += "\r" + prompt + string(buf[:cursor])
	os.Stdout.WriteString(out)
}

func (ed *LineEditor) handleTab(prompt string, buf []byte, cursor int) ([]byte, int) {
	// Extract the word immediately before the cursor
	wordStart := cursor
	for wordStart > 0 && buf[wordStart-1] != ' ' && buf[wordStart-1] != '\t' {
		wordStart--
	}
	prefix := string(buf[wordStart:cursor])

	// Check if this is the first token on the line (ignoring leading spaces)
	isFirstWord := true
	for i := 0; i < wordStart; i++ {
		if buf[i] != ' ' && buf[i] != '\t' {
			isFirstWord = false
			break
		}
	}

	var matches []Match
	if isFirstWord && !hasSlash(prefix) {
		matches = ed.cmdTrie.SearchPrefix(prefix)
	} else {
		matches = completePath(prefix)
	}

	if len(matches) == 0 {
		return buf, cursor
	}

	if len(matches) == 1 {
		completion := matches[0].Name
		if matches[0].IsDir {
			completion += "/"
		} else {
			completion += " "
		}
		tail := append([]byte(nil), buf[cursor:]...)
		buf = append(buf[:wordStart], []byte(completion)...)
		cursor = len(buf)
		buf = append(buf, tail...)
		ed.redraw(prompt, buf, cursor)
		return buf, cursor
	}

	// Multiple matches: extend to longest common prefix if longer than typed prefix
	lcp := longestCommonPrefix(matches)
	if len(lcp) > len(prefix) {
		tail := append([]byte(nil), buf[cursor:]...)
		buf = append(buf[:wordStart], []byte(lcp)...)
		cursor = len(buf)
		buf = append(buf, tail...)
		ed.redraw(prompt, buf, cursor)
		return buf, cursor
	}

	// Otherwise print candidate list below and redraw prompt
	os.Stdout.WriteString("\n")
	for i, m := range matches {
		if i > 0 {
			os.Stdout.WriteString("  ")
		}
		os.Stdout.WriteString(m.Name)
		if m.IsDir {
			os.Stdout.WriteString("/")
		}
	}
	os.Stdout.WriteString("\n")
	ed.lastLen = 0
	ed.redraw(prompt, buf, cursor)
	return buf, cursor
}

func (ed *LineEditor) ReadLine(prompt string) (string, bool) {
	setRawMode(true)
	defer setRawMode(false)

	ed.lastLen = 0
	ed.hist.cursor = len(ed.hist.entries)
	os.Stdout.WriteString(prompt)

	var buf []byte
	cursor := 0
	savedDraft := ""
	atDraft := true

	var b [1]byte
	for {
		n, err := os.Stdin.Read(b[:])
		if err != nil || n == 0 {
			return "", false
		}

		ch := b[0]
		switch ch {
		case '\n', '\r':
			os.Stdout.WriteString("\n")
			line := string(buf)
			ed.hist.Append(line)
			return line, true

		case '\t':
			buf, cursor = ed.handleTab(prompt, buf, cursor)

		case 0x03: // Ctrl+C
			os.Stdout.WriteString("^C\n")
			return "", true

		case 0x04: // Ctrl+D on empty line
			if len(buf) == 0 {
				os.Stdout.WriteString("\n")
				return "", false
			}

		case '\b', 0x7f: // Backspace
			if cursor > 0 {
				buf = append(buf[:cursor-1], buf[cursor:]...)
				cursor--
				ed.redraw(prompt, buf, cursor)
			}

		case 0x1b: // Escape sequence
			var seq [2]byte
			if n1, _ := os.Stdin.Read(seq[:1]); n1 == 0 || seq[0] != '[' {
				continue
			}
			if n2, _ := os.Stdin.Read(seq[1:2]); n2 == 0 {
				continue
			}
			switch seq[1] {
			case 'A': // Up arrow
				if atDraft {
					savedDraft = string(buf)
				}
				if cmd, ok := ed.hist.Prev(); ok {
					atDraft = false
					buf = []byte(cmd)
					cursor = len(buf)
					ed.redraw(prompt, buf, cursor)
				}
			case 'B': // Down arrow
				if cmd, ok, restoredDraft := ed.hist.Next(); ok {
					if restoredDraft {
						atDraft = true
						buf = []byte(savedDraft)
					} else {
						buf = []byte(cmd)
					}
					cursor = len(buf)
					ed.redraw(prompt, buf, cursor)
				}
			case 'C': // Right arrow
				if cursor < len(buf) {
					cursor++
					ed.redraw(prompt, buf, cursor)
				}
			case 'D': // Left arrow
				if cursor > 0 {
					cursor--
					ed.redraw(prompt, buf, cursor)
				}
			case 'H': // Home
				cursor = 0
				ed.redraw(prompt, buf, cursor)
			case 'F': // End
				cursor = len(buf)
				ed.redraw(prompt, buf, cursor)
			case '3': // Delete (\x1b[3~)
				var tilde [1]byte
				os.Stdin.Read(tilde[:])
				if tilde[0] == '~' && cursor < len(buf) {
					buf = append(buf[:cursor], buf[cursor+1:]...)
					ed.redraw(prompt, buf, cursor)
				}
			}

		default:
			if ch >= 32 && ch <= 126 {
				buf = append(buf, 0)
				copy(buf[cursor+1:], buf[cursor:])
				buf[cursor] = ch
				cursor++
				ed.redraw(prompt, buf, cursor)
			}
		}
	}
}

func splitFields(s string) []string {
	var args []string
	i := 0
	for i < len(s) {
		for i < len(s) && (s[i] == ' ' || s[i] == '\t') {
			i++
		}
		if i >= len(s) {
			break
		}
		start := i
		for i < len(s) && s[i] != ' ' && s[i] != '\t' {
			i++
		}
		args = append(args, s[start:i])
	}
	return args
}

func hasSlash(s string) bool {
	for i := 0; i < len(s); i++ {
		if s[i] == '/' {
			return true
		}
	}
	return false
}

func main() {
	ed := &LineEditor{
		hist:    newPagedHistory(),
		cmdTrie: buildCommandTrie(),
	}

	for {
		cwd, err := os.Getwd()
		if err != nil || cwd == "" {
			cwd = "/"
		}

		line, ok := ed.ReadLine("dragoon:" + cwd + "> ")
		if !ok {
			break
		}

		args := splitFields(line)
		if len(args) == 0 {
			continue
		}

		switch args[0] {
		case "exit":
			os.Exit(0)

		case "rehash":
			ed.cmdTrie = buildCommandTrie()

		case "pwd":
			os.Stdout.WriteString(cwd + "\n")

		case "cd":
			target := "/"
			if len(args) > 1 {
				target = args[1]
			}
			if err := os.Chdir(target); err != nil {
				os.Stderr.WriteString("cd: " + target + ": no such directory\n")
			}

		case "history":
			for i, cmd := range ed.hist.entries {
				os.Stdout.WriteString("  " + strconv.Itoa(i+1) + "  " + cmd + "\n")
			}

		case "help":
			os.Stdout.WriteString("Builtins: cd [dir], pwd, history, rehash, run <path> [args...], exit\n")
			os.Stdout.WriteString("Keys:     Tab (complete), Up/Down (paged history), Left/Right/Home/End\n")

		case "run":
			if len(args) < 2 {
				os.Stderr.WriteString("usage: run <path> [args...]\n")
				continue
			}
			status, spawned := spawnAndWait(args[1], args[1:])
			if !spawned {
				os.Stderr.WriteString("run: failed to spawn " + args[1] + "\n")
			} else if status != 0 {
				os.Stderr.WriteString("exited with status " + strconv.Itoa(status) + "\n")
			}

		default:
			binPath := args[0]
			if !hasSlash(binPath) {
				binPath = "/bin/" + binPath + ".bin"
			}
			status, spawned := spawnAndWait(binPath, args)
			if !spawned {
				os.Stderr.WriteString("unknown command: " + args[0] + "\n")
			} else if status != 0 {
				os.Stderr.WriteString("exited with status " + strconv.Itoa(status) + "\n")
			}
		}
	}
}
