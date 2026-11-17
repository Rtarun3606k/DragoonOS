
package main

import "dragoon_user/dragoon"

func main() {
	words := []string{"Hello", "from", "TinyGo", "running", "on", "DragoonOS!"}
	for i, w := range words {
		if i > 0 {
			dragoon.Print(" ")
		}
		dragoon.Print(w)
	}
	dragoon.Println("")
}
