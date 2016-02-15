Hebrew University of Jerusalem's project integrating Memcached with RDMA in order to enable server's side failover.

Memcached becomes more and more popular as a network function utility. Hence it's interesting to embed it into cloud environment, enabling scalability and failover.
The current project tries different approaches in doing so, focusing on Memcached's failover under different consistency modules and using various techniques.

As a first step, it was decided to integrate Memcached with RDMA - that is, all the changes will be made only on the server side, transparent to Memcached's clients, and the communication between the servers (which is not a native Memcached's feature) will be done mostly via RDMA, examining its potential.

more details will be added soon.

Developed by Samyon Ristov from Hebrew University of Jerusalem, under the supervision of Prof. Danny Dolev, Tal Anker and Yaron Weinsberg.
