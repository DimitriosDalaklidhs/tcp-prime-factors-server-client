# TCP Prime Factors Server & Client

A C-based TCP client-server application that exchanges integers and computes their prime factors.  
Originally developed as coursework at the **University of Western Macedonia**, later refined and published as a personal **repository**.

---

## Overview

This project demonstrates low-level network programming in C using the **Berkeley sockets API**.  
It includes both a server and a client:

- **Server**  
  - Listens on TCP port `8080`  
  - Spawns a child process (`fork()`) for each incoming connection  
  - Prompts the client for a positive integer  
  - Validates input and returns its **prime factors**  
  - Closes connection gracefully

- **Client**  
  - Connects to the server using an IP passed as a command-line argument  
  - Exchanges data through sockets  
  - Displays the server’s messages and the calculated prime factors  

##  Key Concepts Demonstrated

- TCP socket creation and communication  
- Process forking for handling multiple clients  
- String parsing and numeric validation  
- Prime factorization algorithm in C  
- Graceful handling of socket errors  



# Compile
gcc -o server server.c
gcc -o client client.c
# Run server
./server

# Run client (in another terminal)
./client 127.0.0.1


# Author

Dimitrios Dalaklidis is an aspiring backend developer with a strong academic foundation in Informatics and hands-on experience in systems programming, data structures, and software architecture. His work reflects a methodical approach to problem solving, supported by practical exposure to multi-language development environments and structured programming disciplines. He has completed a range of projects involving low-level system operations in C, object-oriented application design in Java, browser-based scripting, and networked communication models.

His technical interests center on backend system design, algorithmic efficiency, and the construction of reliable, maintainable software. He actively pursues opportunities to expand his expertise through academically driven projects and independent research, with an emphasis on building robust systems that adhere to professional development practices and modern software engineering principles.

For professional communication, he can be reached at: dalaklidesdemetres@gmail.com
