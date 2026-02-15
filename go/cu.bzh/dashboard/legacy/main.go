package main

import (
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/user"
)

const (
	serverCertFile = "/certs/server-cert-and-ca.pem"
	serverKeyFile  = "/certs/server-key.pem"
	SESSION_SECRET = "ef967a9f96612e50bd9beb97d5190f79"
)

func main() {
	fmt.Println("✨ starting Cubzh dashboard...")

	r := chi.NewRouter()
	r.Use(middleware.Logger)

	r.Get("/", index)
	r.Get("/user/{username}", userInfo)
	r.Get("/month/{year}/{month}", month)

	// r.Get("/user/lastweek", userLastWeek)
	// r.Get("/game/lastweek", gameLastWeek)
	// r.Get("/item/lastweek", itemLastWeek)

	http.ListenAndServe(":80", r)
}

func index(w http.ResponseWriter, r *http.Request) {
	respond(http.StatusOK, "It works! 🙂", w)
}

func userInfo(w http.ResponseWriter, r *http.Request) {

	name := chi.URLParam(r, "username")

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respond(http.StatusInternalServerError, err.Error(), w)
		return
	}

	usr, err := user.GetByUsername(dbClient, name)
	if err != nil {
		respond(http.StatusInternalServerError, err.Error(), w)
		return
	}
	if usr == nil { // user not found
		respond(http.StatusBadRequest, "user not found", w)
		return
	}

	txt := "FOUND!"
	txt += "\nusername: " + usr.Username
	txt += "\nID: " + usr.ID
	txt += "\nemail: " + usr.Email
	txt += "\ncreated: " + usr.Created.Format(time.DateOnly) + "(YYYY-MM-DD)"
	txt += "\nupdated: " + usr.Updated.Format(time.DateOnly) + "(YYYY-MM-DD)"
	txt += "\nlast seen: " + usr.LastSeen.Format(time.DateOnly) + "(YYYY-MM-DD)"
	txt += "\nmagic key: " + usr.MagicKey

	respond(http.StatusOK, txt, w)
}

func month(w http.ResponseWriter, r *http.Request) {
	var err error

	yearStr := chi.URLParam(r, "year")
	monthStr := chi.URLParam(r, "month")

	if len(yearStr) == 0 || len(monthStr) == 0 {
		respond(http.StatusBadRequest, "year and month must be provided", w)
		return
	}

	// convert year and month strings to integer
	startMonth, err := strconv.ParseInt(monthStr, 10, 32)
	if err != nil {
		respond(http.StatusBadRequest, err.Error(), w)
		return
	}
	startYear, err := strconv.ParseInt(yearStr, 10, 32)
	if err != nil {
		respond(http.StatusBadRequest, err.Error(), w)
		return
	}

	endMonthInt := startMonth + 1
	endYearInt := startYear

	if endMonthInt > 12 {
		endMonthInt -= 12
		endYearInt += 1
	}

	start := time.Date(int(startYear), time.Month(startMonth), 1, 0, 0, 0, 0, time.UTC)
	end := time.Date(int(endYearInt), time.Month(endMonthInt), 1, 0, 0, 0, 0, time.UTC)

	str, err := getInfoByMonth(start, end, time.Month(startMonth).String())
	if err != nil {
		respond(http.StatusInternalServerError, err.Error(), w)
		return
	}

	respond(http.StatusOK, str, w)
}

type playerWithCreations struct {
	player *user.User
	worlds []*game.Game
	items  []*item.Item
}

func getAllPlayersWithCreations() ([]playerWithCreations, error) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return nil, err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return nil, err
	}

	if users == nil || len(users) == 0 { // no user in the database
		return nil, errors.New("no user in the database")
	}

	var arr []playerWithCreations

	for _, usr := range users {
		obj := playerWithCreations{}
		obj.player = usr
		i, err := item.GetByRepo(dbClient, usr.Username)
		if err != nil {
			return nil, err
		}
		obj.items = i
		g, err := game.GetByAuthor(dbClient, usr.ID)
		if err != nil {
			return nil, err
		}
		obj.worlds = g
		arr = append(arr, obj)
	}

	return arr, nil // success
}

