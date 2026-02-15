package dbclient

import (
	"context"
	"errors"
	"fmt"
	"log"
	"regexp"
	"time"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
	"go.mongodb.org/mongo-driver/x/bsonx"
)

// // You can edit this code!
// // Click here and start typing.
// package main

// import (
// 	"fmt"
// 	"reflect"
// )

// type Vertex struct {
// 	X int `json:"foo"`
// 	Y int `json:"bar"`
// }

// func main() {
// 	v := Vertex{1, 2}
// 	fmt.Println(getTag(&v, "X").Get("json"))
// }

// func getTag(v *Vertex, fieldName string) reflect.StructTag {
// 	r, _ := reflect.TypeOf(v).Elem().FieldByName(fieldName)
// 	//f := reflect.Indirect(r).FieldByName(field)
// 	return r.Tag
// }

type CollectionName int

const (
	mongoConnectString                        = "mongodb://user-db:27017" // "mongodb://localhost:27017"
	connectTimeout                            = 8 * time.Second
	indexesConstructionTimeout                = 30 * time.Second
	requestTimeout                            = 2 * time.Second
	GameCollectionName         CollectionName = 1
	GameLikeCollectionName     CollectionName = 2
	GameCommentCollectionName  CollectionName = 3
	ItemCollectionName         CollectionName = 4
	ItemLikeCollectionName     CollectionName = 5
	ItemCommentCollectionName  CollectionName = 6
	UserCollectionName         CollectionName = 7
)

var (
	sharedDBClient *Client = nil
)

type Client struct {
	MongoClient           *mongo.Client // MongoClient will later be hidden (not exposed)
	cubzhDatabase         *mongo.Database
	gameCollection        *mongo.Collection
	gameLikeCollection    *mongo.Collection
	gameCommentCollection *mongo.Collection
	itemCollection        *mongo.Collection
	itemLikeCollection    *mongo.Collection
	itemCommentCollection *mongo.Collection
	usersCollection       *mongo.Collection
}

func (c *Client) InsertNewRecord(collectionName CollectionName, record interface{}) error {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	recordBson, err := bson.Marshal(record)
	if err != nil {
		return err
	}

	_, err = collection.InsertOne(ctx, recordBson)
	return err
}

func (c *Client) IncrementFieldInRecord(collectionName CollectionName, filter map[string]interface{}, field string, increment int) error {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	update := bson.D{{Key: "$inc",
		Value: bson.D{
			{Key: field, Value: increment},
		},
	}}

	_, err := collection.UpdateOne(ctx, filter, update)
	return err
}

// GetAllRecords ...
func (c *Client) GetAllRecords(collectionName CollectionName, records interface{}) error {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filter := bson.D{} // empty filter
	cursor, err := collection.Find(ctx, filter)
	if err != nil {
		return err
	}
	defer cursor.Close(ctx)

	err = cursor.All(ctx, records)
	return err
}

func (c *Client) GetRecordsWithFieldHavingOneOfValues(collectionName CollectionName, field string, values []string, records interface{}) error {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filter := bson.D{{Key: field, Value: bson.D{{Key: "$in", Value: values}}}}
	cursor, err := collection.Find(ctx, filter)
	if err != nil {
		return err
	}
	defer cursor.Close(ctx)

	err = cursor.All(ctx, records)
	return err
}

// GetSingleRecordMatchingKeyValue ...
func (c *Client) GetSingleRecord(collectionName CollectionName, filter map[string]interface{}, result interface{}) (found bool, err error) {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return false, errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	// find record in collection
	filterBson := bson.D{}
	for k, v := range filter {
		filterBson = append(filterBson, bson.E{Key: k, Value: v})
	}

	err = collection.FindOne(ctx, filterBson).Decode(result)

	found = true
	if err == mongo.ErrNoDocuments {
		err = nil
		found = false
	}

	return found, err
}

// GetRecordsMatchingKeyValue ...
func (c *Client) GetRecordsMatchingKeyValue(collectionName CollectionName, key string, value interface{}, records interface{}) error {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filter := bson.D{{Key: key, Value: value}}
	cursor, err := collection.Find(ctx, filter)
	if err != nil {
		return err
	}
	defer cursor.Close(ctx)

	err = cursor.All(ctx, records)
	return err
}

