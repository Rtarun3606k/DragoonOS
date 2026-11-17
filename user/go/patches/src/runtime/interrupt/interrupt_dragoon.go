//go:build dragoon

package interrupt

type State uintptr

func Disable() State {
	return 0
}

func Restore(state State) {
}

func In() bool {
	return false
}