func getInfoByMonth(start time.Time, end time.Time, month string) (string, error) {

	// --- This month counts ---

	//
	usersCreatedThisMonth := 0

	//
	worldsCreatedThisMonth := 0

	//
	itemsCreatedThisMonth := 0

	//
	usersHavingBecomeCreatorsThisMonth := 0
	usersHavingBecomeWorldCreatorsThisMonth := 0
	usersHavingBecomeItemCreatorsThisMonth := 0

	// --- Overall counts ---

	//
	usersOverallCount := 0

	//
	creatorsOverallCount := 0
	worldcreatorsOverallCount := 0
	itemcreatorsOverallCount := 0

	//
	worldsOverallCount := 0

	//
	worldsOverallCountNoDefaultScript := 0

	//
	itemsOverallCount := 0

	txt := "MONTH < " + month + " " + strconv.Itoa(start.Year()) + " >"

	arr, err := getAllPlayersWithCreations()
	if err != nil {
		return "", err
	}

	// Review each UserWithCreations
	for _, pwc := range arr {

		if pwc.player.Created.After(start) && pwc.player.Created.Before(end) {
			usersCreatedThisMonth += 1
		}

		if pwc.player.Created.Before(end) {
			usersOverallCount += 1
		}

		// Review user's worlds
		// --------------------------------------------------
		worldsCreatedByUserBeforeThisMonth := 0
		worldsCreatedByUserThisMonth := 0
		for _, world := range pwc.worlds {

			if world.Created.Before(start) {
				worldsCreatedByUserBeforeThisMonth += 1
			} else if world.Created.Before(end) {
				worldsCreatedByUserThisMonth += 1
			}

			if world.Created.Before(end) {
				if !world.DefaultScript {
					worldsOverallCountNoDefaultScript += 1
				}
			}
		}
		worldsCreatedByUser := worldsCreatedByUserBeforeThisMonth + worldsCreatedByUserThisMonth
		worldsOverallCount += worldsCreatedByUser
		worldsCreatedThisMonth += worldsCreatedByUserThisMonth
		userIsWorldCreator := worldsCreatedByUser > 0
		userBecameWorldCreatorThisMonth := userIsWorldCreator && worldsCreatedByUserBeforeThisMonth == 0

		// Review user's items
		// --------------------------------------------------
		itemsCreatedByUserBeforeThisMonth := 0
		itemsCreatedByUserThisMonth := 0
		for _, itm := range pwc.items {

			if itm.Created.Before(start) {
				itemsCreatedByUserBeforeThisMonth += 1
			} else if itm.Created.Before(end) {
				itemsCreatedByUserThisMonth += 1
			}
		}
		itemsCreatedByUser := itemsCreatedByUserBeforeThisMonth + itemsCreatedByUserThisMonth
		itemsOverallCount += itemsCreatedByUser
		itemsCreatedThisMonth += itemsCreatedByUserThisMonth
		userIsItemCreator := itemsCreatedByUser > 0
		userBecameItemCreatorThisMonth := userIsItemCreator && itemsCreatedByUserBeforeThisMonth == 0

		// Computing for user
		// --------------------------------------------------
		userIsCreator := userIsWorldCreator || userIsItemCreator
		if userIsCreator {
			creatorsOverallCount += 1
		}

		if userIsWorldCreator {
			worldcreatorsOverallCount += 1
		}

		if userIsItemCreator {
			itemcreatorsOverallCount += 1
		}

		userBecameCreatorThisMonth := userIsCreator && worldsCreatedByUserBeforeThisMonth == 0 && itemsCreatedByUserBeforeThisMonth == 0
		if userBecameCreatorThisMonth {
			usersHavingBecomeCreatorsThisMonth += 1
		}

		if userBecameWorldCreatorThisMonth {
			usersHavingBecomeWorldCreatorsThisMonth += 1
		}

		if userBecameItemCreatorThisMonth {
			usersHavingBecomeItemCreatorsThisMonth += 1
		}
	}

	// Users
	txt += "\n\nUsers (overall)                      : " + strconv.Itoa(usersOverallCount)
	txt += "\n\nUsers (new this month)               : " + strconv.Itoa(usersCreatedThisMonth)

	// Creators (users having created at least 1 item or 1 world in their lifetime)
	txt += "\n\nCreators (overall)                   : " + strconv.Itoa(creatorsOverallCount)
	txt += "\n\nCreators (new this month)            : " + strconv.Itoa(usersHavingBecomeCreatorsThisMonth)

	// WorldCreators (users having created at least 1 world in their lifetime)
	txt += "\n\nWorldCreators (overall)              : " + strconv.Itoa(worldcreatorsOverallCount)

	// ItemCreators (users having created at least 1 item in their lifetime)
	txt += "\n\nItemCreators (overall)               : " + strconv.Itoa(itemcreatorsOverallCount)
	txt += "\n\nItemCreators (new this month)        : " + strconv.Itoa(usersHavingBecomeItemCreatorsThisMonth)

	// Worlds
	txt += "\n\nWorlds (overall)                     : " + strconv.Itoa(worldsOverallCount)
	txt += "\n\nWorlds (overall, non-default script) : " + strconv.Itoa(worldsOverallCountNoDefaultScript)
	txt += "\n\nWorlds (new this month)              : " + strconv.Itoa(worldsCreatedThisMonth)

	// Items
	txt += "\n\nItems (overall)                      : " + strconv.Itoa(itemsOverallCount)
	txt += "\n\nItems (new this month)               : " + strconv.Itoa(itemsCreatedThisMonth)

	return txt, nil
}