// GetRecordsMatchingFilter ...
func (c *Client) GetRecordsMatchingFilter(collectionName CollectionName, filter map[string]interface{}, findOptions *FindOptions, records interface{}) error {

	// fmt.Printf("[GetRecordsMatchingFilter] %#v \n", filter)

	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	var fo *options.FindOptions = nil
	if findOptions != nil {
		fo = options.Find()
		if findOptions.Limit != nil {
			fo.SetLimit(*findOptions.Limit)
		}
		if findOptions.Sort != nil {
			fo.SetSort(findOptions.Sort)
		}
	}

	filterBson := bson.D{}
	for k, v := range filter {
		// if value is a regex, convert to mongo regex
		if regex, ok := v.(Regex); ok == true {
			filterBson = append(filterBson, bson.E{
				Key:   k,
				Value: bsonx.Regex(regex.Pattern, regex.Options),
			})
		} else if mapStrTime, ok := v.(map[string]time.Time); ok == true {
			// construct a bson.D from mapStrTime
			constraints := bson.D{}
			for key, value := range mapStrTime {
				constraints = append(constraints, bson.E{Key: key, Value: value})
			}
			filterBson = append(filterBson, bson.E{
				Key:   k,
				Value: constraints,
			})
		} else {
			filterBson = append(filterBson, bson.E{
				Key:   k,
				Value: v,
			})
		}
	}

	cursor, err := collection.Find(ctx, filterBson, fo)
	if err != nil {
		return err
	}
	defer cursor.Close(ctx)

	err = cursor.All(ctx, records)
	return err
}

func (c *Client) CountRecords(collectionName CollectionName) (int64, error) {
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return 0, errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filter := bson.D{} // empty filter means all documents will be counted
	count, err := collection.CountDocuments(ctx, filter)
	return count, err
}

func (c *Client) CountRecordsMatchingFilter(collectionName CollectionName, filter map[string]interface{}) (int64, error) {
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return 0, errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filterBson := bson.D{}
	for k, v := range filter {
		filterBson = append(filterBson, bson.E{Key: k, Value: v})
	}

	return collection.CountDocuments(ctx, filterBson)
}

// GetItemsByName returns the Items having the provided name, not considering their repo
func (c *Client) CountRecordsMatchingKeyValue(collectionName CollectionName, key string, value interface{}) (int64, error) {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return 0, errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	filter := bson.D{{Key: key, Value: value}}
	count, err := collection.CountDocuments(ctx, filter)
	return count, err
}

type UpdateRecordOpts struct {
	RefreshUpdatedAt bool
	RefreshLastSeen  bool
}

func (c *Client) UpdateRecord(collectionName CollectionName, filter map[string]interface{}, updateData map[string]interface{}) (err error) {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		err = errors.New("collection not found")
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	// data to update
	fields := bson.D{}
	for k, v := range updateData {
		fields = append(fields, bson.E{Key: k, Value: v})
	}

	update := bson.D{{Key: "$set", Value: fields}}

	_ /* UpdateResult */, err = collection.UpdateOne(ctx, filter, update)
	// fmt.Printf("Matched %v documents and updated %v documents.\n", res.MatchedCount, res.ModifiedCount)

	return
}

// DeleteSingleRecordMatchingFilter deletes a single record matching all the K/V in the filter
func (c *Client) DeleteSingleRecordMatchingFilter(collectionName CollectionName, filter map[string]interface{}) (found bool, err error) {
	// retrieve collection
	collection := c.getCollectionByName(collectionName)
	if collection == nil {
		return false, errors.New("collection not found")
	}

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	res, err := collection.DeleteOne(ctx, filter, nil)
	return res.DeletedCount > 0, err
}

// New creates a new Client object
func New(databaseURI string, username string, password string) (*Client, error) {

	ctx, cancel := context.WithTimeout(context.Background(), connectTimeout)
	defer cancel()

	credential := options.Credential{
		Username: username,
		Password: password,
	}

	clientOpts := options.Client().ApplyURI(databaseURI).SetAuth(credential)
	client, err := mongo.Connect(ctx, clientOpts)
	if err != nil {
		return nil, err
	}

	var database *mongo.Database = client.Database("particubes")

	result := &Client{
		MongoClient:           client,
		cubzhDatabase:         database,
		gameCollection:        database.Collection("games"),
		gameLikeCollection:    database.Collection("gameLikes"),
		gameCommentCollection: database.Collection("gameComments"),
		itemCollection:        database.Collection("items"),
		itemLikeCollection:    database.Collection("itemLikes"),
		itemCommentCollection: database.Collection("itemComments"),
		usersCollection:       database.Collection("users"),
	}

	return result, nil
}

func SharedUserDB() (*Client, error) {
	if sharedDBClient == nil {
		client, err := New("mongodb://user-db:27017", "root", "root")
		if err != nil || client == nil {
			return client, err
		}
		sharedDBClient = client
	}
	return sharedDBClient, nil
}

