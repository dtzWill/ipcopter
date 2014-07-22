package main

// Server entry point

func StartServer() {
	Context := NewContext()
	listenForClients(Context)
}

func main() {
	StartServer()
}
