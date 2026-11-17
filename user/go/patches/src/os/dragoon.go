//go:build dragoon

// DragoonOS filesystem and stdio implementation for TinyGo's os package.
//
// TinyGo's baremetal os package has no filesystem at all.  This file mounts a
// FAT32-backed Filesystem at "/" and supplies a FileHandle that maps the
// standard os.File operations onto the DragoonOS syscall ABI (see
// include/syscall.h and user/go/runtime_dragoon.c).

package os

import (
	"errors"
	"io"
	"io/fs"
	"time"
	"unsafe"
)

/* ---- C syscall wrappers (runtime_dragoon.c) ------------------------------ */

//go:linkname dragoonOpen open
func dragoonOpen(path *byte, flags int32) int32

//go:linkname dragoonRead read
func dragoonRead(fd int32, buf unsafe.Pointer, count uint32) int32

//go:linkname dragoonWrite write
func dragoonWrite(fd int32, buf unsafe.Pointer, count uint32) int32

//go:linkname dragoonClose close
func dragoonClose(fd int32) int32

//go:linkname dragoonLseek lseek
func dragoonLseek(fd int32, offset int32, whence int32) int32

//go:linkname dragoonStatCall stat
func dragoonStatCall(path *byte, st *dragoonStat) int32

//go:linkname dragoonFstatCall fstat
func dragoonFstatCall(fd int32, st *dragoonStat) int32

//go:linkname dragoonMkdir mkdir
func dragoonMkdir(path *byte) int32

//go:linkname dragoonUnlink unlink
func dragoonUnlink(path *byte) int32

//go:linkname dragoonReaddir readdir
func dragoonReaddir(path *byte, index int32, out *dragoonDirent) int32

//go:linkname dragoonChdir chdir
func dragoonChdir(path *byte) int32

//go:linkname dragoonGetcwd getcwd
func dragoonGetcwd(buf *byte, size uint32) *byte

/* ---- ABI-compatible records --------------------------------------------- */

type dragoonStat struct {
	Size       uint32
	Date       uint16
	Time       uint16
	IsDir      uint8
	IsReadonly uint8
	Reserved   uint16
}

type dragoonDirent struct {
	Name  [64]byte
	Size  uint32
	IsDir uint8
	_     [3]byte
}

/* ---- Kernel flag encoding (include/syscall.h) --------------------------- */

const (
	kO_RDONLY = 0x01
	kO_WRONLY = 0x02
	kO_RDWR   = 0x03
	kO_CREAT  = 0x04
	kO_TRUNC  = 0x08
	kO_APPEND = 0x10
)

func dragoonKernelFlags(flag int) int32 {
	k := int32(kO_RDONLY)

	switch flag & 3 {
	case O_WRONLY:
		k = kO_WRONLY
	case O_RDWR:
		k = kO_RDWR
	}

	if flag&O_CREATE != 0 {
		k |= kO_CREAT
	}
	if flag&O_TRUNC != 0 {
		k |= kO_TRUNC
	}
	if flag&O_APPEND != 0 {
		k |= kO_APPEND
	}

	return k
}

func dragoonCString(s string) []byte {
	b := make([]byte, len(s)+1)
	copy(b, s)
	return b
}

/* ---- Error values ------------------------------------------------------- */

var errDragoonIO = errors.New("dragoon: I/O error")

/* ---- Filesystem --------------------------------------------------------- */

// dragoonCwdMax mirrors TASK_CWD_MAX in include/task.h.
const dragoonCwdMax = 256

// dirFileHandle is a pseudo-handle for directory streams.  Directory contents
// are read by path (SYS_READDIR), so no kernel fd is needed.
const dirFileHandle = ^uintptr(0)

type dragoonFilesystem struct{}

func init() {
	Mount("/", dragoonFilesystem{})
	getwdHook = dragoonGetwd
}

func (dragoonFilesystem) OpenFile(path string, flag int, perm FileMode) (uintptr, error) {
	st, ok := dragoonStatPath(path)

	if !ok {
		/*
		 * FatFs' f_stat sometimes rejects the root directory ("/") even
		 * though it can be opened for enumeration.  Probe with readdir index 0
		 * (>= 0 means it is an enumerable directory).
		 */
		cb := dragoonCString(path)
		var probe dragoonDirent
		if dragoonReaddir(&cb[0], 0, &probe) >= 0 {
			if flag&(O_WRONLY|O_RDWR|O_CREATE|O_TRUNC) != 0 {
				return 0, ErrUnsupported
			}
			return dirFileHandle, nil
		}
	}

	if ok && st.IsDir != 0 {
		if flag&(O_WRONLY|O_RDWR|O_CREATE|O_TRUNC) != 0 {
			return 0, ErrUnsupported
		}
		return dirFileHandle, nil
	}

	cb := dragoonCString(path)
	fd := dragoonOpen(&cb[0], dragoonKernelFlags(flag))
	if fd < 0 {
		return 0, ErrNotExist
	}

	return uintptr(uint32(fd)), nil
}