// EnsureItemIndexes ...
// collection *mongo.Collection, likes *mongo.Collection, comments *mongo.Collection
func (c *Client) EnsureItemIndexes() error {
	ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)

	indexView := c.itemCollection.Indexes()
	cursor, err := indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	ctx = context.Background()
	for cursor.Next(ctx) {
		elem := &bson.D{}
		if err := cursor.Decode(elem); err != nil {
			log.Fatal(err)
		}
		fmt.Println("INDEX:", elem)
	}
	cursor.Close(ctx)

	err = setUniqueIndex(indexView, "id")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "repo")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "name")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "public")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "author")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "featured")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "category")
	if err != nil {
		return err
	}

	// commented because not used in search for now
	// err = setTextIndexes(indexView, "name", "description")
	// if err != nil {
	// 	return err
	// }

	// ITEM LIKES

	indexView = c.itemLikeCollection.Indexes()
	cursor, err = indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	err = setUniqueCompoundIndex(indexView, "user", "item")
	if err != nil {
		return err
	}

	// ITEM COMMENTS

	indexView = c.itemCommentCollection.Indexes()
	cursor, err = indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	err = setUniqueCompoundIndex(indexView, "user", "item")
	if err != nil {
		return err
	}

	return nil
}

// EnsureGameIndexes ...
func (c *Client) EnsureGameIndexes() error {

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)

	indexView := c.gameCollection.Indexes()
	cursor, err := indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	ctx = context.Background()
	for cursor.Next(ctx) {
		elem := &bson.D{}
		if err := cursor.Decode(elem); err != nil {
			log.Fatal(err)
		}
		fmt.Println("INDEX:", elem)
	}
	cursor.Close(ctx)

	err = setUniqueIndex(indexView, "id")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "author")
	if err != nil {
		return err
	}

	err = setTextIndexes(indexView, "title", "description")
	if err != nil {
		return err
	}

	err = setIndex(indexView, "featured")
	if err != nil {
		return err
	}

	// LIKES

	indexView = c.gameLikeCollection.Indexes()
	cursor, err = indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	err = setUniqueCompoundIndex(indexView, "user", "game")
	if err != nil {
		return err
	}

	// COMMENTS

	indexView = c.gameCommentCollection.Indexes()
	cursor, err = indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	err = setUniqueCompoundIndex(indexView, "user", "game")
	if err != nil {
		return err
	}

	return nil
}

// EnsureUserIndexes ...
func (c *Client) EnsureUserIndexes() error {
	ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)

	indexView := c.usersCollection.Indexes()
	cursor, err := indexView.List(ctx)
	if err != nil {
		cancel()
		return err
	}
	cancel()

	ctx = context.Background()
	for cursor.Next(ctx) {
		elem := &bson.D{}
		if err := cursor.Decode(elem); err != nil {
			log.Fatal(err)
		}
		fmt.Println("INDEX:", elem)
	}
	cursor.Close(ctx)

	err = setUniqueIndex(indexView, "id")
	if err != nil {
		return err
	}

	// drop existing username index if one already exists
	indexName := "username"
	err, exists, matching := indexExists(indexView, indexName)
	if err == nil && exists == true {
		ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)
		_, err := indexView.DropOne(ctx, matching, nil)
		if err != nil {
			fmt.Println("failed to remove index:", matching)
		}
		cancel()
	}

	err = setIndexThatCanBeNull(indexView, "tmp")
	if err != nil {
		return err
	}

	err = setIndexThatCanBeNull(indexView, "anon")
	if err != nil {
		return err
	}

	err = setUniqueOrNullIndex(indexView, "username")
	if err != nil {
		return err
	}

	err = setUniqueOrNullIndex(indexView, "number")
	if err != nil {
		return err
	}

	err = setUniqueOrNullIndex(indexView, "email")
	if err != nil {
		return err
	}

	err = setUniqueOrNullIndex(indexView, "phone")
	if err != nil {
		return err
	}

	err = setUniqueOrNullIndex(indexView, "email-confirm-hash")
	if err != nil {
		return err
	}

	return nil
}

// GetSingleRecordMatchingKeyValue ...
// TODO remove this ! (and use GetSingleRecord instead)
func (c *Client) GetSingleRecordMatchingKeyValue(collectionName CollectionName, key string, value interface{}, result interface{}) (found bool, err error) {
	filter := make(map[string]interface{})
	filter[key] = value
	return c.GetSingleRecord(collectionName, filter, result)
}

// --------------------------------------------------
// Unexposed functions
// --------------------------------------------------

