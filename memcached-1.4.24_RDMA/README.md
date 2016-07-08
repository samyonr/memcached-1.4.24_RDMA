# Memcached (1.4.24) with RDMA Backup

Hebrew University of Jerusalem's proof of concept project integrating Memcached with RDMA in order to enable server's side failover.

Memcached becomes more and more popular as a network function utility. Hence it's interesting to embed it into cloud environment, enabling scalability and failover. The current project is focusing on Memcached's failover, via TCP and via RDMA communication, using Accelio library. All the changes will be made only on the server side, transparent to Memcached's clients.

See more details under Description and Implementation section. 

Developed by Samyon Ristov from Hebrew University of Jerusalem, under the supervision of Prof. Danny Dolev, Tal Anker and Yaron Weinsberg. Thanks to Benjamin (Ben) Chaney for developing sharedmalloc and integrating OFED's library.
 
## Dependencies

* libevent, http://www.monkey.org/~provos/libevent/ (libevent-dev)
* ofed, http://downloads.openfabrics.org/OFED/
* accelio, https://github.com/accelio/accelio

## Environment

### Linux

The project was developed under Ubuntu 14.10, and never tested under any other OS.

## Description and Implementation

will be added soon

## Contributing

This project is a PoC, and as a next step I will work on more sophisticated and fast backup implementations. The focus will be on working with native RDMA verbs API, and trying other alternative approaches to embedding code into Memcached's source code - for example using CRIU (https://criu.org/). Take this code and try everything you want for yourself, or contact me for more information. You can contact with me via Linkedin, or in the following email address (letters changed protection against bots. Switch every symbol to the corresponding letter. @ = a [except before "mail"], 0 = o, $ = s): s @ m y 0 n . r i $ t 0 v @mail . huji . ac . il 