// func userLastWeek(c *gin.Context) {

// 	dbClient, err := dbclient.SharedUserDB()
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	u, err := user.GetAll(dbClient)
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	if u == nil { // no user in the database
// 		c.String(http.StatusBadRequest, "no user in the database")
// 		return
// 	}
// 	count := 0
// 	countAnonym := 0

// 	var arr []playerWithGames
// 	for _, s := range u {
// 		elapsed := time.Since(*s.Created)
// 		hours := elapsed.Hours()
// 		if hours < 2600 {
// 			countAnonym += 1
// 			if s.Username != "" {
// 				count += 1
// 				obj := playerWithGames{}
// 				obj.player = s
// 				i, err := item.GetByRepo(dbClient, s.Username)
// 				if err != nil {
// 					c.String(http.StatusInternalServerError, err.Error())
// 					return
// 				}
// 				obj.items = i
// 				g, err := game.GetByAuthor(dbClient, s.ID)
// 				if err != nil {
// 					c.String(http.StatusInternalServerError, err.Error())
// 					return
// 				}
// 				obj.games = g
// 				arr = append(arr, obj)
// 			}
// 		}
// 	}

// 	txt := "This is the number of new players this week : " + strconv.Itoa(countAnonym)
// 	txt += "\n\nThis is the number of new players with a name this week : " + strconv.Itoa(count)
// 	txt += "\n\nThis is the list of player who joined last week : "

// 	for _, s := range arr {
// 		txt += "\n- " + s.player.Username + " -> Number of games created : " + strconv.Itoa(len(s.games)) + " and Number of items created : " + strconv.Itoa(len(s.items))
// 	}

// 	c.String(http.StatusOK, txt)
// }

// func gameLastWeek(c *gin.Context) {

// 	dbClient, err := dbclient.SharedUserDB()
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	g, err := game.GetAll(dbClient)
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	if g == nil { // no user in the database
// 		c.String(http.StatusBadRequest, "no game in the database")
// 		return
// 	}

// 	txt := "\nList of games created last week :"
// 	if len(g) == 0 {
// 		txt += "\nSorry there is no new games since the last week ;_;"
// 	}
// 	for _, s := range g {
// 		elapsed := time.Since(s.Created)
// 		hours := elapsed.Hours()
// 		if hours < 2600 { // 168
// 			txt += "\n- " + s.Title
// 		}
// 	}

// 	c.String(http.StatusOK, txt)
// }

// func itemLastWeek(c *gin.Context) {

// 	dbClient, err := dbclient.SharedUserDB()
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	i, err := item.GetAll(dbClient)
// 	if err != nil {
// 		c.String(http.StatusInternalServerError, err.Error())
// 		return
// 	}

// 	if i == nil { // no user in the database
// 		c.String(http.StatusBadRequest, "no items in the database")
// 		return
// 	}

// 	txt := "\nList of items created last week :"
// 	if len(i) == 0 {
// 		txt += "\nSorry there is no new items since the last week ;_;"
// 	}
// 	for _, s := range i {
// 		elapsed := time.Since(s.Created)
// 		hours := elapsed.Hours()
// 		if hours < 2600 { // 168
// 			txt += "\n- " + s.Repo
// 		}
// 	}

// 	c.String(http.StatusOK, txt)
// }

// // func addTemplatesDir(r *gin.Engine, templateDir string) {
// // 	tmplFiles := make([]string, 0)
// // 	files, err := ioutil.ReadDir(templateDir)
// // 	if err != nil {
// // 		return
// // 	}
// // 	for _, file := range files {
// // 		tmplFiles = append(tmplFiles, filepath.Join(templateDir, file.Name()))
// // 	}
// // 	r.LoadHTMLFiles(tmplFiles...)
// // }

// // func renderError(c *gin.Context, title string, msg string, statusCode int) {

// // 	htmlParams := gin.H{
// // 		"headTitle":   "Cubzh - " + title,
// // 		"header":      title,
// // 		"error":       msg,
// // 		"status_code": statusCode,
// // 	}

// // 	c.HTML(statusCode, "error.tmpl", htmlParams)
// // }

// func toString(i interface{}) (string, error) {
// 	if str, ok := i.(string); ok {
// 		return str, nil
// 	}
// 	return "", errors.New("interface is not a string")
// }

// func toBool(i interface{}) (bool, error) {
// 	if b, ok := i.(bool); ok {
// 		return b, nil
// 	}
// 	return false, errors.New("interface is not a bool")
// }

// 	parts := strings.Split(addr, ":")

// 	if len(parts) > 1 {
// 		addr = parts[0]
// 	}

// 	return addr
// }
