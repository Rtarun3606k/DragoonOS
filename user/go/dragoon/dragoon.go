package dragoon

import "unsafe"

//go:linkname rawWrite write
func rawWrite(fd int32, buf unsafe.Pointer, count uint32) int32

func Print(s string) {
	if len(s) == 0 {
		return
	}
	rawWrite(1, unsafe.Pointer(unsafe.StringData(s)), uint32(len(s)))
}

func Println(s string) {
	Print(s)
	Print("\n")
}
