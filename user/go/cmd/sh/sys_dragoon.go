package main

//export dragoon_tty_raw
func cTtyRaw(enable int32) int32

//export dragoon_spawn
func cSpawn(path *byte, argv **byte) int32

//export dragoon_wait
func cWait(pid int32, status *int32) int32

func setRawMode(enabled bool) {
	if enabled {
		cTtyRaw(1)
	} else {
		cTtyRaw(0)
	}
}

func spawnAndWait(path string, args []string) (int, bool) {
	cPath := append([]byte(path), 0)
	ptrs := make([]*byte, len(args)+1)
	keepAlive := make([][]byte, len(args))

	for i, arg := range args {
		b := append([]byte(arg), 0)
		keepAlive[i] = b
		ptrs[i] = &b[0]
	}
	ptrs[len(args)] = nil

	pid := cSpawn(&cPath[0], &ptrs[0])
	if pid < 0 {
		return -1, false
	}

	var status int32
	cWait(pid, &status)
	return int(status), true
}
