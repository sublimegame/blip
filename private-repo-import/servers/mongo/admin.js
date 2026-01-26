// admin.js
admin = db.getSiblingDB("admin")
// creation of the admin user
admin.createUser(
  {
    user: "admin",
    pwd: "5GBUJ3EJMGHW6SYJ",
    roles: [ { role: "userAdminAnyDatabase", db: "admin" } ]
  }
)
// let's authenticate to create the other user
db.getSiblingDB("admin").auth("admin", "5GBUJ3EJMGHW6SYJ" )
// creation of the replica set admin user
db.getSiblingDB("admin").createUser(
  {
    "user" : "replicaAdmin",
    "pwd" : "2XXATQC7133ZPXNU",
    roles: [ { "role" : "clusterAdmin", "db" : "admin" } ]
  }
)