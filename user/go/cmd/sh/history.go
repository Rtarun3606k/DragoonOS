package main

import "os"

const (
	historyFile   = "/.shHistory.txt"
	historyWindow = 10
	readChunkSize = 1024
)

type PagedHistory struct {
	entries     []string // Active window of up to 10 entries in RAM
	cursor      int      // Current navigation index inside `entries` (len(entries) == new draft)
	windowStart int64    // File byte offset where entries[0] begins
	windowEnd   int64    // File byte offset right after entries[len-1]
	fileSize    int64    // Total bytes in /.shHistory.txt
}

func newPagedHistory() *PagedHistory {
	h := &PagedHistory{}
	h.loadTail()
	return h
}

func (h *PagedHistory) loadTail() {
	info, err := os.Stat(historyFile)
	if err != nil || info.Size() == 0 {
		h.entries = nil
		h.cursor = 0
		h.windowStart = 0
		h.windowEnd = 0
		h.fileSize = 0
		return
	}

	h.fileSize = info.Size()
	h.loadPageBefore(h.fileSize)
}

// loadPageBefore loads up to `historyWindow` lines ending at `endOffset`.
func (h *PagedHistory) loadPageBefore(endOffset int64) bool {
	if endOffset <= 0 {
		return false
	}

	f, err := os.Open(historyFile)
	if err != nil {
		return false
	}
	defer f.Close()

	start := endOffset - readChunkSize
	if start < 0 {
		start = 0
	}

	if _, err := f.Seek(start, 0); err != nil {
		return false
	}

	buf := make([]byte, endOffset-start)
	n, err := f.Read(buf)
	if err != nil || n <= 0 {
		return false
	}
	buf = buf[:n]

	// Parse lines backward from the end of the chunk
	var parsed []string
	var lineStarts []int64

	end := len(buf)
	if end > 0 && buf[end-1] == '\n' {
		end--
	}

	for end > 0 && len(parsed) < historyWindow {
		nl := -1
		for i := end - 1; i >= 0; i-- {
			if buf[i] == '\n' {
				nl = i
				break
			}
		}

		// If we hit the start of a partial chunk (start > 0) with no newline,
		// stop here so we don't load a truncated line.
		if nl == -1 && start > 0 {
			break
		}

		lineStart := nl + 1
		line := string(buf[lineStart:end])
		if len(line) > 0 {
			parsed = append(parsed, line)
			lineStarts = append(lineStarts, start+int64(lineStart))
		}
		end = nl
	}

	if len(parsed) == 0 {
		return false
	}

	// Reverse so oldest in the window is at index 0
	for i, j := 0, len(parsed)-1; i < j; i, j = i+1, j-1 {
		parsed[i], parsed[j] = parsed[j], parsed[i]
		lineStarts[i], lineStarts[j] = lineStarts[j], lineStarts[i]
	}

	h.entries = parsed
	h.windowStart = lineStarts[0]
	h.windowEnd = endOffset
	h.cursor = len(h.entries)
	return true
}

// loadPageAfter loads up to `historyWindow` lines starting at `startOffset`.
func (h *PagedHistory) loadPageAfter(startOffset int64) bool {
	if startOffset >= h.fileSize {
		return false
	}

	f, err := os.Open(historyFile)
	if err != nil {
		return false
	}
	defer f.Close()

	if _, err := f.Seek(startOffset, 0); err != nil {
		return false
	}

	buf := make([]byte, readChunkSize)
	n, err := f.Read(buf)
	if err != nil || n <= 0 {
		return false
	}
	buf = buf[:n]

	var parsed []string
	pos := 0
	for pos < len(buf) && len(parsed) < historyWindow {
		nl := -1
		for i := pos; i < len(buf); i++ {
			if buf[i] == '\n' {
				nl = i
				break
			}
		}
		if nl == -1 {
			break
		}
		line := string(buf[pos:nl])
		if len(line) > 0 {
			parsed = append(parsed, line)
		}
		pos = nl + 1
	}

	if len(parsed) == 0 {
		return false
	}

	h.entries = parsed
	h.windowStart = startOffset
	h.windowEnd = startOffset + int64(pos)
	h.cursor = 0
	return true
}

func (h *PagedHistory) Append(line string) {
	if len(line) == 0 {
		return
	}
	if len(h.entries) > 0 && h.entries[len(h.entries)-1] == line {
		h.loadTail()
		return
	}

	f, err := os.OpenFile(historyFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if err == nil {
		f.WriteString(line + "\n")
		f.Close()
	}

	// Reload tail window so RAM never exceeds `historyWindow` items
	h.loadTail()
}

func (h *PagedHistory) Prev() (string, bool) {
	if h.cursor > 0 {
		h.cursor--
		return h.entries[h.cursor], true
	}
	// Hit top of current 10-item window: page backward from disk
	if h.windowStart > 0 && h.loadPageBefore(h.windowStart) {
		if h.cursor > 0 {
			h.cursor--
			return h.entries[h.cursor], true
		}
	}
	return "", false
}

func (h *PagedHistory) Next() (string, bool, bool) {
	if h.cursor+1 < len(h.entries) {
		h.cursor++
		return h.entries[h.cursor], true, false
	}
	// Hit bottom of current 10-item window: page forward toward tail
	if h.windowEnd < h.fileSize && h.loadPageAfter(h.windowEnd) {
		return h.entries[0], true, false
	}
	if h.cursor < len(h.entries) {
		h.cursor = len(h.entries)
		return "", true, true // Restored to blank/draft prompt
	}
	return "", false, false
}
