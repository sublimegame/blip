package main

import (
	"fmt"

	"cu.bzh/hub/search"
)

var BLIP_FEATURED_WORLD_IDS_BY_CATEGORY map[string][]string = map[string][]string{
	"featured": {
		"3155d42b-3643-4b38-9d36-b64baad5fb25", // Drive 🔥
		"46b233c7-e005-48e5-9430-df7fb1cbc6a3", // Merge Castle
		"aeca0f04-141c-422b-b94b-b283321df1bb", // subway surfers
		"bb8b34e2-0b71-4f6f-b604-ccaae867d176", // Breakdown
		"6f49b11c-13d8-4f10-91df-6ad551951638", // Cat vs Dog
		// "4222b123-3dbb-4487-a67d-687a3ff72523", // Imagine (broken)
		"8e36d63b-8e93-4d52-ba2a-c4b0677202b2", // Flying unicorn
		"8ee53160-c2b4-4eeb-8663-a20c13af1f20", // hub v3
		"13693497-03fd-4492-9b36-9776bb11d958", // Eat Your Fruits
		"c02a3c8e-f54d-4de7-97d4-37c1cd64aca1", // dustzh
		// "4d194d06-82e2-45b3-a759-557d4b486f11", // Farming Game (broken)
		// "35ab8bed-afa2-4e19-aeb8-4a2c799351e0", // Idle Gacha (broken)
		"b850b8d3-9335-41a1-b83e-0abc98a990ce", // Just go down (TODO: fix UI + add leaderboards)
		"d42c804f-a7f0-4705-9f8f-91b5264dedcd", // Colorunzh (TODO: force landscape)
		// "eea0164d-adb6-45be-8c25-4e5529dd1cc3", // Doblins Gungeons (broken)
		"63e63b3c-05c1-493b-8715-23a0842c1175", // Retro Motor Club
		// "f9b5dcbe-c0c8-4ba5-9828-31e4216de55a", // Convy (broken)
		// "73a57884-058d-49bb-9a32-41eeff82212c", // 2048
		"bc9c4f68-b280-43c5-ac6d-8a125fdc6667", // Escape the Roots Temple
		"7df03a0f-0e9b-4e40-9fb2-751ced616931", // Cubzh Headquarters
		"704b2492-545c-46dc-992d-1902115a98c8", // Wash It
		"37e4f1cb-7b81-4145-9322-efb1cd2b06bb", // Pickaxes
		"4d356840-0e1e-4296-a6fa-af4fd0e95e22", // Sakura Tree Obby
		"4a3b52b9-a4cc-400c-927f-7d48398877f7", // Minesweepunch
		"122b9c23-45fb-46ee-bb77-240995749dc4", // Lumberjack
		"250c0912-ef4f-4e32-8f2e-587116cdde42", // Chopper Simulation
		"9d4d5106-4a45-4cb1-b8c0-737365f82654", // Hovercraft Simulator
		"0e80c333-187d-403e-9d30-7c9a70382e67", // Ice Bumper
		"502cf818-0438-44a7-a397-16d34d9c3c6c", // Cookie Clicker
		"ea0d4fa6-1034-42ae-aefb-e471cba0f656", // Kraken Obby
		"1470ea4e-ffca-4e2d-901e-e0a1c12fb96b", // Trader Simulator
		"62aa3fcc-6263-4fb6-ba43-a00a3a8dc80d", // Ridiculous Chess
		"947afa3f-85af-44b2-8f9a-e34c85a71e9e", // New year's fireworks
		"8d4dcf4c-73cc-4027-b31e-7af6f4ae18f2", // Building Challenge Furniture
		"687705c1-8233-47c1-9188-c39f55393377", // Space Dustzh
		"3ddb4a98-2fc7-4658-995a-26616363bf2d", // Quakzh
	},
	"fun_with_friends": {
		// "0178802d-bbc8-4085-9a5c-12351a98d1b0", // Fortcubes (broken)
		"b3cdb209-297b-405f-aeeb-5e1a73b0f6d5", // Croco 👥
		"13693497-03fd-4492-9b36-9776bb11d958", // Eat Your Fruits
		"c3a990ee-ef88-42b7-93f4-f6adaf1b5cff", // light saberzh
		"c02a3c8e-f54d-4de7-97d4-37c1cd64aca1", // dustzh
		"d42c804f-a7f0-4705-9f8f-91b5264dedcd", // Colorunzh
		"63e63b3c-05c1-493b-8715-23a0842c1175", // Retro Motor Club
		"7df03a0f-0e9b-4e40-9fb2-751ced616931", // Cubzh Headquarters
		"687705c1-8233-47c1-9188-c39f55393377", // Space Dustzh
		"3ddb4a98-2fc7-4658-995a-26616363bf2d", // Quakzh
		// "4222b123-3dbb-4487-a67d-687a3ff72523", // Imagine
	},
	"solo": {
		"01fe424a-1dc9-45e1-b71d-190dced7ec36", // Zombies 🔫
		"3b18aa59-373b-47fd-8f58-8d12efd5db89", // Building 🛠️
		"1062409a-c5a8-4421-ad18-cf14b77e9b2c", // Space ⭐
		// "95744f37-e378-4f4b-8b6e-3fbea392492d", // Craft Island
		// "4d194d06-82e2-45b3-a759-557d4b486f11", // Farming Game (broken)
		// "73a57884-058d-49bb-9a32-41eeff82212c", // 2048
		"46b233c7-e005-48e5-9430-df7fb1cbc6a3", // Merge Castle
		"b850b8d3-9335-41a1-b83e-0abc98a990ce", // Just go down
		// "35ab8bed-afa2-4e19-aeb8-4a2c799351e0", // Idle Gacha (broken)
		"8e36d63b-8e93-4d52-ba2a-c4b0677202b2", // Flying unicorn
		"704b2492-545c-46dc-992d-1902115a98c8", // Wash It
		"4a3b52b9-a4cc-400c-927f-7d48398877f7", // Minesweepunch
		"1470ea4e-ffca-4e2d-901e-e0a1c12fb96b", // Trader Simulator
	},
}

// Return values:
// - bool: true if the category is found, false otherwise
// - []string: the list of world IDs for the category
func getFeaturedWorldsForCategory(category string, searchClient *search.Client) (found bool, creations []search.Creation) {
	found = false
	creations = make([]search.Creation, 0)

	if category == "" {
		return
	}

	var worldIDs []string
	worldIDs, found = BLIP_FEATURED_WORLD_IDS_BY_CATEGORY[category]
	if !found {
		return
	}

	// for each world ID, get the world info from search engine
	for _, worldID := range worldIDs {
		creation, err := searchClient.GetCreationByID(worldID)
		if err != nil {
			fmt.Println("❌ ERROR:", err.Error())
			continue
		}

		if creation.Type != search.CreationTypeWorld {
			continue
		}

		creations = append(creations, creation)
	}

	return
}
