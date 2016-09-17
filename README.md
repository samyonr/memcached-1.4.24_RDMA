# Memcached (1.4.24) with RDMA Backup

Hebrew University of Jerusalem's proof of concept project integrating Memcached with RDMA in order to enable server's side failover.

Memcached becomes more and more popular as a network function utility. Hence it's interesting to embed it into cloud environment, enabling scalability and failover. The current project is focusing on Memcached's failover, via BSD Sockets and via RDMA communication, using Accelio library. All the changes are made only on the server side, transparent to Memcached's clients.

See more details under Description and Implementation section. 

Developed by Samyon Ristov from Hebrew University of Jerusalem, under the supervision of Prof. Danny Dolev, Dr. Tal Anker and Dr. Yaron Weinsberg. Thanks to Benjamin (Ben) Chaney for developing sharedmalloc library and integrating OFED's library into the project.
 
## Dependencies

* libevent, http://www.monkey.org/~provos/libevent/ (libevent-dev)
* ofed, http://downloads.openfabrics.org/OFED/
* accelio, https://github.com/accelio/accelio

## Hardware

Netgear ProSafe XS708E Switch - a 8-port 10-Gigabit ProSAFE® Plus Switch.

Mellanox Technologies MT27520 Family. ConnectX-3 Pro.

## Environment

### Linux

The project was developed under Ubuntu 14.10. It has not been tested under any other OS, although there should not be any problem with other Debian distributions.

## Description and Implementation

### Backward Compatibility

