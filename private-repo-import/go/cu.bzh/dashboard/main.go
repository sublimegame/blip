package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
	"golang.org/x/crypto/bcrypt"
)

var client *mongo.Client

const (
	username = "cubzh"
	password = "xdodVxTzFQFzmfU"
	dbURI    = "mongodb://user-db:27017"
)

var (
	hashedPassword = ""
)

func main() {
	var err error
	hashedPassword, err = hashPassword(password)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("hashedPassword:", hashedPassword)

	// MongoDB connection
	clientOptions := options.Client().ApplyURI(dbURI)

	client, err = mongo.Connect(context.TODO(), clientOptions)
	if err != nil {
		log.Fatal(err)
	}

	err = client.Ping(context.TODO(), nil)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("Connected to MongoDB!")

	http.HandleFunc("/", BasicAuth(dashboardHandler))
	http.HandleFunc("/search", BasicAuth(searchHandler))

	fmt.Println("Serving...")
	log.Fatal(http.ListenAndServe(":80", nil))
}

func BasicAuth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		user, pass, ok := r.BasicAuth()
		if !ok || user != username || !checkPasswordHash(pass, hashedPassword) {
			w.Header().Set("WWW-Authenticate", `Basic realm="Restricted"`)
			http.Error(w, "Unauthorized.", http.StatusUnauthorized)
			return
		}
		next(w, r)
	}
}

func hashPassword(password string) (string, error) {
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), 14)
	return string(bytes), err
}

func checkPasswordHash(password, hash string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(hash), []byte(password))
	return err == nil
}

func dashboardHandler(w http.ResponseWriter, r *http.Request) {
	http.ServeFile(w, r, "dashboard.html") // Serve the HTML file for the dashboard
}

func searchHandler(w http.ResponseWriter, r *http.Request) {
	searchValue := r.URL.Query().Get("value")

	collection := client.Database("particubes").Collection("users") // Change the database and collection names
	var results []bson.M

	filter := bson.M{
		"$or": []bson.M{
			{"id": searchValue},
			{"email": searchValue},
			{"username": searchValue},
			{"phone": searchValue},
		},
	}

	cursor, err := collection.Find(context.TODO(), filter)
	if err != nil {
		http.Error(w, "User not found.", http.StatusNotFound)
		return
	}
	defer cursor.Close(context.TODO())

	for cursor.Next(context.TODO()) {
		var result bson.M
		err := cursor.Decode(&result)
		if err != nil {
			http.Error(w, "Error decoding user.", http.StatusInternalServerError)
			return
		}
		results = append(results, result)
	}

	if err := cursor.Err(); err != nil {
		http.Error(w, "Error iterating cursor.", http.StatusInternalServerError)
		return
	}

	if len(results) > 10 {
		results = results[:10]
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(results)
}
