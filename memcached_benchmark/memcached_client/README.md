Memcached's client for benchmarking and testing.

The innovation in the following client-benchmark is that it's failure resistant. In case a connection with Memcached's server is lost, the client tries to continue the caching (both sending and retrieving data) from another server (or reconnecting to the same server, configuration depending). Connection failure can happen twice - if the second connection also fails, then the client tries to continue with a third server. That means that if the data is backed up in other server (up to two servers), the client can continue to work seamlessly (with only a small delay for establishing new connection), without adding cache misses.

The benchmark shows regular cache misses, together with total misses - i.e. the misses that happen when server falls, and the relevant data is not yet backed up in another server.

The following client is a modification of CloudSuite's client, and those is distributed under the same licence. Please see the licence below.
The modification of CloudSuite's client was developed by Samyon Ristov from Hebrew University of Jerusalem, under the supervision of Prof. Danny Dolev, Tal Anker and Yaron Weinsberg.

Known Issues:
- Events are only added (after a server goes down) and never deleted. Therefore the benchmark is not really scalable. To solve the issue deletion of the events should be added after a minute in case a server went down (or after many 'empty' events happened). The reason the events are not deleted immediately is because there are always some events in the queue that should be handled before the deletion.

- In order of failover to work, all connections must TX at least once before server goes down.
- "total misses" is always 0
- There is a lot of logs during reconnection.
- On read response, there is a missed place with possible read from socket - the program can fail there.
- Failover is implemented only for TCP.

Usage:
Currently the usage is the same as in the original benchmark. See usage in Parsa's site (link below) or see a copy here:
----------------------------------------------------------------
Data Caching

This benchmark uses the Memcached data caching server, simulating the behavior of a Twitter caching server using the twitter dataset. The metric of interest is throughput expressed as the number of requests served per second. The workload assumes strict quality of service guaranties.

Download the Data Caching benchmark.
Prerequisite Software Packages

   1. Memcachd v1.4.15
   2. Libevent v2.0.21 library required to run Memcached

Building Memcached

   1. Untar the libevent package and configure it.

      tar zxvf libevent-2.0.21-stable.tar.gz
      cd libevent-2.0.21-stable
      ./configure
      make && make install
      These commands will install the library at the default location (/usr/lib/). To change the destination folder, you should configure the package with a different command:
      ./configure --prefix=/path/to/folder/

   2. Untar the Memcachd package and configure it.

      tar zxvf memcached-1.4.15.tar.gz
      cd memcached-1.4.15
      ./configure
      In case you did not install libevent at the default location, you should add the following parameter:
      --with-libevent=/path/to/libevent/

      In case you want to install memcached at a specific location, you should add:
      --prefix=/path/to/memcached/
      For example:
      ./configure --prefix=/path/to/memcached/ --with-libevent=/path/to/libevent/

   3. Build and install memcached:
      make && make install

Starting the server

The following command will start the server with four threads and 4096MB of dedicated memory, with a minimal object size of 550 bytes:
memcached -t 4 -m 4096 -n 550
The default port that the server will be started on is 11211. You can change it by adding the port parameter (e.g., -p 11212).
Preparing the client

Change the directory to memcached/memcached_client/

In case you did not install libevent at the default location, you should update the Makefile:
-L path/to/libevent/lib -I path/to/libevent/include

Compile the client code by typing:
make

Then prepare the server configuration file, which includes the server address and the port number to connect to, in the following format:
server_address, port

The client can simultaneously use multiple servers, one server with several ip addresses (in case the server machine has multiple ethernet cards active), and one server through multiple ports, measuring the overall throughput and quality of service. In that case, each line in the configuration file should contain the server address and the port number. For example:
localhost, 11211
127.0.0.1, 11212
n116.epfl.ch, 11213
192.168.10.116, 11213
Scaling the dataset and warming up the server

The following command will create the dataset by scaling up the Twitter dataset, while preserving both the popularity and object size distributions. The original dataset consumes 300MB of server memory, while the recommended scaled dataset requires around 10GB of main memory dedicated to the memcached server (scaling factor of 30).
./loader -a ../twitter_dataset/twitter_dataset_unscaled -o ../twitter_dataset/twitter_dataset_30x -s servers.txt -w 1 -S 30 -D 4096 -j -T 1

(w - number of client threads, S - scaling factor, D - target server memory, T - statistics interval, s - server configuration file, j - an indicator that the server should be warmed up).

If the scaled file is already created, but the server is not warmed up, use the following command to warm up the server:
./loader -a ../twitter_dataset/twitter_dataset_30x -s servers.txt -w 1 -S 1 -D 4096 -j -T 1

Running the benchmark

To determine the maximum throughput while running the workload with eight client threads, 200 TPC/IP connections, and a get/set ration of 0.8, please type:
./loader -a ../twitter_dataset/twitter_dataset_30x -s servers.txt -g 0.8 -T 1 -c 200 -w 8

This command will run the benchmark with the maximum throughput, however, the QoS requirements will highly likely be violated. Once the maximum throughput is determined, you should run the benchmark using the following command:

./loader -a ../twitter_dataset/twitter_dataset_30x -s servers.txt -g 0.8 -T 1 -c 200 -w 8 -e -r rps

where rps is 90% of the maximum number of requests per second achieved using the previous command. You should experiment with different values of rps to achieve the maximum throughput without violating the target QoS requirements.
Important remarks

    1. It takes several minutes for the server to reach a stable state.
    2. The target QoS requires that 95% of the requests are executed within 10ms
    3. Memcached has known scalability problems, scaling very poorly beyond four threads. To utilize a machine with more than four cores, you should start several server processes and add the corresponding parameters into the client configuration file.
    4. The benchmark is network-intensive and requires a 10Gbit Ethernet card not to be network-bound. Multiple ethernet cards could be used as well, each with a different IP address (two servers in the client configuration file with the same socket, but different IP address). Multisocket machines could also avoid the network problem by running the server and the client on different sockets of the same machine (e.g., pinned using taskset), communicating via localhost.

----------------------------------------------------------------

Original benchmark:
http://parsa.epfl.ch/cloudsuite/memcached.html
Link to original license:
http://parsa.epfl.ch/cloudsuite/licenses.html




Licence:
CloudSuite 2.0 License

CloudSuite 2.0 Benchmark Suite
Copyright (c) 2011-2013, Parallel Systems Architecture Lab, EPFL
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
    nor the names of its contributors may be used to endorse or promote
    products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY, EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Original README:

Memcached Loadtester, 2010-2013
David Meisner (meisner@fb.com) - Facebook, University of Michigan  
Djordje Jevdjic (djordje.jevdjic@epfl.ch)  - Ecole Polytechnique Federale de Lausanne (EPFL)

For questions concerning the workload, please contact Djordje Jevdjic at djordje.jevdjic@epfl.ch,
or ask your question using our mailing list (cloudsuite@listes.epfl.ch)

