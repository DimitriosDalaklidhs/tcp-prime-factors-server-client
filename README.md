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


