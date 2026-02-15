# Notes

## LAUNCH SCYLLA IN DOCKER

```bash
# launch container
docker run --name scyllaU -d scylladb/scylla:6.2.2 --overprovisioned 1 --smp 1
docker run --name scyllaU -p 9042:9042 -d scylladb/scylla:6.2.2 --overprovisioned 1 --smp 1

# check status
docker exec -it scyllaU nodetool status

# launch CQL shell
docker exec -it scyllaU cqlsh
```

## TEST LOCALLY

Launch ScyllaDB container locally

```bash
docker run --name scyllaU -p 9042:9042 -d scylladb/scylla:6.2.2 --overprovisioned 1 --smp 1
```

Execute CQL code to create tables.

```bash
docker exec -it scyllaU cqlsh
# execute CQL commands here
```

Run tests using `localhost:9042` as database server address.

```bash
go test
```

## Misc

replication factor (RF) ? 3 ?
consistency level (CL) ?

## Needs

- what about KVS indexed by world ID when we will have publishing (1 UUID per world version ?)
- Un autre point c'est qu'il faut pouvoir mesurer quantité de data utilisée par chaque jeu. Ça peut se laisser pour plus tard. Mais juste pour prévoir une solution
- WEEKLY : Un world peut mettre son KVS en public (lecture seule) ?