func (c *Client) getCollectionByName(name CollectionName) *mongo.Collection {
	switch name {
	case GameCollectionName:
		return c.gameCollection
	case GameLikeCollectionName:
		return c.gameLikeCollection
	case GameCommentCollectionName:
		return c.gameCommentCollection
	case ItemCollectionName:
		return c.itemCollection
	case ItemLikeCollectionName:
		return c.itemLikeCollection
	case ItemCommentCollectionName:
		return c.itemCommentCollection
	case UserCollectionName:
		return c.usersCollection
	}
	return nil
}

// Returns whether an index with the provided name exists in the IndexView.
// Returns
// - error
// - found or not
// - matching indexName
func indexExists(indexView mongo.IndexView, indexName string) (error, bool, string) {
	var result = false
	var matchingIndexName = ""

	// Specify the MaxTime option to limit the amount of time the operation can run on the server
	opts := options.ListIndexes().SetMaxTime(10 * time.Second)
	cursor, err := indexView.List(context.TODO(), opts)
	if err != nil {
		return err, false, ""
	}

	// Get a slice of all indexes returned and print them out.
	var results []bson.M
	if err = cursor.All(context.TODO(), &results); err != nil {
		return err, false, ""
	}

	for _, element := range results {
		existingIndexName, ok := element["name"].(string)
		if ok {
			match, err := regexp.MatchString(indexName+"_[0-9]+", existingIndexName)
			if err == nil && match == true {
				result = true
				matchingIndexName = existingIndexName
				break
			}
		}
	}

	return nil, result, matchingIndexName
}
func setUniqueIndex(indexView mongo.IndexView, key string) error {
	ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)
	defer cancel()

	keys := bsonx.Doc{{Key: key, Value: bsonx.Int32(1)}}
	indexModel := mongo.IndexModel{Keys: keys}
	unique := true
	indexModel.Options = &options.IndexOptions{Unique: &unique}

	_, err := indexView.CreateOne(ctx, indexModel)
	return err
}

func setIndexThatCanBeNull(indexView mongo.IndexView, key string) error {
	ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)
	defer cancel()

	keys := bsonx.Doc{{Key: key, Value: bsonx.Int32(1)}}
	indexModel := mongo.IndexModel{Keys: keys}
	unique := false
	sparse := true
	indexModel.Options = &options.IndexOptions{Unique: &unique, Sparse: &sparse}
	_, err := indexView.CreateOne(ctx, indexModel)
	return err
}

func setUniqueOrNullIndex(indexView mongo.IndexView, key string) error {
	ctx, cancel := context.WithTimeout(context.Background(), indexesConstructionTimeout)
	defer cancel()

	keys := bsonx.Doc{{Key: key, Value: bsonx.Int32(1)}}
	indexModel := mongo.IndexModel{Keys: keys}
	unique := true
	sparse := true
	indexModel.Options = &options.IndexOptions{Unique: &unique, Sparse: &sparse}

	_, err := indexView.CreateOne(ctx, indexModel)
	return err
}

func setIndex(indexView mongo.IndexView, key string) error {
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	keys := bsonx.Doc{{Key: key, Value: bsonx.Int32(1)}}
	indexModel := mongo.IndexModel{Keys: keys}

	_, err := indexView.CreateOne(ctx, indexModel)
	return err
}

func setUniqueCompoundIndex(indexView mongo.IndexView, indexes ...string) error {
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	keys := bsonx.Doc{}

	for _, index := range indexes {
		keys = append(keys.Append(index, bsonx.Int32(1)))
	}

	indexModel := mongo.IndexModel{Keys: keys}
	unique := true
	indexModel.Options = &options.IndexOptions{Unique: &unique}

	_, err := indexView.CreateOne(ctx, indexModel)
	return err
}

func setTextIndexes(indexView mongo.IndexView, keys ...string) error {
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	_keys := bsonx.Doc{}

	for _, key := range keys {
		_keys = append(_keys, bsonx.Elem{Key: key, Value: bsonx.String("text")})
	}

	indexModel := mongo.IndexModel{Keys: _keys}

	_, err := indexView.CreateOne(ctx, indexModel)
	if err != nil {
		return err
	}

	return nil
}

// func setTextIndexes(indexView mongo.IndexView, keys ...string) error {
// 	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
// 	defer cancel()

// 	_keys := bsonx.Doc{}

// 	for _, key := range keys {
// 		_keys = append(_keys, bsonx.Elem{Key: key, Value: bsonx.String("text")})
// 	}

// 	indexModel := mongo.IndexModel{Keys: _keys}

// 	_, err := indexView.CreateOne(ctx, indexModel)
// 	if err != nil {
// 		return err
// 	}

// 	return nil
// }
