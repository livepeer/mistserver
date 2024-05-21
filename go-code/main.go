package main

import (
	"fmt"
	"os"
)
import "C"

//export AquareumMain
func AquareumMain() {
	fmt.Println("welcome to aquareum. this part is coming from go.")
	fmt.Printf("%v\n", os.Args)
	my_path, err := os.Executable()
	if err != nil {
		panic(err)
	}
	fmt.Println(my_path)
	os.Exit(5)
}

func main() {
	fmt.Println("this is a main package i guess")
}
