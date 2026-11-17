package main

import "os"

type trieNode struct {
	children map[byte]*trieNode
	isEnd    bool
	isDir    bool
}

type Trie struct {
	root *trieNode
}

func newTrie() *Trie {
	return &Trie{root: &trieNode{children: make(map[byte]*trieNode)}}
}

func (t *Trie) Insert(word string, isDir bool) {
	node := t.root
	for i := 0; i < len(word); i++ {
		ch := word[i]
		next, ok := node.children[ch]
		if !ok {
			next = &trieNode{children: make(map[byte]*trieNode)}
			node.children[ch] = next
		}
		node = next
	}
	node.isEnd = true
	node.isDir = isDir
}

type Match struct {
	Name  string
	IsDir bool
}

func (t *Trie) SearchPrefix(prefix string) []Match {
	node := t.root
	for i := 0; i < len(prefix); i++ {
		next, ok := node.children[prefix[i]]
		if !ok {
			return nil
		}
		node = next
	}

	var results []Match
	var dfs func(curr *trieNode, path []byte)
	dfs = func(curr *trieNode, path []byte) {
		if curr.isEnd {
			results = append(results, Match{
				Name:  string(path),
				IsDir: curr.isDir,
			})
		}
		for ch, child := range curr.children {
			dfs(child, append(path, ch))
		}
	}

	dfs(node, []byte(prefix))
	return results
}

func longestCommonPrefix(matches []Match) string {
	if len(matches) == 0 {
		return ""
	}
	lcp := matches[0].Name
	for i := 1; i < len(matches); i++ {
		other := matches[i].Name
		j := 0
		for j < len(lcp) && j < len(other) && lcp[j] == other[j] {
			j++
		}
		lcp = lcp[:j]
	}
	return lcp
}

func buildCommandTrie() *Trie {
	t := newTrie()

	builtins := []string{"cd", "pwd", "history", "help", "run", "exit", "rehash"}
	for _, b := range builtins {
		t.Insert(b, false)
	}

	entries, err := os.ReadDir("/bin")
	if err != nil {
		return t
	}

	for _, e := range entries {
		if e.IsDir() {
			continue
		}
		name := e.Name()
		if len(name) > 4 && name[len(name)-4:] == ".bin" {
			name = name[:len(name)-4]
		}
		t.Insert(name, false)
	}
	return t
}

func completePath(token string) []Match {
	dir := "."
	prefix := token
	slashIdx := -1

	for i := len(token) - 1; i >= 0; i-- {
		if token[i] == '/' {
			slashIdx = i
			break
		}
	}

	basePrefix := ""
	if slashIdx != -1 {
		if slashIdx == 0 {
			dir = "/"
			basePrefix = "/"
		} else {
			dir = token[:slashIdx]
			basePrefix = dir + "/"
		}
		prefix = token[slashIdx+1:]
	}

	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil
	}

	t := newTrie()
	for _, e := range entries {
		t.Insert(e.Name(), e.IsDir())
	}

	raw := t.SearchPrefix(prefix)
	for i := range raw {
		raw[i].Name = basePrefix + raw[i].Name
	}
	return raw
}