func (dragoonFilesystem) Mkdir(path string, perm FileMode) error {
	cb := dragoonCString(path)

	if dragoonMkdir(&cb[0]) < 0 {
		if _, ok := dragoonStatPath(path); ok {
			return ErrExist
		}
		return ErrNotExist
	}

	return nil
}

func (dragoonFilesystem) Remove(path string) error {
	cb := dragoonCString(path)

	if dragoonUnlink(&cb[0]) < 0 {
		if _, ok := dragoonStatPath(path); !ok {
			return ErrNotExist
		}
		return ErrUnsupported
	}

	return nil
}

/* ---- Stdio and File plumbing ------------------------------------------- */

const DevNull = "/dev/null"

var (
	Stdin  = NewFile(0, "/dev/stdin")
	Stdout = NewFile(1, "/dev/stdout")
	Stderr = NewFile(2, "/dev/stderr")
)

// isOS is true so findMount falls back to the root mount for relative paths.
const isOS = true

type file struct {
	handle     FileHandle
	name       string
	appendMode bool
	dirIndex   int
}

func (f *file) close() error {
	return f.handle.Close()
}

type dragoonFileHandle uintptr

func NewFile(fd uintptr, name string) *File {
	return &File{&file{handle: dragoonFileHandle(fd), name: name}}
}

func Chdir(dir string) error {
	cb := dragoonCString(dir)

	if dragoonChdir(&cb[0]) < 0 {
		return &PathError{Op: "chdir", Path: dir, Err: ErrNotExist}
	}

	return nil
}

// dragoonGetwd implements Getwd for DragoonOS (registered through getwdHook
// in file.go).
func dragoonGetwd() (string, error) {
	buf := make([]byte, dragoonCwdMax)

	if dragoonGetcwd(&buf[0], uint32(len(buf))) == nil {
		return "", &PathError{Op: "getwd", Err: errDragoonIO}
	}

	n := 0
	for n < len(buf) && buf[n] != 0 {
		n++
	}

	return string(buf[:n]), nil
}

func Rename(oldpath, newpath string) error {
	return ErrNotImplemented
}

func Pipe() (r *File, w *File, err error) {
	return nil, nil, ErrNotImplemented
}

func Symlink(oldname, newname string) error {
	return ErrNotImplemented
}

func Readlink(name string) (string, error) {
	return "", ErrNotImplemented
}

func tempDir() string {
	return "/tmp"
}

func Truncate(filename string, size int64) error {
	return ErrUnsupported
}

func (f *File) Truncate(size int64) error {
	if f.handle == nil {
		return ErrClosed
	}
	return ErrUnsupported
}

func (f *File) chmod(mode FileMode) error {
	return ErrUnsupported
}

func (f *File) chdir() error {
	return ErrNotImplemented
}

/* ---- FileHandle --------------------------------------------------------- */

func (h dragoonFileHandle) fd() int32 {
	return int32(uint32(uintptr(h)))
}

func (h dragoonFileHandle) Read(b []byte) (int, error) {
	if uintptr(h) == dirFileHandle {
		return 0, ErrUnsupported
	}
	if len(b) == 0 {
		return 0, nil
	}

	n := dragoonRead(h.fd(), unsafe.Pointer(&b[0]), uint32(len(b)))
	if n < 0 {
		return 0, errDragoonIO
	}
	if n == 0 {
		return 0, io.EOF
	}

	return int(n), nil
}

func (h dragoonFileHandle) Write(b []byte) (int, error) {
	fd := uintptr(h)

	if fd == dirFileHandle || fd == 0 {
		return 0, ErrUnsupported
	}
	if len(b) == 0 {
		return 0, nil
	}

	n := dragoonWrite(h.fd(), unsafe.Pointer(&b[0]), uint32(len(b)))
	if n < 0 {
		return 0, errDragoonIO
	}

	return int(n), nil
}

func (h dragoonFileHandle) Close() error {
	fd := uintptr(h)

	if fd == dirFileHandle || fd < 3 {
		return nil
	}
	if dragoonClose(h.fd()) < 0 {
		return ErrClosed
	}

	return nil
}

func (h dragoonFileHandle) Fd() uintptr {
	return uintptr(h)
}

func (h dragoonFileHandle) Seek(offset int64, whence int) (int64, error) {
	res := dragoonLseek(h.fd(), int32(offset), int32(whence))
	if res < 0 {
		return 0, ErrInvalid
	}
	return int64(res), nil
}

func (h dragoonFileHandle) ReadAt(b []byte, off int64) (int, error) {
	if len(b) == 0 {
		return 0, nil
	}

	cur, err := h.Seek(0, io.SeekCurrent)
	if err != nil {
		return 0, err
	}
	if _, err = h.Seek(off, io.SeekStart); err != nil {
		return 0, err
	}

	n, rerr := h.Read(b)

	if _, serr := h.Seek(cur, io.SeekStart); serr != nil && rerr == nil {
		rerr = serr
	}

	return n, rerr
}

