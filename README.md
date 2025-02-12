#Simple HTTP Server in C

A lightweight HTTP server written in **C** that supports basic HTTP methods (`GET` and `POST`) and serves static files.  

---

##Overview

This server allows users to:

 **Retrieve the `User-Agent` header**  
 **Echo back messages** via `/echo/{message}`  
 **Serve static files** from a specified directory  
 **Upload files** using `POST /files/{filename}`  
 **Handle root (`/`) requests** with a `200 OK` response  
 **Return `404 Not Found`** for unrecognized routes  

It uses **fork()** to handle multiple client connections and is built for simplicity.

---

## ⚙️ Compilation & Execution

gcc -o http_server http_server.c

Run the server by specifying a directory for file storage
./http_server --directory ./files