The project is based on Memcached (https://github.com/memcached/memcached), version 1.4.24.  All the code is backward compatible. You can run this version of Memcached in the same way as running the native Memcached v1.4.24. All the added functionality can be enabled on Memcached’s startup via specific configuration in the command line. Specific configurations will be described later.

Unfortunately the project was not forked from the code of the original branch, but downloaded and re-uploaded. Therefore while Memcached will move on this code will stay on version 1.4.24. But it's just a PoC project, and as a PoC it did its job. More sophisticated project, as mentioned in the Contributing section, will be forked from Memcached's source code. Nevertheless, there are things that can be learned from this project and they are described here.

### High Level Design

Memcached acts as a server. It listens to client’s communication and reacts to it. Since this part wasn’t changed at all, the terms “client” and “server” will be used for a different purpose, clarified now. In this project, each Memcached instance can be a backup client or backup server. At every moment only one Memcached instance serves the user. This instance is also a backup client. When starting Memcached you can define what will be its backup servers. When Memcached Backup Client (Memcached client from now on) stores new data, it’s also responsible for sending this data to Memcached Backup Server (Memcached server from now on).

For example you can start two Memcached instances on different machines. One instance will be Memcached server and the second will be Memcached client. The client serves the user and backups all the data on Memcached server. If something happens to that instance the user may move to the backup Memcached, and continue to work with it. This switching from one Memcached instance to another can be transparent to the user if the new instance receives the previous instance's IP.  When Memcached receives communication from the user it acts as Memcached client, and starts the communication and the backup process with Memcached servers.

If the communication with Memcached server is lost, Memcached client will not attempt to reconnect - this is one of the features that have to be added to production level version of the project. Since on every STORED event Memcached client backups all the data on Memcached servers, even if the connection is lost with Memcached client and then reestablished, Memcached server will not miss any data. Storing all the data on every STORED event takes a lot of time, and should not be used in real, production level, systems, but as a PoC it will do.

### Managing Memory

Native Memcached runs in-memory and allocated three memory sections that store the user’s data. These memory sections are allocated in assoc.c and slabs.c files. In assoc.c the allocated memory is called primary_hashtable, and in slabs.c one of the allocated sections is called mem_base, and the other one is a list that is called slab_list and it's a part of slabclass. On native Memcached the slab list can be pre-allocated but it’s size is calculated on run time,and is allocated node by node. In this project slab_list is modified into mem_slabs_lists_base, which can be pre-allocated with one allocation operation, and it’s size is known before the allocation.

In order to make the backup work, preallocation must be done. See Configurations subsection which describe how to preallocate all the memory. The allocated memory sizes are:

```sh
assoc.c (primary_hashtable) - 16 MB (actually, 16 MiB)
slabs.c (mem_base) - 4 GB
slabs.c (mem_slabs_lists_base) - 4 KB
```

In parallel to allocating memory on RAM, three files are created. The files contains the same data as the preallocated memory, and they are created with sharedmalloc.c, which allocates shared memory of a given size. The memory is shared across all processes that use the same key. sharedmalloc is implemented using mmap. These files can be used for solving cold-cache, since one can upload them into the memory after Memcached is up.

### Backup process

The backup is possible in two different ways, via the standard BSD Sockets communication and via RDMA using Accelio library. The last requires a designated hardware, for example the one described in the Hardware section.

Memcached in this project creates two new threads - one for backup client, and the other for backup server. On every STORED event (see complete_nread_ascii in memcached.c) a node is added to queue. The queue act as a sign to do a backup. While there is nodes in the queue the backup process will continue. When the queue is empty the backup process will stop and wait until new node is added to the queue. That notification mechanism could be done without any queue (and it was implemented without a queue at first), but using a queue is a preparation for sending more sophisticated data to the backup thread.

On receiving a node from the queue, the client’s thread stops its normal behaviour, loads the stored data from the three saved files, and transmits it to Memcached server. A better implementation would be to backup without the use files, doing everything in memory.

Memcached server receives these files, copies them to the relevant memory section and saves them on the disk. From that moment Memcached server got the same data as Memcached client, and when the user will ask something from Memcached server, it will see in its memory the same values as in Memcached client, and will respond with the same answer.

It’s worth to mention that when transmitting data via RDMA, in order to keep the connection alive Accelio have to send beacon messages all the time. The Memcached client and Memcached server always communicating with each other, and Memcached client checks the queue only when it receives a response from the Memcached server. I could not find a other way to disable this chit chat between two Accelio nodes.

### Configurations

The project used the following topology:

<p align="center">
<img src ="https://github.com/samyonr/Cloud-Memcached/blob/master/memcached-1.4.24_RDMA/images/topology.png" />
</p>

In order to run BSD Socket failover use the following configuration:

On HA-102
```sh
-t 4 -m 4096 -o hashpower=21 -n 550 -L -u a315 -o shared_malloc_slabs=slabs_key1 -o shared_malloc_assoc=assoc_key1 -o shared_malloc_slabs_lists=slabs_lists_key1 -o failover_dest=10.0.1.1:5555 -o failover_src=10.0.1.2:5555 -o failover_comm_type=TCP
```
On HA-101
```sh
-t 4 -m 4096 -o hashpower=21 -n 550 -L -u a315 -o shared_malloc_slabs=slabs_key1 -o shared_malloc_assoc=assoc_key1 -o shared_malloc_slabs_lists=slabs_lists_key1 -o failover_dest=10.0.1.2:5555 -o failover_src=10.0.1.1:5555 -o failover_comm_type=TCP
```

In order to run RDMA failover use the following configuration:

On HA-102
```sh
-t 4 -m 4096 -o hashpower=21 -n 550 -L -u a315 -o shared_malloc_slabs=slabs_key1 -o shared_malloc_assoc=assoc_key1 -o shared_malloc_slabs_lists=slabs_lists_key1 -o failover_dest=10.0.0.1:5555 -o failover_src=10.0.0.2:5555 -o failover_comm_type=RDMA
```
On HA-101
```sh
-t 4 -m 4096 -o hashpower=21 -n 550 -L -u a315 -o shared_malloc_slabs=slabs_key1 -o shared_malloc_assoc=assoc_key1 -o shared_malloc_slabs_lists=slabs_lists_key1 -o failover_dest=10.0.0.2:5555 -o failover_src=10.0.0.1:5555 -o failover_comm_type=RDMA
```

### Testing

In order to test this project, use two machines. I named them 10.0.0.1 and 10.0.0.2, see Configurations section (you also can use one machine, with two different Memcached ports), and create Memcached on both of them with the configuration described in Configurations section

Activate telnet on one of the machines or on a third machine: telnet 10.0.0.1 11211.

Write:
```sh
set greeting 1 0 11
Hello world
```

You will receive the following answer:
```sh
STORED
```

Then close the memcached on machine 10.0.0.1, and activate telnet again: telnet 10.0.0.2 11211.

Now write:
```sh
get greeting
```

You will receive the following answer:
```sh
VALUE greeting 1 11
Hello world
END
```

Which indicates that the Memcached on machine 10.0.0.2 received the stored information on machine 10.0.0.1.

## Other Solutions

I'm providing here a list of solution for Memcached replication and failover:

repached - a server side memcached replication solution (http://repcached.lab.klab.org/)

yrmcds - Memcached compatible server side failover solution (http://cybozu.github.io/yrmcds/)

libMemcached - Server side replication solution (http://libmemcached.org/libMemcached.html)

MemcacheDB - Memcached compatible DB solution (http://memcachedb.org/)

Couchbase - Memcached compatible DB solution (http://www.couchbase.com/)


## Contributing

This project is a PoC, and as a next step I will work on more sophisticated, robust and fast backup implementations. The focus will be on working with native RDMA verbs API, and trying other alternative approaches to embedding code into Memcached's source code - for example using CRIU (https://criu.org/). Take this code and try everything you want for yourself, or contact me for more information. You can contact with me via Linkedin, or in the following email address (Switch every symbol to the corresponding letter. This is used for protection against bots).
```sh
@ = a (except before "mail"), 0 = o, $ = s
```

```sh
s @ m y 0 n . r i $ t 0 v @mail . huji . ac . il 
```