func (h dragoonFileHandle) WriteAt(b []byte, off int64) (int, error) {
	if len(b) == 0 {
		return 0, nil
	}

	cur, err := h.Seek(0, io.SeekCurrent)
	if err != nil {
		return 0, err
	}
	if _, err = h.Seek(off, io.SeekStart); err != nil {
		return 0, err
	}

	n, werr := h.Write(b)

	if _, serr := h.Seek(cur, io.SeekStart); serr != nil && werr == nil {
		werr = serr
	}

	return n, werr
}

func (h dragoonFileHandle) Sync() error {
	return nil
}

/* ---- Stat --------------------------------------------------------------- */

type fileStat struct {
	name    string
	size    int64
	mode    FileMode
	modTime time.Time
}

func (s *fileStat) Name() string       { return s.name }
func (s *fileStat) Size() int64        { return s.size }
func (s *fileStat) Mode() FileMode     { return s.mode }
func (s *fileStat) ModTime() time.Time { return s.modTime }
func (s *fileStat) Sys() interface{}   { return nil }
func (s *fileStat) IsDir() bool        { return s.mode.IsDir() }

func dragoonStatPath(name string) (dragoonStat, bool) {
	var st dragoonStat
	cb := dragoonCString(name)

	if dragoonStatCall(&cb[0], &st) < 0 {
		return st, false
	}

	/*
	 * FatFs returns success for the origin directory ("/") without filling in
	 * a directory entry, so its attribute byte is not meaningful there.
	 */
	if name == "/" || name == "" {
		st.IsDir = 1
	}

	return st, true
}

func statFromRecord(st *dragoonStat, name string) FileInfo {
	mode := FileMode(0644)
	if st.IsDir != 0 {
		mode = FileMode(0755) | ModeDir
	}

	return &fileStat{name: name, size: int64(st.Size), mode: mode}
}

func (f *File) Stat() (FileInfo, error) {
	if f.handle == nil {
		return nil, ErrClosed
	}

	h, ok := f.handle.(dragoonFileHandle)
	if !ok {
		return nil, ErrInvalid
	}

	if uintptr(h) == dirFileHandle {
		st, ok := dragoonStatPath(f.name)
		if !ok {
			return nil, &PathError{Op: "stat", Path: f.name, Err: ErrNotExist}
		}
		return statFromRecord(&st, basename(f.name)), nil
	}

	var st dragoonStat
	if dragoonFstatCall(h.fd(), &st) < 0 {
		return nil, &PathError{Op: "fstat", Path: f.name, Err: errDragoonIO}
	}

	return statFromRecord(&st, basename(f.name)), nil
}

func statNolog(name string) (FileInfo, error) {
	st, ok := dragoonStatPath(name)
	if !ok {
		return nil, &PathError{Op: "stat", Path: name, Err: ErrNotExist}
	}
	return statFromRecord(&st, basename(name)), nil
}

func lstatNolog(name string) (FileInfo, error) {
	return statNolog(name)
}

/* ---- Directory iteration ------------------------------------------------ */

func direntName(ent *dragoonDirent) string {
	n := 0
	for n < len(ent.Name) && ent.Name[n] != 0 {
		n++
	}
	return string(ent.Name[:n])
}

func direntInfo(name string, ent *dragoonDirent) FileInfo {
	mode := FileMode(0644)
	if ent.IsDir != 0 {
		mode = FileMode(0755) | ModeDir
	}
	return &fileStat{name: name, size: int64(ent.Size), mode: mode}
}

func (f *File) readdir(n int, mode readdirMode) (names []string, dirents []DirEntry, infos []FileInfo, err error) {
	if f == nil || f.handle == nil {
		return nil, nil, nil, ErrInvalid
	}

	if n == 0 {
		n = -1
	}

	cb := dragoonCString(f.name)
	added := 0

	for n != 0 {
		var ent dragoonDirent

		res := dragoonReaddir(&cb[0], int32(f.dirIndex), &ent)
		if res < 0 {
			if added == 0 {
				return nil, nil, nil, &PathError{Op: "readdir", Path: f.name, Err: errDragoonIO}
			}
			break
		}
		if res == 0 {
			break
		}

		f.dirIndex++

		name := direntName(&ent)
		if name == "." || name == ".." {
			continue
		}

		info := direntInfo(name, &ent)

		switch mode {
		case readdirName:
			names = append(names, name)
		case readdirDirEntry:
			dirents = append(dirents, fs.FileInfoToDirEntry(info))
		default:
			infos = append(infos, info)
		}

		added++
		if n > 0 {
			n--
		}
	}

	if n > 0 && added == 0 {
		return nil, nil, nil, io.EOF
	}

	return names, dirents, infos, nil
}
