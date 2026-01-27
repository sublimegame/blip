# Migrating ScyllaDB to new infrastructure

## List of tables

- ✅ kvs.leaderboard                              OK
- ✅ kvs.unordered                                OK

- ✅ moderation.username                          OK
- ✅ moderation.chat_message                      OK

- ✅ universe.user_notification                   OK
- ✅ universe.parent_dashboard                    OK
- ✅ universe.module                              OK
- ✅ universe.module_ref                          OK
- ✅ universe.module_contributor                  OK
- ✅ universe.relation                            OK
- ✅ universe.user_balance                        OK
- ✅ universe.user_balance_lock                   OK
- ✅ universe.transaction_group                   OK
- ✅ universe.grant_coin_transaction_by_user      OK
- ✅ universe.dummy_coin_transaction_by_user      OK
- ✅ universe.earn_coin_transaction_by_user       OK
- ✅ universe.purchase_coin_transaction_by_user   OK
- ✅ universe.treasury_transaction_grant_coin     OK
- ✅ universe.treasury_transaction_dummy_coin     OK
- ✅ universe.treasury_transaction_earn_coin      OK
- ✅ universe.treasury_transaction_purchase_coin  OK

## CQL script to create entire schema

It is in file `db_schema_manual.cql`.

## Backup tables content

```bash
cqlsh -e "COPY kvs.leaderboard TO 'kvs.leaderboard.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY kvs.unordered TO 'kvs.unordered.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY moderation.username TO 'moderation.username.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY moderation.chat_message TO 'moderation.chat_message.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_notification TO 'universe.user_notification.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.parent_dashboard TO 'universe.parent_dashboard.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.module TO 'universe.module.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.module_ref TO 'universe.module_ref.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.module_contributor TO 'universe.module_contributor.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.relation TO 'universe.relation.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_balance TO 'universe.user_balance.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_balance_lock TO 'universe.user_balance_lock.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.transaction_group TO 'universe.transaction_group.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.grant_coin_transaction_by_user TO 'universe.grant_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.dummy_coin_transaction_by_user TO 'universe.dummy_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.earn_coin_transaction_by_user TO 'universe.earn_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.purchase_coin_transaction_by_user TO 'universe.purchase_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_grant_coin TO 'universe.treasury_transaction_grant_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_dummy_coin TO 'universe.treasury_transaction_dummy_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_earn_coin TO 'universe.treasury_transaction_earn_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_purchase_coin TO 'universe.treasury_transaction_purchase_coin.csv' with DELIMITER = ';' and Header=true;"
```

## Restore tables content

```bash
cqlsh -e "COPY kvs.leaderboard FROM 'kvs.leaderboard.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY kvs.unordered FROM 'kvs.unordered.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY moderation.username FROM 'moderation.username.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY moderation.chat_message FROM 'moderation.chat_message.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_notification FROM 'universe.user_notification.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.parent_dashboard FROM 'universe.parent_dashboard.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.module FROM 'universe.module.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.module_ref FROM 'universe.module_ref.csv' with DELIMITER = ';' and Header=true and NULL='_null_';"
cqlsh -e "COPY universe.module_contributor FROM 'universe.module_contributor.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.relation FROM 'universe.relation.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_balance FROM 'universe.user_balance.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.user_balance_lock FROM 'universe.user_balance_lock.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.transaction_group FROM 'universe.transaction_group.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.grant_coin_transaction_by_user FROM 'universe.grant_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.dummy_coin_transaction_by_user FROM 'universe.dummy_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.earn_coin_transaction_by_user FROM 'universe.earn_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.purchase_coin_transaction_by_user FROM 'universe.purchase_coin_transaction_by_user.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_grant_coin FROM 'universe.treasury_transaction_grant_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_dummy_coin FROM 'universe.treasury_transaction_dummy_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_earn_coin FROM 'universe.treasury_transaction_earn_coin.csv' with DELIMITER = ';' and Header=true;"
cqlsh -e "COPY universe.treasury_transaction_purchase_coin FROM 'universe.treasury_transaction_purchase_coin.csv' with DELIMITER = ';' and Header=true;"
```

## CQLSH

```bash
# /cubzh/scylla/connect.sh on czh-prod-worker-3 machine
docker run -it --rm --entrypoint cqlsh -v /cubzh/scylla/cqlshrc:/root/.cassandra/cqlshrc scylladb/scylla-cqlsh
```

```bash
# get a shell with cqlsh
docker run -it --rm --entrypoint bash -v /cubzh/scylla/cqlshrc:/root/.cassandra/cqlshrc -v /cubzh/scylla/output:/output scylladb/scylla-cqlsh

# run cqlsh in interactive mode
cqlsh

# get the schema into a local file
cqlsh -e "DESC SCHEMA WITH INTERNALS" > /output/db_schema_2.cql
```

```bash
# list tables
cqlsh -e "DESCRIBE TABLES"

# list keyspaces
cqlsh -e "DESCRIBE KEYSPACES"
```

## ScyllaDB test container

```bash
docker run --rm -ti --name scylla-test -d -v /Users/gaetan/scylla-test:/scylla scylladb/scylla
```
