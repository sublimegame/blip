[...]

## Join Docker Swarm cluster

We need to make the new machine (AWS EC2 instance) join the existing Swarm cluster.

### Add worker node to swarm cluster

On a Swarm manager node, you can obtain the command to call on the new machine:

```
$ docker swarm join-token worker
To add a worker to this swarm, run the following command:

    docker swarm join --token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s 15.237.83.213:2377
```

Call it on the new machine:

- `--advertise-addr` is the public IP of the new machine

```
$ docker swarm join \
--token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s \
--advertise-addr 3.16.138.239 \
15.237.83.213:2377

This node joined a swarm as a worker.
```

On the Swarm manager, you can check that the new node has been added:

```
$ docker node ls
ID                            HOSTNAME                  STATUS    AVAILABILITY   MANAGER STATUS   ENGINE VERSION
[...]
1bw5ckrgmnav77tp71nq7w2pd     czh-prod-scylladb         Ready     Active                          26.1.3
```

## Create directories on the host to store database files

```bash
sudo mkdir -p /cubzh/scylla/data /cubzh/scylla/commitlog /cubzh/scylla/hints /cubzh/scylla/view_hints

# Add directory for backups
sudo mkdir -p /cubzh/scylla/backups
```

## Add Docker Swarm label on the machine

```bash
# On a Swarm manager node
docker node update --label-add scylladb=1 czh-prod-scylladb
```
