package main

import (
	"fmt"
	"os"
)

func main() {
	fmt.Printf("argc=%d\n", len(os.Args))
	for i, arg := range os.Args {
		fmt.Printf("argv[%d]=%s\n", i, arg)
	}
}
