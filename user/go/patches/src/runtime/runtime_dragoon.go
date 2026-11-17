//go:build dragoon

package runtime

import "unsafe"

//go:linkname cPutchar putchar
func cPutchar(c int) int

//go:linkname cExit _exit
func cExit(status int)

//go:linkname cAbort abort
func cAbort()

// The C _start stub records argc/argv in opaque C globals so the whole-program
// optimiser cannot fold them away before the runtime queries them.
//go:linkname cSetArgs dragoon_set_args
func cSetArgs(argc int32, argv **byte)

//go:linkname cGetArgc dragoon_get_argc
func cGetArgc() int32

//go:linkname cGetArgv dragoon_get_argv
func cGetArgv() **byte

var dragoonArgs []string

// os_runtime_args is called lazily from os.init(), after initHeap(), so it is
// safe to allocate here. Building the slice on demand avoids any dependency on
// the order in which package initialisers run.
//
//go:linkname os_runtime_args os.runtime_args
func os_runtime_args() []string {
	if dragoonArgs != nil {
		return dragoonArgs
	}

	argc := cGetArgc()
	argv := cGetArgv()
	list := make([]string, 0, int(argc))

	for i := int32(0); i < argc; i++ {
		if argv == nil {
			break
		}

		slot := unsafe.Add(unsafe.Pointer(argv), uintptr(i)*unsafe.Sizeof(uintptr(0)))
		arg := *(**byte)(slot)

		if arg == nil {
			break
		}

		length := 0
		for *(*byte)(unsafe.Add(unsafe.Pointer(arg), uintptr(length))) != 0 {
			length++
		}

		list = append(list, string(unsafe.Slice(arg, length)))
	}

	dragoonArgs = list
	return dragoonArgs
}

//go:linkname syscall_runtime_envs syscall.runtime_envs
func syscall_runtime_envs() []string {
	return nil
}

//export main
func main(argc int32, argv **byte) int32 {
	cSetArgs(argc, argv)
	run()
	return 0
}

func putchar(c byte) {
	cPutchar(int(c))
}

func exit(code int) {
	cExit(code)
}

func abort() {
	cAbort()
}

func ticks() timeUnit {
	return 0
}

func ticksToNanoseconds(t timeUnit) int64 {
	return int64(t)
}

func nanosecondsToTicks(ns int64) timeUnit {
	return timeUnit(ns)
}

func sleepTicks(d timeUnit) {
}
