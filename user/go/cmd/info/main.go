package main

import (
	"fmt"
	"runtime"
)

func main() {
	fmt.Println("=== DragoonOS TinyGo Userland Utility ===")
	fmt.Printf("Architecture : %s\n", runtime.GOARCH)
	fmt.Printf("OS Target    : %s\n", runtime.GOOS)
	fmt.Printf("Compiler     : %s\n", runtime.Compiler)

	// Test dynamic heap allocation (exercises sbrk / conservative GC)
	items := make([]string, 0, 4)
	items = append(items, "Ring 3 Execution: OK")
	items = append(items, "Heap Allocation : OK")
	items = append(items, "FAT32 Bundling  : OK")

	fmt.Println("\nSystem Checks:")
	for i, status := range items {
		fmt.Printf("  [%d] %s\n", i+1, status)
	}
}
