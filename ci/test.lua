---@diagnostic disable: undefined-global, undefined-field, lowercase-global

--
Config = {
	Items = { "pen", "aduermael.sword" },
	Map = "black_cube_10x10",
	foo = "bar",
}

-- should return an error string (on failure) or nil (on success)
Client.test_main = function()
	local err = nil

	-----------
	-- Lua Types
	-----------
	if Type == nil then
		return "Type == nil"
	end
	if Type["nil"] ~= "nil" then
		return 'Type.nil ~= "nil"'
	end
	if "boolean" ~= "boolean" then
		return '"boolean" ~= "boolean"'
	end
	if Type.userdata ~= "userdata" then
		return 'Type.userdata ~= "userdata"'
	end
	if Type.number ~= "number" then
		return 'Type.number ~= "number"'
	end
	if Type.string ~= "string" then
		return 'Type.string ~= "string"'
	end
	if Type.table ~= "table" then
		return 'Type.table ~= "table"'
	end
	if "function" ~= "function" then
		return 'Type.function ~= "function"'
	end
	if Type.thread ~= "thread" then
		return 'Type.thread ~= "thread"'
	end

	-----------
	-- Cubzh Types
	-----------
	-- if Type.AudioListener ~= 'AudioListener" then return ""AudioListener ~= \"AudioListener\""' end
	-- if Type.AudioSource ~= 'AudioSource" then return ""AudioSource ~= \"AudioSource\""' end
	if Type.Map ~= "Map" then
		return 'Type.Map ~= "Map"'
	end
	if Type.Camera ~= "Camera" then
		return 'Type.Camera ~= "Camera"'
	end
	if Type.Player ~= "Player" then
		return 'Type.Player ~= "Player"'
	end
	if "Number3" ~= "Number3" then
		return '"Number3" ~= "Number3"'
	end
	if Type.Rotation ~= "Rotation" then
		return 'Type.Rotation ~= "Rotation"'
	end
	if Type.Shape ~= "Shape" then
		return 'Type.Shape ~= "Shape"'
	end
	if Type.Item ~= "Item" then
		return 'Type.Item ~= "Item"'
	end
	if Type.Items ~= "Items" then
		return 'Type.Items ~= "Items"'
	end
	if Type.Config ~= "Config" then
		return 'Type.Config ~= "Config"'
	end
	if Type.Palette ~= "Palette" then
		return 'Type.Palette ~= "Palette"'
	end
	if Type.Face ~= "Face" then
		return 'Type.Face ~= "Face"'
	end
	if Type.Ray ~= "Ray" then
		return 'Type.Ray ~= "Ray"'
	end
	if Type.Box ~= "Box" then
		return 'Type.Box ~= "Box"'
	end
	if Type.Event ~= "Event" then
		return 'Type.Event ~= "Event"'
	end
	if Type.Timer ~= "Timer" then
		return 'Type.Timer ~= "Timer"'
	end
	if Type.Data ~= "Data" then
		return 'Type.Data ~= "Data"'
	end

	if typeof(10) ~= "number" then
		return 'typeof(10) ~= "number"'
	end
	if typeof(10.0) ~= "number" then
		return 'typeof(10.0) ~= "number"'
	end

	for i = 1, 2 do
		if typeof(i) ~= "number" then
			return 'type of iterator ~= "number"'
		end
	end

	if 1 ~= 1.0 then
		return "1 ~= 1.0"
	end

	print("✅ Types")

	-----------
	-- AudioListener
	-----------

	--  err = Client.audioListenerTestSuite()
	-- if err ~= nil then return err end

	-- print("✅ AudioListener")

	-----------
	-- AudioSource
	-----------

	--  err = Client.audioSourceTestSuite()
	-- if err ~= nil then return err end

	-- print("✅ AudioSource")

	-----------
	-- Block
	-----------

	err = Client.blockTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Block")

	-----------
	-- Face
	-----------

	err = Client.faceTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Face")

	-----------
	-- BlockProperties
	-----------

	-- cannot work: block.Properties == nil
	-- local blockProperties = Client.blockPropertiesTestSuite()
	-- if blockProperties ~= nil then return blockProperties end

	-- print("✅ BlockProperties")

	----------------------
	-- Box
	----------------------

	err = Client.boxTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Box")

	----------------------
	-- Camera
	----------------------

	err = Client.cameraTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Camera")

	-----------
	-- Color
	-----------

	err = Client.colorTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Color")

	-----------
	-- Config
	-----------

	err = Client.configTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Config")

	-----------
	-- Client
	-----------

	err = Client.clientTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Client")

	-----------
	-- Server / Shared
	-----------
	if Local ~= nil then
		return "Local should be nil now"
	end
	if Server == nil then
		return "Server == nil"
	end

	print("✅ Server / Shared")

	-----------
	-- Clouds
	-----------

	err = Client.cloudsTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Clouds")

	-----------
	-- CollisionGroups
	-----------

	err = Client.collisionGroupsTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ CollisionGroups")

	-----------
	-- Data
	-----------

	err = Client.dataTestSuite()
	if err ~= nil then
		return err
	end

	print("✅ Data")

	-----------
	-- Event
	-----------

	-- Can't send Events before connection is established now.
	-- Tests need to be updated.

	-- err = Client.eventTestSuite()
	-- if err ~= nil then
	-- 	return err
	-- end

	-- print("✅ Event")
	print("⚠️ Event (OFF)")

	-----------
	-- File
	-----------

	local file = Client.fileTestSuite()
	if file ~= nil then
		return file
	end

	print("✅ File")

	-----------
	-- Fog
	-----------

	local fog = Client.fogTestSuite()
	if fog ~= nil then
		return fog
	end

	print("✅ Fog")

	-----------
	-- Items
	-----------

	local items = Client.itemsTestSuite()
	if items ~= nil then
		return items
	end

	print("✅ Items")

	-----------
	-- Map
	-----------

	local map = Client.mapTestSuite()
	if map ~= nil then
		return map
	end

	-- NOTE: aduermael: there was an issue with Width/Height/Depth when the map was
	-- not a cube, but can't test this with current map (10x10x10)

	print("✅ Map")

	----------------------
	-- Number2
	----------------------

	local number2 = Client.number2TestSuite()
	if number2 ~= nil then
		return number2
	end

	print("✅ Number2")

	----------------------
	-- Number3
	----------------------

	local number3 = Client.number3TestSuite()
	if number3 ~= nil then
		return number3
	end

	print("✅ Number3")

	----------------------
	-- Rotation
	----------------------

	local rotation = Client.rotationTestSuite()
	if rotation ~= nil then
		return rotation
	end

	print("✅ Rotation")

	----------------------
	-- Object type
	----------------------

	local object = Client.objectTestSuite()
	if object ~= nil then
		return object
	end

	print("✅ Object")

	----------------------
	-- Palette
	----------------------

	local palette = Client.paletteTestSuite()
	if palette ~= nil then
		return palette
	end

	print("✅ Palette")

	----------------------
	-- Pointer
	----------------------

	local pointer = Client.pointerTestSuite()
	if pointer ~= nil then
		return pointer
	end

	print("✅ Pointer")

	----------------------
	-- Ray
	----------------------

	local ray = Client.rayTestSuite()
	if ray ~= nil then
		return ray
	end

	print("✅ Ray")

	----------------------
	-- Shape type
	----------------------

	local shape = Client.shapeTestSuite()
	if shape ~= nil then
		return shape
	end

	print("✅ Shape")

	----------------------
	-- MutableShape type
	----------------------

	local mutableShape = Client.mutableShapeTestSuite()
	if mutableShape ~= nil then
		return mutableShape
	end

	print("✅ MutableShape")

	----------------------
	-- Player type
	----------------------

	local player = Client.playerTestSuite()
	if player ~= nil then
		return player
	end

	print("✅ Player")

	----------------------
	-- Players
	----------------------

	-- Tests turned off, we now need to way for auth to complete
	-- for local player to be added to Players.
	-- No quick solution to include auth in the tests.

	-- local players = Client.playersTestSuite()
	-- if players ~= nil then
	-- 	return players
	-- end

	print("⚠️ Players (OFF)")

	----------------------
	-- OtherPlayers
	----------------------

	local otherPlayers = Client.otherPlayersTestSuite()
	if otherPlayers ~= nil then
		return otherPlayers
	end

	print("✅ OtherPlayers")

	-----------
	-- Time
	-----------

	local time = Client.timeTestSuite()
	if time ~= nil then
		return time
	end

	print("✅ Time")

	-----------
	-- TimeCycle
	-----------

	local timeCycle = Client.timeCycleTestSuite()
	if timeCycle ~= nil then
		return timeCycle
	end

	print("✅ TimeCycle")

	----------------------
	-- TimeCycleMark
	----------------------

	local timeCycleMark = Client.timeCycleMarkTestSuite()
	if timeCycleMark ~= nil then
		return timeCycleMark
	end

	print("✅ TimeCycleMark")

	----------------------
	-- Timer
	----------------------

	local timer = Client.timerTestSuite()
	if timer ~= nil then
		return timer
	end

	print("✅ Timer")

	----------------------
	-- JSON
	----------------------

	local json = Client.jsonTestSuite()
	if json ~= nil then
		return json
	end

	print("✅ JSON")

	err = unit.point.basic()
	if err ~= nil then
		return err
	end
	print("✅ Point")

	-- local printed = false
	-- while not dataTestEnded do
	-- 	if not printed then
	-- 		print("Waiting for a response")
	-- 		printed = true
	-- 	end
	-- end

	-- success
	return nil
end

-- testing to define Client.Tick
Client.Tick = function(dt)
	-- dummy function
end

Client.audioListenerTestSuite = function()
	if AudioListener == nil then
		return "AudioListener == nil"
	end
	return nil
end

Client.audioSourceTestSuite = function()
	if AudioSource == nil then
		return "AudioSource == nil"
	end
	return nil
end

Client.blockTestSuite = function()
	if Block == nil then
		return "Block == nil"
	end

	---- fields type checks

	local shape = Shape(Items.pen)
	local block = shape:GetBlock(Number3(0, 0, 0))

	if not checkType(block.Coordinates, "Number3") then
		return "Block.Coordinates"
	end
	if not checkType(block.Coords, "Number3") then
		return "Block.Coords"
	end
	if not checkType(block.Position, "Number3") then
		return "Block.Position"
	end
	if not checkType(block.Pos, "Number3") then
		return "Block.Pos"
	end
	if not checkType(block.LocalPosition, "Number3") then
		return "Block.LocalPosition"
	end
	if not checkType(block.AddNeighbor, "function") then
		return "Block:AddNeighbor"
	end
	if not checkType(block.Remove, "function") then
		return "Block:Remove"
	end
	if not checkType(block.Replace, "function") then
		return "Block:Replace"
	end
	if not checkType(block.PaletteIndex, "number") then
		return "Block.PaletteIndex"
	end

	---- functional checks

	local shape = MutableShape(Items.pen)
	local block1 = Block(Color.Red)
	if typeof(block1) ~= "Block" then
		return 'typeof(block1) ~= "Block"'
	end
	if block1 == nil then
		return "block1 (= Block(Color.Red)) == nil"
	end

	local block2 = Block(Color.Red, 0, 1, 2)
	if typeof(block2) ~= "Block" then
		return 'typeof(block2) ~= "Block"'
	end
	if block2 == nil then
		return "block2 (= Block(Color.Red, 0, 1, 2)) == nil"
	end
	shape:AddBlock(block2)

	-- comparison of Number3 is needed
	-- doing individual comparisons instead
	if block2.Coords.X ~= 0 then
		return "block2.Coords.X ~= 0"
	end
	if block2.Coords.Y ~= 1 then
		return "block2.Coords.Y ~= 1"
	end
	if block2.Coords.Z ~= 2 then
		return "block2.Coords.Z ~= 2"
	end

	local number3 = Number3(4, 5, 6)
	local block3 = Block(Color.Red, number3)
	if block3 == nil then
		return "block3 (= Block(Color.Red, number3)) == nil"
	end
	if typeof(block3) ~= Type.Block then
		return 'typeof(block3) ~= "Block"'
	end

	if block3.Coords.X ~= 4 then
		return "block3.Coords.X ~= 4"
	end
	if block3.Coords.Y ~= 5 then
		return "block3.Coords.Y ~= 5"
	end
	if block3.Coords.Z ~= 6 then
		return "block3.Coords.Z ~= 6"
	end

	-- TODO: test myBlock.Shape

	-- MutableShape
	---------------------------

	local mutableShape = MutableShape(Items.pen)
	if mutableShape == nil then
		return "[blockTestSuite] mutableShape is nil"
	end

	-- air block is nil
	local airBlock = mutableShape:GetBlock(Number3(0, 0, 21))
	if airBlock ~= nil then
		return "[blockTestSuite] mutableShape air block is not nil"
	end

	--
	local block = mutableShape:GetBlock(Number3(0, 0, 0))
	if block == nil then
		return "[blockTestSuite] mutableShape block is nil"
	end

	-- TODO: block:AddNeighbor(...) in mutable Shape

	-- block:Replace() in non-mutable Shape
	if pcall(block.Replace, Color.Red) then
		return "Block.Replace did not raise an error (1)"
	end
	if pcall(block.Replace, nil, Color.Red) then
		return "Block.Replace did not raise an error (2)"
	end
	block:Replace(1)
	block = mutableShape:GetBlock(Number3(0, 0, 0))
	if block == nil then
		return "[blockTestSuite] block is nil (1)"
	end
	if block.PaletteIndex ~= 1 then
		return "[blockTestSuite] mutableShape replaced block PaletteIndex ~= 1"
	end

	-- Replace with a palette index that doesn't exist in the shape's palette is expected to return an error
	if pcall(block.Replace, block, 32) then
		return "Block.Replace did not raise an error"
	end

	block:Replace(2)
	if block == nil then
		return "[blockTestSuite] block is nil (2)"
	end
	block = mutableShape:GetBlock(Number3(0, 0, 0))
	if block == nil then
		return "[blockTestSuite] block is nil (3)"
	end
	if block.PaletteIndex ~= 2 then
		return "[blockTestSuite] mutableShape replaced block PaletteIndex ~= 2"
	end

	-- block:Remove() in mutable Shape
	block:Remove()
	if pcall(block.Remove) then
		return "Block.Remove did not raise an error"
	end
	if block == nil then
		return "[blockTestSuite] mutableShape removed block is nil"
	end
	if block.PaletteIndex ~= 2 then
		return "[blockTestSuite] mutableShape removed block PaletteIndex is ~= 2"
	end
	-- TODO: block.Shape should be nil
	block = mutableShape:GetBlock(Number3(0, 0, 0))
	if block ~= nil then
		return "[blockTestSuite] mutableShape removed block is not nil"
	end

	-- (Non Mutable) Shape
	---------------------------

	-- TODO: block:AddNeighbor(...) in non-mutable Shape

	-- block:Remove() in non-mutable Shape
	local nonMutableShape = Shape(Items.pen)
	if nonMutableShape == nil then
		return "[blockTestSuite] nonMutableShape is nil"
	end

	-- air block is nil
	local airBlock = nonMutableShape:GetBlock(Number3(0, 0, 21))
	if airBlock ~= nil then
		return "[blockTestSuite] nonMutableShape air block is not nil"
	end

	--
	local block = nonMutableShape:GetBlock(Number3(0, 0, 0))
	if block == nil then
		return "nonMutableShape:GetBlock(0,0,0) == nil"
	end
	-- if block.PaletteIndex ~= 55 then return "nonMutableShape:GetBlock(0,0,0).PaletteIndex ~= 55" end

	block = nonMutableShape:GetBlock(Number3(0, 0, 0))
	if block == nil then
		return "nonMutableShape:GetBlock(0,0,0) == nil (3)"
	end
	-- if block.PaletteIndex ~= 55 then return "nonMutableShape:GetBlock(0,0,0).PaletteIndex ~= 55 (3)" end

	return nil
end

Client.faceTestSuite = function()
	if Face == nil then
		return "Face == nil"
	end
	if typeof(Face.Top) ~= "Face" then
		return 'typeof(Face.Top) ~= "Face"'
	end
	if typeof(Face.Bottom) ~= Type.Face then
		return 'typeof(Face.Bottom) ~= "Face"'
	end
	if typeof(Face.Right) ~= Type.Face then
		return 'typeof(Face.Right) ~= "Face"'
	end
	if typeof(Face.Left) ~= Type.Face then
		return 'typeof(Face.Left) ~= "Face"'
	end
	if typeof(Face.Front) ~= Type.Face then
		return 'typeof(Face.Front) ~= "Face"'
	end
	if typeof(Face.Back) ~= Type.Face then
		return 'typeof(Face.Back) ~= "Face"'
	end

	-- ensure the same table is used
	local a = Face.Top
	local b = Face.Top
	if a ~= b then
		return "Face.Top: two different tables given"
	end

	if Face.Top ~= Face.Top then
		return "Face.Top ~= Face.Top"
	end
	if Face.Bottom ~= Face.Bottom then
		return "Face.Bottom ~= Face.Bottom"
	end
	if Face.Left ~= Face.Left then
		return "Face.Left ~= Face.Left"
	end
	if Face.Right ~= Face.Right then
		return "Face.Right ~= Face.Right"
	end
	if Face.Front ~= Face.Front then
		return "Face.Front ~= Face.Front"
	end
	if Face.Back ~= Face.Back then
		return "Face.Back ~= Face.Back"
	end

	local base = Map:GetBlock(1, 1, 1)

	Map:GetBlock(1, 1, 0):Remove()
	Map:GetBlock(1, 0, 1):Remove()
	Map:GetBlock(1, 1, 2):Remove()
	Map:GetBlock(0, 1, 1):Remove()
	Map:GetBlock(2, 1, 1):Remove()
	Map:GetBlock(1, 2, 1):Remove()

	base:AddNeighbor(1, Face.Back)
	if Map:GetBlock(1, 1, 0) == nil then
		return "Face.Back has an unwanted behaviour"
	end
	base:AddNeighbor(1, Face.Bottom)
	if Map:GetBlock(1, 0, 1) == nil then
		return "Face.Bottom has an unwanted behaviour"
	end
	base:AddNeighbor(1, Face.Front)
	if Map:GetBlock(1, 1, 2) == nil then
		return "Face.Front has an unwanted behaviour"
	end
	base:AddNeighbor(1, Face.Left)
	if Map:GetBlock(0, 1, 1) == nil then
		return "Face.Left has an unwanted behaviour"
	end
	base:AddNeighbor(1, Face.Right)
	if Map:GetBlock(2, 1, 1) == nil then
		return "Face.Right has an unwanted behaviour"
	end
	base:AddNeighbor(1, Face.Top)
	if Map:GetBlock(1, 2, 1) == nil then
		return "Face.Top has an unwanted behaviour"
	end

	return nil
end

Client.blockPropertiesTestSuite = function()
	local shape = MutableShape(R.pen)

	local block10 = Block(Color.Red, 0, 0, 0)
	local block20 = Block(Color.Red, 1, 1, 1)

	if block10.Properties == nil then
		return "block10.Properties == nil"
	end
	if typeof(block10.Properties) ~= Type.Properties then
		return 'typeof(block10.Properties) ~= "Properties"'
	end

	if block10.Properties ~= nil then
		return "orphan block block10.Properties ~= nil"
	end
	shape:AddBlock(block10)
	shape:AddBlock(block20)
	--if block10.Properties == nil then return "non-orphan block block10.Properties == nil" end

	-- if block10.Properties.Color ~= Color(10, 11, 12) then return "incorrect block10.Properties.Color" end
	if block10.Properties.ID ~= 10 then
		return "incorrect block10.Properties.ID"
	end
	if block10.Properties.Light ~= true then
		return "incorrect block10.Properties.Light"
	end

	-- if block20.Properties.Color ~= Color(20, 21, 22) then return "incorrect block20.Properties.Color" end
	if block20.Properties.ID ~= 20 then
		return "incorrect block20.Properties.ID"
	end
	if block20.Properties.Light ~= false then
		return "incorrect block20.Properties.Light"
	end

	return nil
end

Client.boxTestSuite = function()
	if Box == nil then
		return "Box == nil"
	end

	local b1 = Box({ -1, -2, -3 }, { 1.0, 2.0, 3.0 })
	if pcall(Box, { 1.0, 2.0, 3.0 }) then
		return "Box({ 1.0, 2.0, 3.0 }) did not raise an error"
	end
	if typeof(b1) ~= "Box" then
		return 'typeof(b1) ~= "Box"'
	end
	if b1.Min ~= Number3(-1, -2, -3) then
		return "b1.Min ~= Number3(-1, -2, -3)"
	end
	if b1.Max ~= Number3(1, 2, 3) then
		return "b1.Max ~= Number3(1, 2, 3)"
	end
	if b1.Min.X ~= -1 then
		return "b1.Min.X ~= -1"
	end
	if b1.Min.Y ~= -2 then
		return "b1.Min.Y ~= -2"
	end
	if b1.Min.Z ~= -3 then
		return "b1.Min.Z ~= -3"
	end
	if b1.Max.X ~= 1 then
		return "b1.Max.X ~= 1"
	end
	if b1.Max.Y ~= 2 then
		return "b1.Max.Y ~= 2"
	end
	if b1.Max.Z ~= 3 then
		return "b1.Max.Z ~= 3"
	end
	--b1.Min.X = -2
	b1.Max = { 4, 5, 6 }
	--if b1.Min.X ~= -2.0 then return "b1.Min.X ~= -2.0" end
	if b1.Max.X ~= 4 then
		return "b1.Max.X ~= 4"
	end
	if b1.Max.Y ~= 5 then
		return "b1.Max.Y ~= 5"
	end
	if b1.Max.Z ~= 6 then
		return "b1.Max.Z ~= 6"
	end

	local b2 = Box(Number3(-1.0, -2.0, -3.0), Number3(1, 2, 3))
	if typeof(b2) ~= Type.Box then
		return 'typeof(b2) ~= "Box"'
	end
	if b2.Min ~= Number3(-1, -2, -3) then
		return "b2.Min ~= Number3(-1, -2, -3)"
	end
	if b2.Max ~= Number3(1, 2, 3) then
		return "b2.Max ~= Number3(1, 2, 3)"
	end
	if b2.Min.X ~= -1 then
		return "b2.Min.X ~= -1"
	end
	if b2.Min.Y ~= -2 then
		return "b2.Min.Y ~= -2"
	end
	if b2.Min.Z ~= -3 then
		return "b2.Min.Z ~= -3"
	end
	if b2.Max.X ~= 1 then
		return "b2.Max.X ~= 1"
	end
	if b2.Max.Y ~= 2 then
		return "b2.Max.Y ~= 2"
	end
	if b2.Max.Z ~= 3 then
		return "b2.Max.Z ~= 3"
	end

	local b3 = Box()
	if typeof(b3) ~= Type.Box then
		return 'typeof(b3) ~= "Box"'
	end
	if b3.Min ~= Number3(0, 0, 0) then
		return "b3.Min ~= Number3(0, 0, 0)"
	end
	if b3.Max ~= Number3(0, 0, 0) then
		return "b3.Max ~= Number3(0, 0, 0)"
	end
	if b3.Min.X ~= 0 then
		return "b3.Min.X ~= 0"
	end
	if b3.Min.Y ~= 0 then
		return "b3.Min.Y ~= 0"
	end
	if b3.Min.Z ~= 0 then
		return "b3.Min.Z ~= 0"
	end
	if b3.Max.X ~= 0 then
		return "b3.Max.X ~= 0"
	end
	if b3.Max.Y ~= 0 then
		return "b3.Max.Y ~= 0"
	end
	if b3.Max.Z ~= 0 then
		return "b3.Max.Z ~= 0"
	end
	local b3MinRef = b3.Min
	b3.Min = Number3(-1, -1, -1)
	--if b3MinRef ~= Number3(-1, -1, -1) then return "b3MinRef ~= Number3(-1, -1, -1)" end

	b3 = nil
	collectgarbage("collect")
	b3MinRef.X = 45 -- should not crash, thanks to weak pointers
	if b3MinRef ~= Number3(45, 0, 0) then
		return " b3MinRef ~= Number3(0,0,0) after parent Box is GC-ed"
	end

	-- box:Copy()

	-- Shape boxes tests

	local p = Shape(Items.pen)
	if typeof(p.BoundingBox) ~= Type.Box then
		return 'typeof(p.BoundingBox) ~= "Box"'
	end
	if p.BoundingBox.Min ~= Number3(0, 0, 0) then
		return "p.BoundingBox.Min ~= Number3(0, 0, 0)"
	end
	if p.BoundingBox.Max ~= Number3(5, 5, 26) then
		return "p.BoundingBox.Max ~= Number3(5, 5, 26)"
	end

	if typeof(p.CollisionBox) ~= Type.Box then
		return 'typeof(p.CollisionBox) ~= "Box"'
	end
	if p.CollisionBox.Min ~= Number3(0, 0, 0) then
		return "p.CollisionBox.Min ~= Number3(0, 0, 0)"
	end
	if p.CollisionBox.Max ~= Number3(5, 5, 26) then
		return "p.CollisionBox.Max ~= Number3(5, 5, 26)"
	end
	if p.CollisionBox.Min.X ~= 0 or p.CollisionBox.Min.Y ~= 0 or p.CollisionBox.Min.Z ~= 0 then
		return "p.CollisionBox.Min.X ~= 0 or p.CollisionBox.Min.Y ~= 0 or p.CollisionBox.Min.Z ~= 0"
	end
	if p.CollisionBox.Max.X ~= 5 or p.CollisionBox.Max.Y ~= 5 or p.CollisionBox.Max.Z ~= 26 then
		return "p.CollisionBox.Max.X ~= 5 or p.CollisionBox.Max.Y ~= 5 or p.CollisionBox.Max.Z ~= 26"
	end
	p.CollisionBox.Min = Number3(-1, -2, -3)
	p.CollisionBox.Max = Number3(27, 28, 29)
	if p.CollisionBox.Min ~= Number3(-1, -2, -3) then
		return "p.CollisionBox.Min ~= Number3(-1, -2, -3)"
	end
	if p.CollisionBox.Max ~= Number3(27, 28, 29) then
		return "p.CollisionBox.Max ~= Number3(27, 28, 29)"
	end
	p.CollisionBox = Box(Number3(1, 2, 3), Number3(4, 5, 6))
	if p.CollisionBox.Min ~= Number3(1, 2, 3) then
		return "p.CollisionBox.Min ~= Number3(1, 2, 3)"
	end
	if p.CollisionBox.Max ~= Number3(4, 5, 6) then
		return "p.CollisionBox.Max ~= Number3(4, 5, 6)"
	end

	b1 = nil
	b2 = nil
	b3 = nil
	b3MinRef = nil
	p = nil
	collectgarbage("collect")

	return nil
end

Client.cameraTestSuite = function()
	if Camera == nil then
		return "Camera == nil"
	end
	if not checkType(Camera, "Camera") then
		return "Camera"
	end

	Camera:SetModeFree()
	Camera:SetParent(Map)

	---- Object extension checks

	local cameraAsObject = Client.objectSupportTestSuite(Camera)
	if cameraAsObject ~= nil then
		return cameraAsObject
	end

	---- fields type checks

	if not checkType(Camera.SetModeFirstPerson, "function") then
		return "Camera:SetModeFirstPerson"
	end
	if pcall(Camera.SetModeFirstPerson) then
		return "Camera.SetModeFirstPerson did not raise an error"
	end

	if not checkType(Camera.SetModeThirdPerson, "function") then
		return "Camera:SetModeThirdPerson"
	end
	if pcall(Camera.SetModeThirdPerson) then
		return "Camera.SetModeThirdPerson did not raise an error"
	end

	if not checkType(Camera.SetModeSatellite, "function") then
		return "Camera:SetModeSatellite"
	end
	if pcall(Camera.SetModeSatellite) then
		return "Camera.SetModeSatellite did not raise an error"
	end

	if not checkType(Camera.SetModeFree, "function") then
		return "Camera:SetModeFree"
	end
	if pcall(Camera.SetModeFree) then
		return "Camera.SetModeFree did not raise an error"
	end

	if not checkType(Camera.CastRay, "function") then
		return "Camera:CastRay"
	end
	if pcall(Camera.CastRay) then
		return "Camera.CastRay did not raise an error"
	end

	if not checkType(Camera.FitToScreen, "function") then
		return "Camera:FitToScreen"
	end
	if pcall(Camera.FitToScreen) then
		return "Camera.FitToScreen did not raise an error"
	end

	---- functional checks

	local impact = Camera:CastRay()

	Camera:FitToScreen(Shape(Items.pen), 0.5, false)

	return nil
end

Client.clientTestSuite = function()
	if Client == nil then
		return "Client == nil"
	end

	-- OS & hardware infos
	if Client.OSName == nil then
		return "Client.OSName is nil"
	end
	if Client.OSVersion == nil then
		return "Client.OSVersion is nil"
	end
	if Client.IsMobile == nil then
		return "Client.IsMobile is nil"
	end
	if Client.IsPC == nil then
		return "Client.IsPC is nil"
	end
	if Client.IsConsole == nil then
		return "Client.IsConsole is nil"
	end
	if Client.HasTouchScreen == nil then
		return "Client.HasTouchScreen is nil"
	end

	if Client.OSName ~= "Linux" and Client.OSName ~= "macOS" then
		return "Client.OSName is not Linux nor macOS:" .. Client.OSName
	end
	-- if Client.OSVersion ~= "22.04" then return "Client.OSName is not 22.04" end
	if Client.IsMobile ~= false then
		return "Client.IsMobile is not false"
	end
	if Client.IsPC ~= true then
		return "Client.IsPC is not true"
	end
	if Client.IsConsole ~= false then
		return "Client.IsConsole is not false"
	end
	if Client.HasTouchScreen ~= false then
		return "Client.HasTouchScreen is not false"
	end

	if Client.HapticFeedback == nil then
		return "Client.HapticFeedback"
	end

	if Client.ShowVirtualKeyboard == nil then
		return "Client.ShowVirtualKeyboard"
	end
	if Client.HideVirtualKeyboard == nil then
		return "Client.HideVirtualKeyboard"
	end

	local actionFunc = function() end
	Client.Action1 = actionFunc
	if Client.Action1 == nil then
		return "Client.Action1 update failed"
	end
	Client.Action1Release = actionFunc
	if Client.Action1Release == nil then
		return "Client.Action1Release update failed"
	end

	Client.Action2 = actionFunc
	if Client.Action2 == nil then
		return "Client.Action2 update failed"
	end
	Client.Action2Release = actionFunc
	if Client.Action2Release == nil then
		return "Client.Action2Release update failed"
	end

	if Client.Action3 ~= nil then
		return "Client.Action3 ~= nil"
	end
	Client.Action3 = actionFunc
	if Client.Action3 == nil then
		return "Client.Action3 update failed"
	end
	Client.Action3Release = actionFunc
	if Client.Action3Release == nil then
		return "Client.Action3Release update failed"
	end

	if Camera == nil then
		return "Camera == nil"
	end
	if Client.Clouds == nil then
		return "Client.Clouds == nil"
	end

	Client.DirectionalPad = function(a, b)
		print(a, b)
	end
	if Client.DirectionalPad == nil then
		return "Client.DirectionalPad update failed"
	end

	Client.AnalogPad = function(a, b)
		print(a, b)
	end
	if Client.AnalogPad == nil then
		return "Client.AnalogPad update failed"
	end

	if Client.Fog == nil then
		return "Client.Fog == nil"
	end
	if Client.Inputs == nil then
		return "Client.Inputs == nil"
	end

	Client.OnPlayerJoin = function(player)
		print(player.Username .. " joined the game!")
	end
	if Client.OnPlayerJoin == nil then
		return "Client.OnPlayerJoin update failed"
	end

	Client.OnPlayerLeave = function(player)
		print("So long " .. player.Username .. "!")
	end
	if Client.OnPlayerLeave == nil then
		return "Client.OnPlayerLeave update failed"
	end

	Client.OnStart = function() end
	if Client.OnStart == nil then
		return "Client.OnStart update failed"
	end

	if Client.Player == nil then
		return "Client.Player == nil"
	end

	Client.Tick = function(dt) end
	if Client.Tick == nil then
		return "Client.Tick update failed"
	end

	return nil
end

Client.cloudsTestSuite = function()
	if Clouds == nil then
		return "Clouds == nil"
	end

	-- cannot be tested in headless mode

	return nil
end

Client.collisionGroupsTestSuite = function()
	if CollisionGroups == nil then
		return "CollisionGroups"
	end

	local g = CollisionGroups()
	if typeof(g) ~= "CollisionGroups" then
		return 'typeof(g) ~= "CollisionGroups"'
	end
	if #g ~= 0 then
		return "#g ~= 0"
	end

	g = CollisionGroups(1, 3, 4)

	if #g ~= 3 then
		return "#g ~= 3"
	end

	if g[1] ~= 1 then
		return "g[1] ~= 1"
	end
	if g[2] ~= 3 then
		return "g[2] ~= 3"
	end
	if g[3] ~= 4 then
		return "g[3] ~= 4"
	end

	for i = 1, #g do
		local v = g[i]
		if i == 1 and v ~= 1 then
			return "i == 1 and v ~= 1"
		elseif i == 2 and v ~= 3 then
			return "i == 2 and v ~= 3"
		elseif i == 3 and v ~= 4 then
			return "i == 3 and v ~= 4"
		end
	end

	for i = 1, #g do
		local v = g[i]
		if k == 1 and v ~= 1 then
			return "k == 1 and v ~= 1"
		elseif k == 2 and v ~= 3 then
			return "k == 2 and v ~= 3"
		elseif k == 3 and v ~= 4 then
			return "k == 3 and v ~= 4"
		end
	end

	if g ~= CollisionGroups(1, 3, 4) then
		return "g ~= CollisionGroups(1, 3, 4)"
	end
	if g ~= CollisionGroups(1, 4, 3) then
		return "g ~= CollisionGroups(1, 4, 3)"
	end
	if g ~= CollisionGroups(1, 4, 3, 3, 3, 3) then
		return "g ~= CollisionGroups(1, 4, 3, 3, 3, 3)"
	end

	if CollisionGroups(1, 3, 4) ~= g then
		return "CollisionGroups(1, 3, 4) ~= g"
	end
	if CollisionGroups(1, 4, 3) ~= g then
		return "CollisionGroups(1, 4, 3) ~= g "
	end
	if CollisionGroups(1, 4, 3, 3, 3, 3) ~= g then
		return "CollisionGroups(1, 4, 3, 3, 3, 3) ~= g"
	end

	g = CollisionGroups({ 1, 3, 4 })

	if #g ~= 3 then
		return "#g ~= 3"
	end

	if g[1] ~= 1 then
		return "g[1] ~= 1"
	end
	if g[2] ~= 3 then
		return "g[2] ~= 3"
	end
	if g[3] ~= 4 then
		return "g[3] ~= 4"
	end

	for i = 1, #g do
		local v = g[i]
		if i == 1 and v ~= 1 then
			return "i == 1 and v ~= 1"
		elseif i == 2 and v ~= 3 then
			return "i == 2 and v ~= 3"
		elseif i == 3 and v ~= 4 then
			return "i == 3 and v ~= 4"
		end
	end

	for i = 1, #g do
		local v = g[i]
		if k == 1 and v ~= 1 then
			return "k == 1 and v ~= 1"
		elseif k == 2 and v ~= 3 then
			return "k == 2 and v ~= 3"
		elseif k == 3 and v ~= 4 then
			return "k == 3 and v ~= 4"
		end
	end

	if g ~= CollisionGroups(1, 3, 4) then
		return "g ~= CollisionGroups(1, 3, 4)"
	end
	if g ~= CollisionGroups(1, 4, 3) then
		return "g ~= CollisionGroups(1, 4, 3)"
	end
	if g ~= CollisionGroups(1, 4, 3, 3, 3, 3) then
		return "g ~= CollisionGroups(1, 4, 3, 3, 3, 3)"
	end

	g = CollisionGroups(1, 2, 5) + CollisionGroups(3, 4)
	if g ~= CollisionGroups(1, 2, 3, 4, 5) then
		return "g ~= CollisionGroups(1, 2, 3, 4, 5)"
	end
	if g ~= CollisionGroups(1, 2, 3, 4, 5, 5, 5) then
		return "g ~= CollisionGroups(1, 2, 3, 4, 5, 5, 5)"
	end

	g = CollisionGroups(3, 4) + CollisionGroups(1, 2, 5)
	if g ~= CollisionGroups(1, 2, 3, 4, 5) then
		return "g ~= CollisionGroups(1, 2, 3, 4, 5)"
	end
	if g ~= CollisionGroups(1, 2, 3, 4, 5, 5, 5) then
		return "g ~= CollisionGroups(1, 2, 3, 4, 5, 5, 5)"
	end

	g = CollisionGroups(1, 2, 3, 4, 5) - CollisionGroups(4, 5)
	if g ~= CollisionGroups(1, 2, 3) then
		return "g ~= CollisionGroups(1, 2, 3)"
	end

	g = CollisionGroups(1, 2, 3, 4, 5) - CollisionGroups(4, 5)
	if g ~= CollisionGroups(1, 2, 3) then
		return "g ~= CollisionGroups(1, 2, 3)"
	end

	return nil
end

dataSuccess = false
dataTestEnded = false
Client.dataTestSuite = function()
	return nil
	-- local e = Event()
	-- e.name = "data_test"
	-- e:SendTo(Server)
end

-- Client.eventtypeTestSuite = function()

-- 	EventType.eType1 = EventType:New()
-- 	-- local event = Event(EventType.eType1)

-- 	if event == nil then return "can't create Event (event == nil)" end
-- 	if typeof(event) ~= "Event" then return "typeof(event) ~= \"Event\"" end

-- 	return nil
-- end

Client.eventTestSuite = function()
	if Event == nil then
		return "Event == nil"
	end

	local e = Event()
	if e == nil then
		return "event instance is nil"
	end

	e.foo = "bar"
	if e.foo ~= "bar" then
		return 'e.foo ~= "bar"'
	end

	e.baz = 42
	if e.baz ~= 42 then
		return "e.baz ~= 42"
	end

	-- ...

	return nil
end

Client.colorTestSuite = function()
	local c1 = Color(255, 0, 0)
	if pcall(Color) then
		return "Color() did not raise and error"
	end
	if typeof(c1) ~= Type.Color then
		return 'typeof(c1) ~= "Color"'
	end

	c1.R = 0
	if c1.R ~= 0 then
		return "Color: R update failed"
	end

	local c2 = c1
	if c2 ~= c1 then
		return "Color: assignation failed"
	end
	c2.G = 255
	if c2.G ~= 255 then
		return "Color: G update failed"
	end
	if c1.G ~= 255 then
		return "Color: reference copy failed"
	end

	local c3 = Color(0, 0, 0, 0)
	c3.B = 255
	if c3.B ~= 255 then
		return "Color: B update failed"
	end
	c3.A = 255
	if c3.A ~= 255 then
		return "Color: A update failed"
	end

	if c3.Alpha == nil then
		return "c3.Alpha == nil"
	end
	if c3.Blue == nil then
		return "c3.Blue == nil"
	end
	if c3.Green == nil then
		return "c3.Green == nil"
	end
	if c3.Red == nil then
		return "c3.Red == nil"
	end

	local c4 = Color(0.5, 0.5, 0.5, 0.5)
	if c4.R ~= 127 then
		return "Color: initialization with float failed (R)"
	end
	if c4.G ~= 127 then
		return "Color: initialization with float failed (G)"
	end
	if c4.B ~= 127 then
		return "Color: initialization with float failed (B)"
	end
	if c4.A ~= 127 then
		return "Color: initialization with float failed (A)"
	end

	local blend1 = Color(0.8, 0.6, 0.7, 255)
	local blend2 = Color(0.2, 0.5, 0.4, 0.6)
	if blend1 * blend2 ~= Color(0.16, 0.3, 0.28, 0.6) then
		return "Color: blend1 * blend2"
	end
	if blend1 * 0.5 ~= Color(0.4, 0.3, 0.35, 0.5) then
		return "Color: blend1 * 0.5"
	end
	if blend1 / blend2 ~= Color(255, 255, 255, 255) then
		return "Color: blend1 / blend2"
	end
	if blend1 / 2 ~= Color(0.4, 0.3, 0.35, 0.5) then
		return "Color: blend1 / 2"
	end
	if blend1 + blend2 ~= Color(255, 255, 255, 255) then
		return "Color: blend1 + blend2"
	end
	if blend1 + 0.5 ~= Color(255, 255, 255, 255) then
		return "Color: blend1 + 0.5"
	end
	if blend1 - blend2 ~= Color(153, 26, 76, 102) then
		return "Color: blend1 - blend2"
	end
	if blend1 - 0.5 ~= Color(77, 26, 51, 128) then
		return "Color: blend1 - 0.5"
	end

	-- explicitly calling the garbage collection
	c1 = nil
	c2 = nil
	c3 = nil
	c4 = nil
	blend1 = nil
	blend2 = nil
	collectgarbage("collect")

	return nil
end

Client.configTestSuite = function()
	-- Config is set at the beginning of this file
	if Config == nil then
		return "Config == nil"
	end

	if Items ~= Config.Items then
		return "Items ~= Config.Items"
	end

	if typeof(Config.Map) ~= "Item" then
		return 'Config.Map.Type ~= "Item"'
	end
	if typeof(Config.Items) ~= "Items" then
		return 'typeof(Config.Items) ~= "Items"'
	end
	if typeof(Items) ~= "Items" then
		return 'typeof(Items) ~= "Items"'
	end
	if typeof(Config.ConstantAcceleration) ~= "Number3" then
		return 'typeof(Config.ConstantAcceleration) ~= "Number3"'
	end

	if Config.Map.Name ~= "black_cube_10x10" then
		return 'Config.Map.Name ~= "black_cube_10x10"'
	end
	if Config.Map.Repo ~= "official" then
		return 'Config.Map.Repo ~= "official"'
	end

	if Config.Items[1].Name ~= "pen" then
		return 'Config.Items[1].Name ~= "pen"'
	end
	if Config.Items[2].Fullname ~= "aduermael.sword" then
		return 'Config.Items[2].Fullname ~= "aduermael.sword"'
	end
	if Config.foo ~= "bar" then
		return 'Config.foo ~= "bar"'
	end

	for i = 1, #Config.Items do
		if i == 1 and Config.Items[i].Name ~= "pen" then
			return 'Config.Items[1].Name ~= "pen" (ipairs)'
		end
		if i == 2 and Config.Items[i].Fullname ~= "aduermael.sword" then
			return 'Config.Items[2].Fullname ~= "aduermael.sword" (ipairs)'
		end
	end

	if Config.ConstantAcceleration.Y ~= -225.0 then
		return "Config.ConstantAcceleration.Y ~= -225.0"
	end

	-- TODO: Config should be read-only here, make sure of it
	-- as soon as it's implemented
	return nil
end

Client.fileTestSuite = function()
	if File == nil then
		return "File == nil"
	end
	if File.OpenAndReadAll == nil then
		return "File.OpenAndReadAll == nil"
	end
	if typeof(File.OpenAndReadAll) ~= "function" then
		return 'typeof(File.OpenAndReadAll) ~= Type.["function"]'
	end
end

Client.fogTestSuite = function()
	if Fog == nil then
		return "Fog == nil"
	end

	if typeof(Fog.On) ~= "boolean" then
		return 'typeof(Fog.On) ~= "boolean"'
	end
	if typeof(Fog.LightAbsorption) ~= Type.number then
		return 'typeof(Fog.LightAbsorption) ~= "number"'
	end
	if typeof(Fog.Near) ~= Type.number then
		return 'typeof(Fog.Near) ~= "number"'
	end
	if typeof(Fog.Far) ~= Type.number then
		return 'typeof(Fog.Far) ~= "number"'
	end
	if typeof(Fog.Distance) ~= Type.number then
		return 'typeof(Fog.Distance) ~= "number"'
	end

	-- NOTE: the attributes cannot be changed on headless
	-- Fog.On = false
	-- if Fog.On ~= false then return "Fog.On update failed" end
	-- Fog.On = false --not Fog.On
	-- if Fog.On ~= true then return "Fog.On toggle failed" end
	-- Fog.LightAbsorption = 0
	-- if Fog.LightAbsorption ~= 0.0 then return "Fog.LightAbsorption update (0) failed" end
	-- Fog.LightAbsorption = 0.5
	-- if Fog.LightAbsorption ~= 0.5 then return "Fog.LightAbsorption update (0.5) failed" end
	-- Fog.Near = 50
	-- if Fog.Near ~= 50 then return "Fog.Near update failed" end
	-- Fog.Far = 100 -- offset of 50
	-- if Fog.Far ~= 100 then return "Fog.Far update failed" end
	-- Fog.Distance = 150
	-- if Fog.Distance ~= 150 then return "Fog.Distance update failed" end
	-- if Fog.Near ~= 150 then return "Fog.Distance: Fog.Near update failed" end
	-- if Fog.Far ~= 200 then return "Fog.Distance: Fog.Far update failed" end
	return nil
end

Client.inputsTestSuite = function()
	if Inputs == nil then
		return "Inputs == nil"
	end
	if Inputs.Pointer == nil then
		return "Inputs.Pointer == nil"
	end

	return nil
end

Client.itemsTestSuite = function()
	if Items == nil then
		return "Items == nil"
	end

	if typeof(Items[1]) ~= Type.Item then
		return 'typeof(Items[1]) ~= "Item"'
	end
	if typeof(Items[2]) ~= Type.Item then
		return 'typeof(Items[2]) ~= "Item"'
	end

	if Items[1] ~= Items.pen then
		return "Items[1] ~= Items.pen"
	end
	if Items[2] ~= Items.aduermael.sword then
		return "Items[2] ~= Items.aduermael.sword"
	end

	-- 2 items
	if #Items ~= 2 then
		return "#Items ~= 2 -> #Items == " .. #Items
	end

	return nil
end

Client.mapTestSuite = function()
	if Map == nil then
		return "Map == nil"
	end
	if typeof(Map) ~= Type.Map then
		return 'typeof(Map) ~= "Map"'
	end

	--------------------------
	-- Map:GetBlock
	--------------------------

	local mapBlock = Map:GetBlock(0, 0, 0)
	if mapBlock == nil then
		return "Map:GetBlock(0,0,0) == nil"
	end
	if typeof(mapBlock) ~= Type.Block then
		return 'Map:GetBlock(0,0,0) incorrect type ("Block)"'
	end
	-- if mapBlock.PaletteIndex ~= 211 then return "Map:GetBlock(0,0,0) ~= 211" end

	mapBlock = Map:GetBlock({ 0.9, 0.8, 0.7 })
	if pcall(Map.GetBlock, { 0.9, 0.8, 0.7 }) then
		return "Map.GetBlock did not raise an error"
	end
	if pcall(Map.GetBlock, nil, { 0.9, 0.8, 0.7 }) then
		return "Map.GetBlock did not raise an error"
	end
	if mapBlock == nil then
		return "Map:GetBlock({0.9, 0.8, 0.7}) == nil"
	end
	if typeof(mapBlock) ~= Type.Block then
		return 'Map:GetBlock({0.9, 0.8, 0.7}) incorrect type ("Block)"'
	end
	-- if mapBlock.PaletteIndex ~= 211 then return "Map:GetBlock({0.9, 0.8, 0.7}) ~= 211" end

	mapBlock = Map:GetBlock(Number3(0, 0, 0))
	if mapBlock == nil then
		return "Map:GetBlock(Number3(0,0,0)) == nil"
	end
	if typeof(mapBlock) ~= Type.Block then
		return 'Map:GetBlock(Number3(0,0,0)) incorrect type ("Block)"'
	end
	-- if mapBlock.PaletteIndex ~= 211 then return "Map:GetBlock(Number3(0,0,0)) ~= 211" end

	-- air blocks should be nil
	Map:GetBlock(0, 0, 0):Remove()
	if Map:GetBlock(0, 0, 0) ~= nil then
		return "Map:GetBlock(0, 0, 0) -> air block is not nil"
	end
	if Map:GetBlock(Number3(0, 0, 0)) ~= nil then
		return "Map:GetBlock(Number3(0, 0, 0)) -> air block is not nil"
	end
	if Map:GetBlock({ 0, 0, 0 }) ~= nil then
		return "Map:GetBlock({0, 0, 0}) -> air block is not nil"
	end
	Map:AddBlock(1, 0, 0, 0)

	-- try getting a non-existent block (nil value is expected)
	local noblock = Map:GetBlock(-1, 0, 0)
	if noblock ~= nil then
		return "noblock ~= nil"
	end

	--------------------------
	-- Map:AddBlock
	--------------------------
	Map:GetBlock(0, 0, 0):Remove()
	if Map:GetBlock(0, 0, 0) ~= nil then
		return "Map:GetBlock(0, 0, 0):Remove() failed"
	end
	local addedBlock = Map:AddBlock(1, 0, 0, 0)
	if addedBlock == false then
		return "Map:AddBlock(Block(1, 0, 0, 0)) failed"
	end

	Map:GetBlock(0.1, 0.2, 0.3):Remove()
	if Map:GetBlock(0, 0, 0) ~= nil then
		return "Map:GetBlock(0, 0, 0):Remove() failed"
	end
	local addedBlock = Map:AddBlock(1, 0, 0, 0)
	if addedBlock == false then
		return "Map:AddBlock(1, 0, 0, 0) failed"
	end

	Map:GetBlock(0.4, 0.5, 0.6):Remove()
	if Map:GetBlock(0, 0, 0) ~= nil then
		return "Map:GetBlock(0, 0, 0):Remove() failed"
	end
	local addedBlock = Map:AddBlock(1, Number3(0, 0, 0))
	if addedBlock == false then
		return "Map:AddBlock(1, 0, 0, 0) failed"
	end

	-- set block back to PaletteIndex == 1
	Map:GetBlock(0, 0, 0):Remove()
	if Map:GetBlock(0, 0, 0) ~= nil then
		return "Map:GetBlock: incorrect type (nil)"
	end
	Map:AddBlock(1, 0, 0, 0)
	if typeof(Map:GetBlock(0, 0, 0)) ~= Type.Block then
		return "Map:AddBlock update failed"
	end

	if Map.Width ~= 10.0 then
		return "Map.Width ~= 10.0"
	end
	if typeof(Map.Width) ~= Type.number then
		return 'typeof(Map.Width) ~= "number"'
	end
	if Map.Height ~= 10.0 then
		return "Map.Height ~= 10.0"
	end
	if typeof(Map.Height) ~= Type.number then
		return 'typeof(Map.Height) ~= "number"'
	end
	if Map.Depth ~= 10.0 then
		return "Map.Depth ~= 10.0"
	end
	if typeof(Map.Depth) ~= Type.number then
		return 'typeof(Map.Depth) ~= "number"'
	end

	if Map.ChildrenCount ~= 1 then
		return "Map.ChildrenCount ~= 1 (#1)"
	end

	local o = Object()
	Map:AddChild(o)
	if Map.ChildrenCount ~= 2 then
		return "Map.ChildrenCount ~= 2(#2)"
	end
	Map:RemoveChild(o)
	if Map.ChildrenCount ~= 1 then
		return "Map.ChildrenCount ~= 1(#3)"
	end

	local previousChild = Map:GetChild(1)
	Map:RemoveChildren()
	Map:RemoveChildren()
	if Map.ChildrenCount ~= 0 then
		return "Map.ChildrenCount ~= 0(#4)"
	end
	print(typeof(previousChild))
	Map:AddChild(previousChild)
	if Map.ChildrenCount ~= 1 then
		return "Map.ChildrenCount ~= 1(#5)"
	end

	-- if typeof(Map.LocalPalette[211].Color) ~= Type.Color then return 'typeof(Map.LocalPalette[211].Color) ~= "Color"' end

	if Map.Depth ~= 10 then
		return "Map.Depth ~= 10"
	end
	if Map.Height ~= 10 then
		return "Map.Height ~= 10"
	end
	if Map.Width ~= 10 then
		return "Map.Width ~= 10"
	end

	return nil
end

Client.number2TestSuite = function()
	if Number2 == nil then
		return "Number2 == nil"
	end
	local number2 = Number2(10, 20)
	if number2 == nil then
		return "number2 == nil"
	end
	if typeof(number2) ~= Type.Number2 then
		return 'typeof(number2) ~= "Number2"'
	end
	if number2.X ~= 10 then
		return "number2.X ~= 10"
	end
	if number2.Y ~= 20 then
		return "number2.Y ~= 20"
	end

	number2.X = number2.X + 1
	if number2.X ~= 11 then
		return "number2.X ~= 11"
	end

	number2.Y = number2.Y + 1
	if number2.Y ~= 21 then
		return "number2.Y ~= 21"
	end

	number2 = Number2(10, 20)
	number2 = -number2
	if number2.X ~= -10 then
		return "number2.X ~= -10"
	end
	if number2.Y ~= -20 then
		return "number2.Y ~= -20"
	end

	number2 = Number2(10, 20)
	number2 = number2 - Number2(1, 1)
	if number2.X ~= 9 then
		return "number2.X ~= 9"
	end
	if number2.Y ~= 19 then
		return "number2.Y ~= 19"
	end

	number2 = Number2(10.125, 20.25)
	number2 = number2 - Number2(1, 1)
	if number2.X ~= 9.125 then
		return "number2.X ~= 9.125"
	end
	if number2.Y ~= 19.25 then
		return "number2.Y ~= 19.25"
	end

	number2 = Number2(10, 20)
	number2 = number2 / 2
	if number2.X ~= 5 then
		return "number2.X ~= 5"
	end
	if number2.Y ~= 10 then
		return "number2.Y ~= 10"
	end

	number2 = Number2(10, 20)
	number2 = number2 / Number2(4, 10)
	if number2.X ~= 2.5 then
		return "number2.X ~= 2.5"
	end
	if number2.Y ~= 2 then
		return "number2.Y ~= 2"
	end

	number2 = Number2(5, 10)
	number2 = number2 ^ 2
	if number2.X ~= 25 then
		return "number2.X ~= 25"
	end
	if number2.Y ~= 100 then
		return "number2.Y ~= 100"
	end

	number2 = Number2(10, 10)
	number2 = number2 // 3
	if number2.X ~= 3 then
		return "number3.X ~= 3"
	end
	if number2.Y ~= 3 then
		return "number3.Y ~= 3"
	end

	number2 = Number2(10, 20)
	number2 = number2 // Number2(4, 10)
	if number2.X ~= 2 then
		return "number2.X ~= 2"
	end
	if number2.Y ~= 2 then
		return "number2.Y ~= 2"
	end

	number2 = Number2(10, 20)
	number2 = number2 * 2
	if number2.X ~= 20 then
		return "number2.X ~= 20"
	end
	if number2.Y ~= 40 then
		return "number2.Y ~= 40"
	end

	number2 = Number2(10, 20)
	number2 = 2 * number2
	if number2.X ~= 20 then
		return "number2.X ~= 20"
	end
	if number2.Y ~= 40 then
		return "number2.Y ~= 40"
	end

	-- check == operator
	local number2 = Number2(1, 2)
	if not number2 == Number2(1, 2) then
		return "number2 ~= Number2(1,2)"
	end

	-- Normalize
	number2 = Number2(10, 50)
	if pcall(number2.Normalize) then
		return "Number2.Normalize did not raise an error"
	end
	number2:Normalize()
	if numberEqual(number2.Length, 1, epsilon) == false then
		return "number2.Length ~= 1"
	end

	-- Length (get / set)
	number2 = Number2(13, 0)
	if number2.Length ~= 13 then
		return "number2.Length ~= 13"
	end

	number2.Length = 7
	if number2.Length ~= 7 then
		return "number2.Length ~= 7 (" .. number2.Length .. ")"
	end
	if number2.X ~= 7 then
		return "number2.X ~= 7"
	end

	-- SquaredLength (get / set)
	number2 = Number2(2, 2)
	if number2.SquaredLength ~= 8 then
		return "number3.SquaredLength ~= 8"
	end

	number2.SquaredLength = 3

	if numberEqual(number2.SquaredLength, 3, epsilon) == false then
		return "numberEqual(number2.SquaredLength, 3, epsilon) == false"
	end
	if numberEqual(number2.Length, 1.732050, epsilon) == false then
		return "numberEqual(number2.Length, 1.732050, epsilon)"
	end

	return nil
end

Client.number3TestSuite = function()
	if Number3 == nil then
		return "Number3 == nil"
	end
	local number3 = Number3(10, 20, 30)
	if number3 == nil then
		return "number3 == nil"
	end
	if typeof(number3) ~= "Number3" then
		return 'typeof(number3) ~= "Number3"'
	end
	if number3.X ~= 10 then
		return "number3.X ~= 10"
	end
	if number3.Y ~= 20 then
		return "number3.Y ~= 20"
	end
	if number3.Z ~= 30 then
		return "number3.Z ~= 30"
	end
	if number3 ~= Number3(10, 20, 30) then
		return "number3 ~= Number3(10, 20, 30)"
	end
	if Number3(10, 20, 30) ~= number3 then
		return "Number3(10, 20, 30) ~= number3"
	end
	if number3 + { 1, 2, 3 } ~= Number3(11, 22, 33) then
		return "number3 + {1, 2, 3} ~= Number3(11, 22, 33)"
	end
	if number3 - { 1, 2, 3 } ~= Number3(9, 18, 27) then
		return "number3 - {1, 2, 3} ~= Number3(9, 18, 27)"
	end

	number3.X = number3.X + 1
	if number3.X ~= 11 then
		return "number3.X ~= 11"
	end

	number3.Y = number3.Y + 1
	if number3.Y ~= 21 then
		return "number3.Y ~= 21"
	end

	number3.Z = number3.Z + 1
	if number3.Z ~= 31 then
		return "number3.Z ~= 31"
	end

	number3 = Number3(10, 20, 30)
	number3 = -number3
	if number3.X ~= -10 then
		return "number3.X ~= -10"
	end
	if number3.Y ~= -20 then
		return "number3.Y ~= -20"
	end
	if number3.Z ~= -30 then
		return "number3.Z ~= -30"
	end

	number3 = Number3(10, 20, 30)
	number3 = number3 - Number3(1, 1, 1)
	if number3.X ~= 9 then
		return "number3.X ~= 9"
	end
	if number3.Y ~= 19 then
		return "number3.Y ~= 19"
	end
	if number3.Z ~= 29 then
		return "number3.Z ~= 29"
	end

	number3 = Number3(10.125, 20.25, 30.5)
	number3 = number3 - Number3(1, 1, 1)
	if number3.X ~= 9.125 then
		return "number3.X ~= 9.125"
	end
	if number3.Y ~= 19.25 then
		return "number3.Y ~= 19.25"
	end
	if number3.Z ~= 29.5 then
		return "number3.Z ~= 29.5"
	end

	number3 = Number3(10, 20, 30)
	number3 = number3 / 2
	if number3.X ~= 5 then
		return "number3.X ~= 5"
	end
	if number3.Y ~= 10 then
		return "number3.Y ~= 10"
	end
	if number3.Z ~= 15 then
		return "number3.Z ~= 15"
	end

	number3 = number3 ^ 2
	if number3.X ~= 25 then
		return "number3.X ~= 25"
	end
	if number3.Y ~= 100 then
		return "number3.Y ~= 100"
	end
	if number3.Z ~= 225 then
		return "number3.Z ~= 225"
	end

	number3 = Number3(10, 10, 10)
	number3 = number3 // 3
	if number3.X ~= 3 then
		return "number3.X ~= 3"
	end
	if number3.Y ~= 3 then
		return "number3.Y ~= 3"
	end
	if number3.Z ~= 3 then
		return "number3.Z ~= 3"
	end

	number3 = Number3(10, 20, 30)
	number3 = number3 * 2
	if number3.X ~= 20 then
		return "number3.X ~= 20"
	end
	if number3.Y ~= 40 then
		return "number3.Y ~= 40"
	end
	if number3.Z ~= 60 then
		return "number3.Z ~= 60"
	end

	number3 = Number3(10, 20, 30)
	number3 = 2 * number3
	if number3.X ~= 20 then
		return "number3.X ~= 20"
	end
	if number3.Y ~= 40 then
		return "number3.Y ~= 40"
	end
	if number3.Z ~= 60 then
		return "number3.Z ~= 60"
	end

	local vector = Number3(1, 0, 0)
	if pcall(vector.Rotate, Number3(0, math.rad(180), 0)) then
		return "Number3.Rotate did not raise an error (1)"
	end
	if pcall(vector.Rotate, nil, Number3(0, math.rad(180), 0)) then
		return "Number3.Rotate did not raise an error (2)"
	end
	vector:Rotate(Number3(0, math.rad(180), 0))
	if not checkNumber3(vector, -1, 0, 0, epsilon) then
		return "Number3:Rotate"
	end

	-- Rotate
	local vector = Number3(1, 0, 0)
	vector:Rotate(Number3(0, math.rad(90), 0)) -- euler as Number3
	if not checkNumber3(vector, 0, 0, -1, epsilon) then
		return "Number3:Rotate (1)"
	end
	vector:Rotate({ 0, math.rad(90), 0 }) -- euler as table
	if not checkNumber3(vector, -1, 0, 0, epsilon) then
		return "Number3:Rotate (2)"
	end
	vector:Rotate(0, math.rad(90), 0) -- euler as numbers
	if not checkNumber3(vector, 0, 0, 1, epsilon) then
		return "Number3:Rotate (3)"
	end
	vector:Rotate(Rotation(0, math.rad(90), 0)) -- euler as Rotation
	if not checkNumber3(vector, 1, 0, 0, epsilon) then
		return "Number3:Rotate (4)"
	end

	-- Angle
	local v1 = Number3(1, 0, 0)
	local angle = v1:Angle(Number3(0, 0, -1)) -- vector as Number3
	if not numberEqual(angle, math.rad(90), epsilon) then
		return "Number3:Angle (1)"
	end
	angle = v1:Angle({ 0, 0, -1 }) -- vector as table
	if not numberEqual(angle, math.rad(90), epsilon) then
		return "Number3:Angle (2)"
	end
	angle = v1:Angle(0, 0, -1) -- vector as numbers
	if not numberEqual(angle, math.rad(90), epsilon) then
		return "Number3:Angle (3)"
	end

	-- check == operator
	local number3 = Number3(1, 2, 3)
	if not n1 == Number3(1, 2, 3) then
		return "number3 ~= Number3(1,2,3)"
	end

	-- Normalize
	number3 = Number3(10, 0, 0)
	if pcall(number3.Normalize) then
		return "Number3.Normalize did not raise an error"
	end
	number3:Normalize()
	if number3.X ~= 1 then
		return "number3.X ~= 1"
	end

	number3 = Number3(0, 10, 0)
	number3:Normalize()
	if number3.Y ~= 1 then
		return "number3.Y ~= 1"
	end

	number3 = Number3(0, 0, 10)
	number3:Normalize()
	if number3.Z ~= 1 then
		return "number3.Z ~= 1"
	end

	-- element-wise product
	number3 = Number3(1, 1, 1)

	local r = number3 * 2
	if r ~= Number3(2, 2, 2) then
		return "r ~= Number3(2, 2, 2)"
	end
	r = number3 * Number3(2, 2, 2)
	if r ~= Number3(2, 2, 2) then
		return "r ~= Number3(2, 2, 2)"
	end
	r = number3 * Number3(2, 2, 2)
	if r ~= Number3(2, 2, 2) then
		return "r ~= Number3(2, 2, 2)"
	end

	r = number3 / 2
	if r ~= Number3(0.5, 0.5, 0.5) then
		return "r ~= Number3(0.5, 0.5, 0.5)"
	end
	r = number3 / Number3(2, 2, 2)
	if r ~= Number3(0.5, 0.5, 0.5) then
		return "r ~= Number3(0.5, 0.5, 0.5)"
	end
	r = number3 / { 2, 2, 2 }
	if r ~= Number3(0.5, 0.5, 0.5) then
		return "r ~= Number3(0.5, 0.5, 0.5)"
	end

	-- Dot
	number3 = Number3(0, 0, 10)
	if pcall(number3.Dot, { 0, 0, 5 }) then
		return "Number3.Dot did not raise an error (1)"
	end
	if pcall(number3.Dot, nil, { 0, 0, 5 }) then
		return "Number3.Dot did not raise an error(2)"
	end
	local dotProduct = number3:Dot({ 0, 0, 5 })
	if dotProduct ~= 50 then
		return "dotProduct ~= 50 "
	end

	number3 = Number3(0, 0, 10)
	dotProduct = number3:Dot({ 0, 5, 0 })
	if dotProduct ~= 0 then
		return "dotProduct ~= 0 "
	end

	-- Length (get / set)
	number3 = Number3(13, 0, 0)
	if number3.Length ~= 13 then
		return "number3.Length ~= 13"
	end

	number3.Length = 7
	if number3.Length ~= 7 then
		return "number3.Length ~= 7 (" .. number3.Length .. ")"
	end
	if number3.X ~= 7 then
		return "number3.X ~= 7"
	end

	-- SquaredLength (get / set)
	number3 = Number3(2, 2, 2)
	if number3.SquaredLength ~= 12 then
		return "number3.SquaredLength ~= 12"
	end

	number3.SquaredLength = 3

	if numberEqual(number3.SquaredLength, 3, epsilon) == false then
		return "numberEqual(number3.SquaredLength, 3, epsilon) == false"
	end
	if numberEqual(number3.Length, 1.732050, epsilon) == false then
		return "numberEqual(number3.Length, 1.732050, epsilon)"
	end

	-- OnSet callbacks
	local n3 = Number3(0, 0, 0)
	local x1
	local x2
	local fn1 = function(n)
		x1 = n.X
	end
	local fn2 = function(n)
		x2 = n.X
	end

	n3:AddOnSetCallback(fn1)
	n3:AddOnSetCallback(fn2)

	n3.X = 42
	if x1 ~= 42 then
		return "number3:AddOnSetCallback - x1 ~= 42"
	end
	if x2 ~= 42 then
		return "number3:AddOnSetCallback - x2 ~= 42"
	end

	n3:RemoveOnSetCallback(fn1)
	n3.X = 35

	if x1 ~= 42 then
		return "number3:AddOnSetCallback - x1 ~= 42"
	end
	if x2 ~= 35 then
		return "number3:AddOnSetCallback - x2 ~= 35"
	end

	return nil
end

Client.rotationTestSuite = function()
	---- constructor checks

	local rot = Rotation()
	if rot ~= Rotation(0, 0, 0) then
		return "rotation constructor (1)"
	end
	rot = Rotation(0.1, 0.2, 0.3)
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation constructor (2)"
	end
	rot = Rotation({ 0.4, 0.5, 0.6 })
	if rot ~= Rotation(0.4, 0.5, 0.6) then
		return "rotation constructor (3)"
	end
	rot = Rotation(Number3(0.1, 0.2, 0.3))
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation constructor (4)"
	end
	rot = Rotation({ 1.0, 0.0, 0.0 }, 0.3)
	if rot ~= Rotation(0.3, 0.0, 0.0) then
		return "rotation constructor (5)"
	end
	rot = Rotation(Number3(0.0, 1.0, 0.0), 0.3)
	if rot ~= Rotation(0.0, 0.3, 0.0) then
		return "rotation constructor (6)"
	end
	rot = Rotation({ 0.0, 0.0, 1.0 }, 0.3)
	if rot ~= Rotation(0.0, 0.0, 0.3) then
		return "rotation constructor (7)"
	end

	---- fields type checks

	if not checkType(rot.X, "number") then
		return "Rotation.X"
	end
	if not checkType(rot.Y, "number") then
		return "Rotation.Y"
	end
	if not checkType(rot.Z, "number") then
		return "Rotation.Z"
	end
	if not checkType(rot.Copy, "function") then
		return "Rotation:Copy"
	end
	if not checkType(rot.Inverse, "function") then
		return "Rotation:Inverse"
	end
	if not checkType(rot.Lerp, "function") then
		return "Rotation:Lerp"
	end
	if not checkType(rot.Slerp, "function") then
		return "Rotation:Slerp"
	end
	if not checkType(rot.Angle, "function") then
		return "Rotation:Angle"
	end
	if not checkType(rot.Set, "function") then
		return "Rotation:Set"
	end
	if not checkType(rot.SetAxisAngle, "function") then
		return "Rotation:SetAxisAngle"
	end
	if not checkType(rot.SetLookRotation, "function") then
		return "Rotation:SetLookRotation"
	end
	if not checkType(rot.SetFromToRotation, "function") then
		return "Rotation:SetFromToRotation"
	end

	---- fields setter/getter checks

	rot.X = 0.4
	if not numberEqual(rot.X, 0.4, radEpsilon) then
		return "rotation.X ~= 0.4"
	end
	rot.Y = 0.5
	if not numberEqual(rot.Y, 0.5, radEpsilon) then
		return "rotation.Y ~= 0.5"
	end
	rot.Z = 0.6
	if not numberEqual(rot.Z, 0.6, radEpsilon) then
		return "rotation.Z ~= 0.6"
	end

	---- arithmetic checks

	rot = Rotation(0.1, 0.2, 0.3)
	if not numberEqual(rot.X, 0.1, radEpsilon) then
		return "rotation.X ~= 0.1"
	end
	if not numberEqual(rot.Y, 0.2, radEpsilon) then
		return "rotation.Y ~= 0.2"
	end
	if not numberEqual(rot.Z, 0.3, radEpsilon) then
		return "rotation.Z ~= 0.3"
	end
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation ~= Rotation(0.1, 0.2, 0.3)"
	end
	if rot ~= Rotation(Number3(0.1, 0.2, 0.3)) then
		return "rotation ~= Rotation(Number3(0.1, 0.2, 0.3))"
	end
	if rot ~= Rotation(0.1 + 2 * math.pi, 0.2, 0.3) then
		return "rotation ~= Rotation(0.1 + 2 * math.pi, 0.2, 0.3)"
	end

	rot = Rotation(0.1, 0.0, 0.0)
	if rot * Rotation(0.2, 0.0, 0.0) ~= Rotation(0.3, 0.0, 0.0) then
		return "rotation * (1)"
	end
	if rot * Rotation(3 * math.pi, 0.0, 0.0) ~= Rotation(0.1 + math.pi, 0.0, 0.0) then
		return "rotation * (2)"
	end
	if rot * Rotation(0.0, 0.2, 0.0) == Rotation(0.0, 0.2, 0.0) * rot then
		return "rotation * (3)"
	end
	if rot * { 0.0, 0.2, 0.0 } == Rotation(0.0, 0.2, 0.0) * rot then
		return "rotation * (4)"
	end
	if rot * Rotation(-0.1, 0.0, 0.0) ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation * (5)"
	end

	rot = Rotation(0.4, 0.5, 0.6)
	rot2 = rot:Copy()
	rot2:Inverse()
	if -rot ~= rot2 then
		return "-rotation"
	end

	rot = Rotation(0.1, 0.0, 0.0)
	if rot + Rotation(0.1, 0.0, 0.0) ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation + (1)"
	end
	if rot + Number3(0.1, 0.0, 0.0) ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation + (2)"
	end
	if rot + Rotation(0.1, 0.0, 0.0) ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation + (3)"
	end

	rot = Rotation(0.2, 0.0, 0.0)
	if rot - Rotation(0.1, 0.0, 0.0) ~= Rotation(0.1, 0.0, 0.0) then
		return "rotation - (1)"
	end
	if rot - Number3(0.1, 0.0, 0.0) ~= Rotation(0.1, 0.0, 0.0) then
		return "rotation - (2)"
	end
	if rot - Rotation(0.1, 0.0, 0.0) ~= Rotation(0.1, 0.0, 0.0) then
		return "rotation - (3)"
	end

	---- functional checks

	if rot ~= rot:Copy() then
		return "rotation:Copy()"
	end

	rot = Rotation(0.1, 0.2, 0.3)
	local rot2 = rot:Copy()
	rot2:Inverse()
	if rot2 * rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:Inverse() (1)"
	end
	if rot2 * rot * Rotation(0.4, 0.0, 0.0) ~= Rotation(0.4, 0.0, 0.0) then
		return "rotation:Inverse() (2)"
	end

	rot:Lerp(Rotation(0.1, 0.2, 0.3), Rotation(0.5, 0.6, 0.7), 0.0)
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation:Lerp() (1)"
	end
	rot:Lerp(Rotation(0.1, 0.2, 0.3), Rotation(0.5, 0.6, 0.7), 1.0)
	if rot ~= Rotation(0.5, 0.6, 0.7) then
		return "rotation:Lerp() (2)"
	end
	rot:Lerp(Rotation(0.1, 0.0, 0.0), Rotation(0.3, 0.0, 0.0), 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Lerp() (3)"
	end
	rot:Lerp({ 0.1, 0.0, 0.0 }, Rotation(0.3, 0.0, 0.0), 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Lerp() (4)"
	end
	rot:Lerp(Rotation(0.1, 0.0, 0.0), { 0.3, 0.0, 0.0 }, 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Lerp() (5)"
	end
	rot:Lerp({ 0.1, 0.0, 0.0 }, { 0.3, 0.0, 0.0 }, 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Lerp() (6)"
	end

	rot:Slerp(Rotation(0.1, 0.2, 0.3), Rotation(0.5, 0.6, 0.7), 0.0)
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation:Slerp() (1)"
	end
	rot:Slerp(Rotation(0.1, 0.2, 0.3), Rotation(0.5, 0.6, 0.7), 1.0)
	if rot ~= Rotation(0.5, 0.6, 0.7) then
		return "rotation:Slerp() (2)"
	end
	rot:Slerp(Rotation(0.1, 0.0, 0.0), Rotation(0.3, 0.0, 0.0), 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Slerp() (3)"
	end
	rot:Slerp({ 0.1, 0.0, 0.0 }, Rotation(0.3, 0.0, 0.0), 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Slerp() (4)"
	end
	rot:Slerp(Rotation(0.1, 0.0, 0.0), { 0.3, 0.0, 0.0 }, 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Slerp() (5)"
	end
	rot:Slerp({ 0.1, 0.0, 0.0 }, { 0.3, 0.0, 0.0 }, 0.5)
	if rot ~= Rotation(0.2, 0.0, 0.0) then
		return "rotation:Slerp() (6)"
	end

	rot = Rotation(0.0, 0.8, 0.0)
	if not numberEqual(rot:Angle({ 0.0, 1.0, 0.0 }), 0.2, radEpsilon) then
		return "rotation:Angle() (1)"
	end
	if not numberEqual(rot:Angle({ 0.0, -0.2, 0.0 }), 1.0, radEpsilon) then
		return "rotation:Angle() (2)"
	end
	if not numberEqual(rot:Angle(Rotation(0.0, 1.0, 0.0)), 0.2, radEpsilon) then
		return "rotation:Angle() (3)"
	end

	rot:Set(0.1, 0.2, 0.3)
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation:Set() (1)"
	end
	rot:Set({ 0.4, 0.5, 0.6 })
	if rot ~= Rotation(0.4, 0.5, 0.6) then
		return "rotation:Set() (2)"
	end
	rot:Set(Number3(0.1, 0.2, 0.3))
	if rot ~= Rotation(0.1, 0.2, 0.3) then
		return "rotation:Set() (3)"
	end

	rot:SetAxisAngle({ 1.0, 0.0, 0.0 }, 0.3)
	if rot ~= Rotation(0.3, 0.0, 0.0) then
		return "rotation:SetAxisAngle() (1)"
	end
	rot:SetAxisAngle(Number3(0.0, 1.0, 0.0), 0.3)
	if rot ~= Rotation(0.0, 0.3, 0.0) then
		return "rotation:SetAxisAngle() (2)"
	end
	rot:SetAxisAngle({ 0.0, 0.0, 1.0 }, 0.3)
	if rot ~= Rotation(0.0, 0.0, 0.3) then
		return "rotation:SetAxisAngle() (3)"
	end

	rot:SetLookRotation(Number3(0.0, 0.0, 1.0))
	if rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:SetLookRotation() (1)"
	end
	rot:SetLookRotation({ 0.0, 0.0, 1.0 })
	if rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:SetLookRotation() (2)"
	end
	rot:SetLookRotation({ 0.0, 0.0, -1.0 })
	if rot ~= Rotation(0.0, math.pi, 0.0) then
		return "rotation:SetLookRotation() (3)"
	end
	rot:SetLookRotation({ 1.0, 0.0, 0.0 })
	if rot ~= Rotation(0.0, math.pi * 0.5, 0.0) then
		return "rotation:SetLookRotation() (4)"
	end

	rot:SetFromToRotation(Number3(0.0, 0.0, 1.0), Number3(0.0, 0.0, 1.0))
	if rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:SetFromToRotation() (1)"
	end
	rot:SetFromToRotation({ 0.0, 0.0, 1.0 }, Number3(0.0, 0.0, 1.0))
	if rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:SetFromToRotation() (2)"
	end
	rot:SetFromToRotation({ 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 })
	if rot ~= Rotation(0.0, 0.0, 0.0) then
		return "rotation:SetFromToRotation() (3)"
	end
	rot:SetFromToRotation({ 0.0, 0.0, 1.0 }, { 0.0, 0.0, -1.0 })
	if rot ~= Rotation(0.0, math.pi, 0.0) then
		return "rotation:SetFromToRotation() (4)"
	end

	-- OnSet callbacks
	local rot = Rotation(0, 0, 0)
	local x1
	local x2
	local fn1 = function(r)
		x1 = r.X
	end
	local fn2 = function(r)
		x2 = r.X
	end

	rot:AddOnSetCallback(fn1)
	rot:AddOnSetCallback(fn2)

	rot.X = 0.1
	if not numberEqual(x1, 0.1, epsilon) then
		return "rotation:AddOnSetCallback - x1 ~= 0.1 (1) -> " .. x1
	end
	if not numberEqual(x2, 0.1, epsilon) then
		return "rotation:AddOnSetCallback - x2 ~= 0.1 -> " .. x2
	end

	rot:RemoveOnSetCallback(fn1)
	rot.X = 0.2

	if not numberEqual(x1, 0.1, epsilon) then
		return "rotation:AddOnSetCallback - x1 ~= 0.1 (2) -> " .. x1
	end
	if not numberEqual(x2, 0.2, epsilon) then
		return "rotation:AddOnSetCallback - x2 ~= 0.2 -> " .. x2
	end

	return nil
end

checkType = function(value, t)
	return value ~= nil and typeof(value) == t
end

-- check Number3 equality w/ epsilon
-- some transform computations can result in small inaccuracies (float errors adding up)
-- engine side, we consider zero-epsilon to be 1e-5 for general purposes
-- for radians, we may consider zero-epsilon to be 1e-3
epsilon = 0.00001
radEpsilon = 0.000001
checkNumber3 = function(n3, x, y, z, epsilon)
	local fx = math.abs(n3.X - x)
	local fy = math.abs(n3.Y - y)
	local fz = math.abs(n3.Z - z)
	return fx < epsilon and fy < epsilon and fz < epsilon
end

function numberEqual(n1, n2, epsilon)
	return math.abs(n1 - n2) < epsilon
end

Client.objectSupportTestSuite = function(object)
	---- fields type checks

	if not checkType(object.Acceleration, "Number3") then
		return "Object.Acceleration"
	end
	if not checkType(object.Position, "Number3") then
		return "Object.Position"
	end
	if not checkType(object.LocalPosition, "Number3") then
		return "Object.LocalPosition"
	end
	if not checkType(object.Rotation, Type.Rotation) then
		return "Object.Rotation"
	end
	if not checkType(object.LocalRotation, Type.Rotation) then
		return "Object.LocalRotation"
	end
	if not checkType(object.LocalScale, "Number3") then
		return "Object.LocalScale"
	end
	if not checkType(object.LossyScale, "Number3") then
		return "Object.LossyScale"
	end
	if not checkType(object.SetParent, "function") then
		return "Object:SetParent"
	end
	if not checkType(object.RemoveFromParent, "function") then
		return "Object:RemoveFromParent"
	end
	if not checkType(object.AddChild, "function") then
		return "Object:AddChild"
	end
	if not checkType(object.RemoveChild, "function") then
		return "Object:RemoveChild"
	end
	if not checkType(object.RemoveChildren, "function") then
		return "Object:RemoveChildren"
	end
	if not checkType(object.GetChild, "function") then
		return "Object:GetChild"
	end
	if not checkType(object.ChildrenCount, "number") then
		return "Object.ChildrenCount"
	end
	if not checkType(object.PositionLocalToWorld, "function") then
		return "Object:PositionLocalToWorld"
	end
	if not checkType(object.PositionWorldToLocal, "function") then
		return "Object:PositionWorldToLocal"
	end
	if not checkType(object.RotationLocalToWorld, "function") then
		return "Object:RotationLocalToWorld"
	end
	if not checkType(object.RotationWorldToLocal, "function") then
		return "Object:RotationWorldToLocal"
	end
	if not checkType(object.RotateLocal, "function") then
		return "Object:RotateLocal"
	end
	if not checkType(object.RotateWorld, "function") then
		return "Object:RotateWorld"
	end
	if not checkType(object.Right, "Number3") then
		return "Object.Right"
	end
	if not checkType(object.Up, "Number3") then
		return "Object.Up"
	end
	if not checkType(object.Forward, "Number3") then
		return "Object.Forward"
	end
	if not checkType(object.Left, "Number3") then
		return "Object.Left"
	end
	if not checkType(object.Down, "Number3") then
		return "Object.Down"
	end
	if not checkType(object.Backward, "Number3") then
		return "Object.Backward"
	end
	if not checkType(object.Physics, Type.PhysicsMode) then
		return "Object.Physics"
	end
	if not checkType(object.Velocity, "Number3") then
		return "Object.Velocity"
	end
	if not checkType(object.Motion, "Number3") then
		return "Object.Motion"
	end
	if not checkType(object.IsHidden, "boolean") then
		return "Object.IsHidden"
	end
	if not checkType(object.IsOnGround, "boolean") then
		return "Object.IsOnGround"
	end
	if not checkType(object.CollisionGroups, Type.CollisionGroups) then
		return "Object.CollisionGroups"
	end
	if not checkType(object.CollisionGroupsMask, "number") then
		return "Object.CollisionGroupsMask"
	end
	if not checkType(object.CollidesWithGroups, Type.CollisionGroups) then
		return "Object.CollidesWithGroups"
	end
	if not checkType(object.CollidesWithMask, "number") then
		return "Object.CollidesWithMask"
	end
	if not checkType(object.CollidesWith, "function") then
		return "Object:CollidesWith"
	end

	local o1 = Object()
	local o2 = Object()
	if pcall(o1.SetParent, nil, o2) then
		return "Object.SetParent did not raise an error"
	end
	if pcall(o1.RemoveFromParent) then
		return "Object.RemoveFromParent did not raise an error"
	end
	if pcall(o2.AddChild, nil, o1) then
		return "Object.AddChild did not raise an error"
	end
	if pcall(o2.RemoveChild, nil, o1) then
		return "Object.RemoveChild did not raise an error"
	end
	if pcall(o2.RemoveChildren) then
		return "Object.RemoveChildren did not rais an error"
	end

	object.Position = { 0, 0, 0 }
	object.Rotation = { 0, 0, 0 }
	object.LocalScale = 1

	---- functional checks

	-- add child
	local child1 = Object(Items.pen)
	local child2 = Object(Items.pen)
	local child3 = Object(Items.pen)
	local child4 = Object(Items.pen)
	object:AddChild(child1)
	object:AddChild(child2)
	object:AddChild(child3)
	object:AddChild(child4)
	if object.ChildrenCount ~= 4 then
		return "object:AddChild"
	end
	if typeof(object.ChildrenCount) ~= "number" then
		return 'typeof(object.ChildrenCount) ~= "number"'
	end

	-- get child
	child1.Position = Number3(10, -9, 2)
	local getChild1 = object:GetChild(1)
	if pcall(object.GetChild, 1) then
		return "Object.GetChild did not raise an error (1)"
	end
	if pcall(object.GetChild, nil, 1) then
		return "Object.GetChild did not raise an error (2)"
	end
	if not checkNumber3(getChild1.Position, 10, -9, 2, epsilon) then
		return "Object:GetChild"
	end

	-- remove child
	object:RemoveChild(2)
	object:RemoveChild(child4)
	if object.ChildrenCount ~= 2 then
		return "object:RemoveChild"
	end

	-- set parent
	child4:SetParent(object)
	if object.ChildrenCount ~= 3 then
		return "object:SetParent"
	end

	-- remove parent
	child4:RemoveFromParent()
	if object.ChildrenCount ~= 2 then
		return "object:RemoveFromParent"
	end

	-- remove children
	object:RemoveChildren()
	if object.ChildrenCount ~= 0 then
		return "object:RemoveChildren"
	end

	-- position conversion
	object.LocalPosition = Number3(61, 40, 60)
	local pos_ltw = object:PositionLocalToWorld(Number3(10, -2, 0))
	if pcall(object.PositionLocalToWorld, Number3(10, -2, 0)) then
		return "Object.PositionLocalToWorld did not raise an error (1)"
	end
	if pcall(object.PositionLocalToWorld, nil, Number3(10, -2, 0)) then
		return "Object.PositionLocalToWorld did not raise an error (2)"
	end
	local pos_wtl = object:PositionWorldToLocal(Number3(10 * Map.Scale.X, -2 * Map.Scale.Y, 0))
	if pcall(object.PositionWorldToLocal, Number3(10 * Map.Scale.X, -2 * Map.Scale.Y, 0)) then
		return "Object.PositionWorldToLocal did not raise an error (1)"
	end
	if pcall(object.PositionWorldToLocal, nil, Number3(10 * Map.Scale.X, -2 * Map.Scale.Y, 0)) then
		return "Object.PositionWorldToLocal did not raise an error (2)"
	end
	if not checkNumber3(pos_ltw, 71 * Map.Scale.X, 38 * Map.Scale.Y, 60 * Map.Scale.Z, epsilon) then
		return "Object:PositionLocalToWorld"
	end
	if not checkNumber3(pos_wtl, -51, -42, -60, epsilon) then
		return "Object:PositionWorldToLocal"
	end

	-- rotation conversion
	if pcall(object.RotationLocalToWorld, rot) then
		return "Object.RotationLocalToWorld did not raise an error (1)"
	end
	if pcall(object.RotationLocalToWorld, nil, rot) then
		return "Object.RotationLocalToWorld did not raise an error (2)"
	end
	if pcall(object.RotationWorldToLocal, rot) then
		return "Object.RotationWorldToLocal did not raise an error (1)"
	end
	if pcall(object.RotationWorldToLocal, nil, rot) then
		return "Object.RotationWorldToLocal did not raise an error (2)"
	end

	object.Rotation = { 0, math.rad(45), 0 }
	local rot = Number3(0, math.rad(45), 0) -- euler as Number3
	if object:RotationLocalToWorld(rot) ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotationLocalToWorld (1)"
	end
	if object:RotationWorldToLocal(rot) ~= Rotation(0, 0, 0) then
		return "Object:RotationWorldToLocal (1)"
	end
	rot = { 0, math.rad(45), 0 } -- euler as table
	if object:RotationLocalToWorld(rot) ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotationLocalToWorld (2)"
	end
	if object:RotationWorldToLocal(rot) ~= Rotation(0, 0, 0) then
		return "Object:RotationWorldToLocal (2)"
	end
	-- euler as numbers
	if object:RotationLocalToWorld(0, math.rad(45), 0) ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotationLocalToWorld (3)"
	end
	if object:RotationWorldToLocal(0, math.rad(45), 0) ~= Rotation(0, 0, 0) then
		return "Object:RotationWorldToLocal (3)"
	end
	rot = Rotation(0, math.rad(45), 0) -- euler as Rotation
	if object:RotationLocalToWorld(rot) ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotationLocalToWorld (4)"
	end
	if object:RotationWorldToLocal(rot) ~= Rotation(0, 0, 0) then
		return "Object:RotationWorldToLocal (4)"
	end

	-- rotate
	if pcall(object.RotateWorld, rot) then
		return "Object.RotateWorld did not raise an error (1)"
	end
	if pcall(object.RotateWorld, nil, rot) then
		return "Object.RotateWorld did not raise an error (2)"
	end
	if pcall(object.RotateLocal, rot) then
		return "Object.RotateLocal did not raise an error (1)"
	end
	if pcall(object.RotateLocal, nil, rot) then
		return "Object.RotateLocal did not raise an error (2)"
	end

	object.Rotation = { 0, 0, 0 }
	object:RotateWorld(Number3(0, math.rad(45), 0)) -- euler as Number3
	if object.Rotation ~= Rotation(0, math.rad(45), 0) then
		return "Object:RotateWorld (1)"
	end
	object:RotateWorld({ 0, math.rad(45), 0 }) -- euler as table
	if object.Rotation ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotateWorld (2)"
	end
	object:RotateWorld(0, math.rad(45), 0) -- euler as numbers
	if object.Rotation ~= Rotation(0, math.rad(135), 0) then
		return "Object:RotateWorld (3)"
	end
	object:RotateWorld(Rotation(0, math.rad(45), 0)) -- euler as Rotation
	if object.Rotation ~= Rotation(0, math.rad(180), 0) then
		return "Object:RotateWorld (4)"
	end
	object:RotateWorld(Number3(0, 1, 0), math.rad(45)) -- axis/angle as Number3
	if object.Rotation ~= Rotation(0, math.rad(225), 0) then
		return "Object:RotateWorld (5)"
	end
	object:RotateWorld({ 0, 1, 0 }, math.rad(45)) -- axis/angle as table
	if object.Rotation ~= Rotation(0, math.rad(270), 0) then
		return "Object:RotateWorld (6)"
	end
	object:RotateWorld(0, 1, 0, math.rad(45)) -- axis/angle as numbers
	if object.Rotation ~= Rotation(0, math.rad(315), 0) then
		return "Object:RotateWorld (7)"
	end

	object.Rotation = { 0, 0, 0 }
	object:RotateLocal(Number3(0, math.rad(45), 0)) -- euler as Number3
	if object.LocalRotation ~= Rotation(0, math.rad(45), 0) then
		return "Object:RotateLocal (1)"
	end
	object:RotateLocal({ 0, math.rad(45), 0 }) -- euler as table
	if object.LocalRotation ~= Rotation(0, math.rad(90), 0) then
		return "Object:RotateLocal (2)"
	end
	object:RotateLocal(0, math.rad(45), 0) -- euler as numbers
	if object.LocalRotation ~= Rotation(0, math.rad(135), 0) then
		return "Object:RotateLocal (3)"
	end
	object:RotateLocal(Rotation(0, math.rad(45), 0)) -- euler as Rotation
	if object.LocalRotation ~= Rotation(0, math.rad(180), 0) then
		return "Object:RotateLocal (4)"
	end
	object:RotateLocal(Number3(0, 1, 0), math.rad(45)) -- axis/angle as Number3
	if object.LocalRotation ~= Rotation(0, math.rad(225), 0) then
		return "Object:RotateLocal (5)"
	end
	object:RotateLocal({ 0, 1, 0 }, math.rad(45)) -- axis/angle as table
	if object.LocalRotation ~= Rotation(0, math.rad(270), 0) then
		return "Object:RotateLocal (6)"
	end
	object:RotateLocal(0, 1, 0, math.rad(45)) -- axis/angle as numbers
	if object.LocalRotation ~= Rotation(0, math.rad(315), 0) then
		return "Object:RotateLocal (7)"
	end

	-- unit vectors
	object.Right = Number3(1, 0, 0)
	if not checkNumber3(object.Rotation, 0, 0, 0, radEpsilon) then
		return "Object.Right"
	end
	object.Up = Number3(0, 1, 0)
	if not checkNumber3(object.Rotation, 0, 0, 0, radEpsilon) then
		return "Object.Up"
	end
	object.Forward = Number3(0, 0, 1)
	if not checkNumber3(object.Rotation, 0, 0, 0, radEpsilon) then
		return "Object.Forward"
	end

	-- reverse unit vectors
	if not checkNumber3(object.Left, -object.Right.X, -object.Right.Y, -object.Right.Z, epsilon) then
		return "Object.Left"
	end
	if not checkNumber3(object.Down, -object.Up.X, -object.Up.Y, -object.Up.Z, epsilon) then
		return "Object.Down"
	end
	if not checkNumber3(object.Backward, -object.Forward.X, -object.Forward.Y, -object.Forward.Z, epsilon) then
		return "Object.Backward"
	end

	-- local & lossy scale
	object.LocalScale = 3
	if not checkNumber3(object.LocalScale, 3, 3, 3, epsilon) then
		return "Object.LocalScale (uniform setter)"
	end

	object.LocalScale = Number3(1, 2, 3)
	if not checkNumber3(object.LocalScale, 1, 2, 3, epsilon) then
		return "Object.LocalScale (setter)"
	end

	object.LocalScale = { 4, 5, 6 }
	if not checkNumber3(object.LocalScale, 4, 5, 6, epsilon) then
		return "Object.LocalScale (table setter)"
	end

	object.LocalScale.X = 7
	object.LocalScale.Y = 8
	object.LocalScale.Z = 9
	if not checkNumber3(object.LocalScale, 7, 8, 9, epsilon) then
		return "Object.LocalScale (individual setter)"
	end

	object.LocalScale = Number3(3, 2, 4)
	if not checkNumber3(object.LossyScale, 15, 10, 20, epsilon) then
		return "Object.LossyScale"
	end

	-- collision masks (most but not all objects have a rigidbody eg. Camera)
	object.CollisionGroupsMask = 3
	if object.CollisionGroupsMask == 3 then
		object.CollidesWithMask = 0
		Player.CollisionGroupsMask = 2
		Player.CollidesWithMask = 0
		if not object:CollidesWith(Player) == false then
			return "Object:CollidesWith (1)"
		end
		Player.CollidesWithMask = 3
		if not object:CollidesWith(Player) then
			return "Object:CollidesWith (2)"
		end
		object.CollidesWithMask = 2
		Player.CollidesWithMask = 0
		if not object:CollidesWith(Player) then
			return "Object:CollidesWith (3)"
		end

		-- set groups and check masks
		object.CollisionGroups = 1
		if object.CollisionGroupsMask ~= 1 then
			return "object.CollisionGroupsMask ~= 1"
		end
		object.CollisionGroups = { 1 }
		if object.CollisionGroupsMask ~= 1 then
			return "object.CollisionGroupsMask ~= 1"
		end
		object.CollisionGroups = { 1, 2 }
		if object.CollisionGroupsMask ~= 3 then
			return "object.CollisionGroupsMask ~= 3"
		end
		object.CollisionGroups = { 2, 3 }
		if object.CollisionGroupsMask ~= 6 then
			return "object.CollisionGroupsMask ~= 6"
		end
		object.CollisionGroups = { 2, 4, 3 }
		if object.CollisionGroupsMask ~= 14 then
			return "object.CollisionGroupsMask ~= 14"
		end
		object.CollisionGroups = { 1, 2, 3, 4, 5, 6, 7, 8 }
		if object.CollisionGroupsMask ~= 255 then
			return "object.CollisionGroupsMask ~= 255"
		end

		object.CollidesWithGroups = 1
		if object.CollidesWithMask ~= 1 then
			return "object.CollidesWithMask ~= 1"
		end
		object.CollidesWithGroups = { 1 }
		if object.CollidesWithMask ~= 1 then
			return "object.CollidesWithMask ~= 1"
		end
		object.CollidesWithGroups = { 1, 2 }
		if object.CollidesWithMask ~= 3 then
			return "object.CollidesWithMask ~= 3"
		end
		object.CollidesWithGroups = { 2, 3 }
		if object.CollidesWithMask ~= 6 then
			return "object.CollidesWithMask ~= 6"
		end
		object.CollidesWithGroups = { 2, 4, 3 }
		if object.CollidesWithMask ~= 14 then
			return "object.CollidesWithMask ~= 14"
		end
		object.CollidesWithGroups = { 1, 2, 3, 4, 5, 6, 7, 8 }
		if object.CollidesWithMask ~= 255 then
			return "object.CollidesWithMask ~= 255"
		end
	end

	return nil
end

Client.objectTestSuite = function()
	if Object == nil then
		return "Object == nil"
	end

	local object = Object(Items.pen)
	Map:AddChild(object)
	if not checkType(object, Type.Object) then
		return "Object: incorrect type"
	end

	return Client.objectSupportTestSuite(object)
end

Client.paletteTestSuite = function()
	if typeof(#Map.Palette) ~= "number" then
		return 'typeof(#Map.Palette) ~= "number"'
	end
	if typeof(Map.Palette) ~= Type.Palette then
		return 'typeof(Map.Palette) ~= "Palette"'
	end

	for i = 1, #Map.Palette do
		if typeof(Map.Palette[i]) ~= Type.BlockProperties then
			return 'typeof(Map.Palette[" .. i .. "]) ~= "BlockProperties"'
		end
		if typeof(Map.Palette[i].ID) ~= "number" then
			return 'typeof(Map.Palette[" .. i .. "].ID) ~= "number"'
		end
		if typeof(Map.Palette[i].Color) ~= Type.Color then
			return 'typeof(Map.Palette[" .. i .. "].Color) ~= "Color"'
		end
		if typeof(Map.Palette[i].Light) ~= "boolean" then
			return 'typeof(Map.Palette[" .. i .. "].Light) ~= "boolean"'
		end
	end

	local n = #Map.Palette
	local c = Color(1.0, 0.9, 0.7, 0.5)

	local ok = pcall(Map.Palette.AddColor, nil, c)
	if ok then
		return "Palette.AddColor did not raise an error"
	end
	ok = pcall(Map.Palette.RemoveColor, nil, 1)
	if ok then
		return "Palette.RemoveColor did not raise an error"
	end
	oks = pcall(Map.Palette.GetIndex, nil, c)
	if ok then
		return "Palette.GetIndex did not raise an error"
	end

	-- new color
	if Map.Palette:AddColor(c) ~= n + 1 then
		return "Palette:AddColor (1)"
	end
	if #Map.Palette ~= n + 1 then
		return "Palette:AddColor (2)"
	end

	if Map.Palette:GetIndex(c) ~= n + 1 then
		return "Palette:GetIndex"
	end

	if not Map.Palette:RemoveColor(n + 1) then
		return "Palette:RemoveColor (1)"
	end
	if #Map.Palette ~= n then
		return "Palette:RemoveColor (2)"
	end

	-- standalone palette
	local standalonePalette = Palette()
	if typeof(standalonePalette) ~= Type.Palette then
		return 'typeof(standalonePalette) ~= "Palette"'
	end
	if #standalonePalette ~= 0 then
		return "#standalonePalette ~= 0"
	end
	if standalonePalette:AddColor(c) ~= 1 then
		return "standalonePalette:AddColor(c)"
	end

	-- shared palette
	local s1 = Shape(Items.pen)
	local s2 = Shape(Items.pen)
	s1.Palette[1].Color = Color.Red
	s2.Palette[1].Color = Color.Green
	if s1.Palette[1].Color == s2.Palette[1].Color then
		return "s1.Palette[1].Color == s2.Palette[1].Color"
	end
	s1.Palette = s2.Palette
	if s1.Palette[1].Color ~= s2.Palette[1].Color then
		return "s1.Palette[1].Color ~= s2.Palette[1].Color"
	end

	-- copy
	local copyPalette = standalonePalette:Copy()
	if typeof(copyPalette) ~= Type.Palette then
		return 'typeof(copyPalette) ~= "Palette"'
	end
	if #copyPalette ~= #standalonePalette then
		return "#copyPalette ~= #standalonePalette (1)"
	end
	if copyPalette[1].Color ~= standalonePalette[1].Color then
		return "copyPalette[1].Color ~= standalonePalette[1].Color"
	end

	-- merge
	copyPalette:AddColor(Color.Red)
	local count = #standalonePalette
	standalonePalette:Merge(copyPalette)
	if #standalonePalette ~= count + 1 then
		return "#standalonePalette ~= count + 1"
	end
	standalonePalette:Merge(copyPalette, { duplicates = true })
	if #standalonePalette ~= count + 1 + #copyPalette then
		return "#standalonePalette ~= count + #copyPalette"
	end

	-- reduce
	Map.Palette:Reduce()
	count = #Map.Palette
	local idx = Map.Palette:AddColor(Color.Red)
	if Map.Palette[idx].BlocksCount ~= 0 then
		return "Map.Palette:GetColor(idx).BlocksCount ~= 0"
	end
	Map.Palette:Reduce()
	if #Map.Palette ~= count then
		return "#Map.Palette ~= count"
	end

	return nil
end

Client.pointerTestSuite = function()
	if Pointer == nil then
		return "Pointer == nil"
	end

	if typeof(Pointer.Hide) ~= "function" then
		return 'typeof(Pointer.Hide) ~= "function"'
	end
	Pointer:Hide()
	if typeof(Pointer.Show) ~= "function" then
		return 'typeof(Pointer.Show) ~= "function"'
	end
	Pointer:Show()

	local f1 = function()
		print("pointer")
	end
	local f2 = function(pointerEvent)
		print(pointerEvent.DX, pointerEvent.DY)
	end
	local f3 = function(a)
		print(a)
	end

	Pointer.Drag = f2
	if Pointer.Drag ~= f2 then
		return "Pointer.Drag update failed"
	end
	Pointer.Drag2 = f2
	if Pointer.Drag2 ~= f2 then
		return "Pointer.Drag2 update failed"
	end
	Pointer.Up = f2
	if Pointer.Up ~= f2 then
		return "Pointer.DidUpHide update failed"
	end
	Pointer.Zoom = f3
	if Pointer.Zoom ~= f3 then
		return "Pointer.Zoom update failed"
	end

	return nil
end

Client.rayTestSuite = function()
	if Ray == nil then
		return "Ray == nil"
	end

	local ray = Ray({ 0, 0, 0 }, { 0, 0, 1 })

	if not checkType(ray.Origin, "Number3") then
		return "Ray.Origin"
	end
	if not checkType(ray.Direction, "Number3") then
		return "Ray.Direction"
	end

	if not ray.Origin == Number3(0, 0, 0) then
		return "not ray.Origin == Number3(0,0,0)"
	end
	ray.Origin = { 1, 1, 1 }
	if not ray.Origin == Number3(1, 1, 1) then
		return "not ray.Origin == Number3(1,1,1)"
	end

	if not ray.Direction == Number3(0, 0, 1) then
		return "not ray.Direction == Number3(0,0,1)"
	end
	ray.Direction = { 1, 2, 3 }
	if not ray.Direction == Number3(1, 2, 3) then
		return "not ray.Direction == Number3(1,2,3)"
	end

	-- The following works in a normal world, not in CI, ever since map is a generic shape physics-wise (in rtree),
	-- need to check if a call to tick() can be added to build a rtree
	--[[
	ray.Origin = Number3(0.5, 0.5, -2)
	ray.Direction = Number3(0, 0, 1)
	local impact = ray:Cast()

	if impact == nil then return "impact == nil" end
	if impact.Block == nil then return "impact.Block == nil" end
	if impact.Block.PaletteIndex ~= 1 then return "impact.Block.PaletteIndex ~= 1" end
	]]

	-- test FilterIn

	ray.FilterIn = 1 -- group number
	-- if not checkType(ray.FilterIn, Type.CollisionGroups) then return "ray.FilterIn is not CollisionGroups" end
	ray.FilterIn = { 1, 2 } -- group numbers in array
	if not checkType(ray.FilterIn, Type.CollisionGroups) then
		return "ray.FilterIn is not CollisionGroups"
	end
	ray.FilterIn = CollisionGroups(1, 2, 3)
	if not checkType(ray.FilterIn, Type.CollisionGroups) then
		return "ray.FilterIn is not CollisionGroups"
	end
	local shape = Shape(Items.pen)
	ray.FilterIn = shape
	if not checkType(ray.FilterIn, Type.Shape) then
		return "ray.FilterIn is not a Shape"
	end
	local mutableShape = MutableShape(Items.pen)
	ray.FilterIn = mutableShape
	if not checkType(ray.FilterIn, Type.MutableShape) then
		return "ray.FilterIn is not a MutableShape"
	end

	-- test FilterOut

	ray.FilterOut = shape
	if not checkType(ray.FilterOut, Type.Shape) then
		return "ray.FilterOut is not a Shape"
	end
	ray.FilterOut = mutableShape
	if not checkType(ray.FilterOut, Type.MutableShape) then
		return "ray.FilterOut is not a MutableShape"
	end
	local o = Object()
	ray.FilterOut = o
	if not checkType(ray.FilterOut, Type.Object) then
		return "ray.FilterOut is not an Object"
	end
	ray.FilterOut = Map
	if not checkType(ray.FilterOut, Type.Map) then
		return "ray.FilterOut is not a Map"
	end
	ray.FilterOut = Player
	if not checkType(ray.FilterOut, Type.Player) then
		return "ray.FilterOut is not a Player"
	end
end

Client.shapeSupportTestSuite = function(shape)
	---- fields type checks

	if not checkType(shape.Width, "number") then
		return "Shape.Width"
	end
	if not checkType(shape.Height, "number") then
		return "Shape.Height"
	end
	if not checkType(shape.Depth, "number") then
		return "Shape.Depth"
	end
	if not checkType(shape.Pivot, "Number3") then
		return "Shape.Position"
	end
	if not checkType(shape.GetBlock, "function") then
		return "Shape.GetBlock"
	end
	if not checkType(shape.BlockToWorld, "function") then
		return "Shape:BlockToWorld"
	end
	if not checkType(shape.BlockToLocal, "function") then
		return "Shape:BlockToLocal"
	end

	---- functional checks

	-- pivot
	shape.Pivot = { 10.5, 20.5, 30.5 }
	if not checkNumber3(shape.Pivot, 10.5, 20.5, 30.5, epsilon) then
		return "shape.Pivot set as Number3"
	end
	shape.Pivot = Block(Color.Red, 10, 20, 30)
	if not checkNumber3(shape.Pivot, 10.5, 20.5, 30.5, epsilon) then
		return "shape.Pivot set as Block"
	end

	-- get block
	if pcall(shape.GetBlock, 1, 1.4, 1.2) then
		return "Shape.GetBlock did not raise an error (1)"
	end
	if pcall(shape.GetBlock, nil, 1, 1.4, 1.2) then
		return "Shape.GetBlock did not raise an error (2)"
	end
	local block = shape:GetBlock(1, 1.4, 1.2)
	if not checkType(block, Type.Block) then
		return "Shape:GetBlock w/ numbers"
	end
	block = shape:GetBlock({ 1, 1, 1 })
	if not checkType(block, Type.Block) then
		return "Shape:GetBlock w/ table"
	end
	block = shape:GetBlock(Number3(1, 1, 1))
	if not checkType(block, Type.Block) then
		return "Shape:GetBlock w/ Number3"
	end

	shape.Rotation = { 0, 0, 0 }
	shape.Scale = 1.0
	shape.Pivot = { 10, 20, 30 }
	shape.LocalPosition = { 100, -20, 30 }

	-- block to world
	local block_w1 = shape:BlockToWorld(Block(Color.Red, 30.1, 2.2, 10))
	if not checkNumber3(block_w1, 120 * Map.Scale.X, -38 * Map.Scale.Y, 10 * Map.Scale.Z, epsilon) then
		return "Shape:BlockToWorld w/ Block"
	end
	local block_w2 = shape:BlockToWorld(Number3(30.1, 2.2, 10))
	if not checkNumber3(block_w2, 120.1 * Map.Scale.X, -37.80 * Map.Scale.Y, 10 * Map.Scale.Z, epsilon) then
		return "Shape:BlockToWorld w/ Number3 "
	end
	local block_w3 = shape:BlockToWorld(30.1, 2.2, 10)
	if not checkNumber3(block_w3, 120.1 * Map.Scale.X, -37.80 * Map.Scale.Y, 10 * Map.Scale.Z, epsilon) then
		return "Shape:BlockToWorld w/ numbers"
	end

	-- world to block
	local world_b1 = shape:WorldToBlock(Number3(120.1 * Map.Scale.X, -37.80 * Map.Scale.Y, 10 * Map.Scale.Z))
	if not checkNumber3(world_b1, 30.1, 2.2, 10, epsilon) then
		return "Shape:WorldToBlock w/ Number3 "
	end
	local world_b2 = shape:WorldToBlock(120.1 * Map.Scale.X, -37.80 * Map.Scale.Y, 10 * Map.Scale.Z)
	if not checkNumber3(world_b2, 30.1, 2.2, 10, epsilon) then
		return "Shape:WorldToBlock w/ numbers"
	end

	-- block to local
	local block_l1 = shape:BlockToLocal(Block(Color.Red, 30.1, 2.2, 10))
	if not checkNumber3(block_l1, 20, -18, -20, epsilon) then
		return "Shape:BlockToLocal w/ Block"
	end
	local block_l2 = shape:BlockToLocal(Number3(30.1, 2.2, 10))
	if not checkNumber3(block_l2, 20.1, -17.80, -20, epsilon) then
		return "Shape:BlockToLocal w/ Number3"
	end
	local block_l3 = shape:BlockToLocal(30.1, 2.2, 10)
	if not checkNumber3(block_l3, 20.1, -17.80, -20, epsilon) then
		return "Shape:BlockToLocal w/ numbers"
	end

	-- local to block
	local local_b1 = shape:LocalToBlock(Number3(20.1, -17.80, -20))
	if not checkNumber3(local_b1, 30.1, 2.2, 10, epsilon) then
		return "Shape:LocalToBlock w/ Number3"
	end
	local local_b2 = shape:LocalToBlock(20.1, -17.80, -20)
	if not checkNumber3(local_b2, 30.1, 2.2, 10, epsilon) then
		return "Shape:LocalToBlock w/ numbers"
	end

	-- Copy checks
	local original = Shape(Items.pen)
	original.CollisionBox.Min = original.CollisionBox.Min - { 1, 1, 1 }
	original.CollisionBox.Max = original.CollisionBox.Max + { 2, 2, 2 }
	original.Pivot.Z = 20
	original.CollisionGroups = original.CollisionGroups + { 1, 2 } -- result {1, 2, 3}
	original.CollidesWithGroups = original.CollidesWithGroups + { 4 }
	original.Physics = true
	original.Mass = 25
	original.Friction = 0.35
	original.Bounciness = 0.45
	original.IsHidden = true

	local s2 = original:Copy()
	if pcall(original.Copy) then
		return "Shape.Copy did not raise an error"
	end
	if s2.BoundingBox.Min ~= original.BoundingBox.Min or s2.BoundingBox.Max ~= original.BoundingBox.Max then
		return "shape:Copy(): wrong BoundingBox"
	end
	if s2.Min ~= original.Min or s2.Max ~= original.Max or s2.Center ~= original.Center then
		return "shape:Copy(): wrong BoundingBox (Min/Max/Center)"
	end
	if s2.CollisionBox.Min ~= original.CollisionBox.Min or s2.CollisionBox.Max ~= original.CollisionBox.Max then
		return "shape:Copy(): wrong CollisionBox"
	end
	if s2.Center ~= original.Center then
		return "shape:Copy(): wrong Center"
	end
	if s2.Width ~= original.Width or s2.Height ~= original.Height or s2.Depth ~= original.Depth then
		return "shape:Copy(): wrong Width / Height / Depth"
	end
	if s2.Pivot ~= original.Pivot then
		return "shape:Copy(): wrong Pivot"
	end
	if s2.CollisionGroups ~= original.CollisionGroups then
		return "shape:Copy(): wrong CollisionGroups"
	end
	if s2.CollidesWithGroups ~= original.CollidesWithGroups then
		return "shape:Copy(): wrong CollidesWithGroups"
	end
	if s2.Physics ~= original.Physics then
		return "shape:Copy(): wrong Physics"
	end

	if s2.Mass ~= original.Mass then
		return "shape:Copy(): wrong Mass"
	end
	if s2.Friction ~= original.Friction then
		return "shape:Copy(): wrong Friction"
	end
	if s2.Bounciness ~= original.Bounciness then
		return "shape:Copy(): wrong Bounciness"
	end
	if s2.IsHidden ~= original.IsHidden then
		return "shape:Copy(): wrong IsHidden"
	end

	original.CollisionBox.Min = original.CollisionBox.Min - { 1, 1, 1 }
	original.CollisionBox.Max = original.CollisionBox.Max + { 2, 2, 2 }
	original.Pivot.Z = 25
	original.CollisionGroups = original.CollisionGroups - { 2 }
	original.CollidesWithGroups = original.CollidesWithGroups - { 3 }
	original.Physics = not original.Physics
	original.Mass = 2.0 * original.Mass
	original.Friction = 1.0 - original.Friction
	original.Bounciness = 1.0 - original.Bounciness
	original.IsHidden = not original.IsHidden

	if s2.CollisionBox.Min == original.CollisionBox.Min or s2.CollisionBox.Max == original.CollisionBox.Max then
		return "shape:Copy(): wrong CollisionBox after origin change"
	end
	if s2.Pivot == original.Pivot then
		return "shape:Copy(): wrong Pivot after origin change"
	end
	if s2.CollisionGroups == original.CollisionGroups then
		return "shape:Copy(): wrong CollisionGroups after origin change"
	end
	if s2.CollidesWithGroups == original.CollidesWithGroups then
		return "shape:Copy(): wrong CollidesWithGroups after origin change"
	end
	if s2.Physics == original.Physics then
		return "shape:Copy(): wrong Physics after origin change"
	end
	if s2.Mass == original.Mass then
		return "shape:Copy(): wrong Mass after origin change"
	end
	if s2.Friction == original.Friction then
		return "shape:Copy(): wrong Friction after origin change"
	end
	if s2.Bounciness == original.Bounciness then
		return "shape:Copy(): wrong Bounciness after origin change"
	end
	if s2.IsHidden == original.IsHidden then
		return "shape:Copy(): wrong IsHidden after origin change"
	end

	return nil
end

Client.shapeTestSuite = function()
	if Shape == nil then
		return "Shape == nil"
	end

	-- Shape instance

	local shape = Shape(Items.pen)
	if typeof(shape) ~= Type.Shape then
		return 'typeof(shape) ~= "Shape"'
	end
	Map:AddChild(shape)
	if not checkType(shape, Type.Shape) then
		return "Shape"
	end

	if shape.Width ~= 5 or shape.Height ~= 5 or shape.Depth ~= 26 then
		return "wrong Items.pen size"
	end

	-- Other constructors

	local s2 = Shape(shape)
	if not checkType(s2, Type.Shape) then
		return "Shape(shape) is not a Shape"
	end
	if s2.Width ~= 5 or s2.Height ~= 5 or s2.Depth ~= 26 then
		return "Shape wrong s2 size"
	end

	local s3 = shape:Copy()
	if not checkType(s3, Type.Shape) then
		return "shape:Copy() is not a Shape"
	end
	if s3.Width ~= 5 or s3.Height ~= 5 or s3.Depth ~= 26 then
		return "Shape wrong s3 size"
	end

	-- Object extension checks

	local shapeAsObject = Client.objectSupportTestSuite(shape)
	if shapeAsObject ~= nil then
		return shapeAsObject
	end

	-- Shape checks

	local shapeAsShape = Client.shapeSupportTestSuite(shape)
	if shapeAsShape ~= nil then
		return shapeAsShape
	end

	return nil
end

Client.mutableShapeTestSuite = function()
	if MutableShape == nil then
		return "MutableShape == nil"
	end

	local mutableShape = MutableShape(Items.pen)
	Map:AddChild(mutableShape)
	if not checkType(mutableShape, Type.MutableShape) then
		return "MutableShape"
	end

	-- Other constructors

	local s2 = MutableShape(mutableShape)
	if not checkType(s2, Type.MutableShape) then
		return "MutableShape(mutableShape) is not a MutableShape"
	end
	if s2.Width ~= 5 or s2.Height ~= 5 or s2.Depth ~= 26 then
		return "MutableShape wrong s2 size"
	end

	local s3 = mutableShape:Copy()
	if not checkType(s3, Type.MutableShape) then
		return "shape:Copy() is not a MutableShape"
	end
	if s3.Width ~= 5 or s3.Height ~= 5 or s3.Depth ~= 26 then
		return "MutableShape wrong s3 size"
	end

	--- empty MutableShape

	local empty = MutableShape()
	if empty.Width ~= 0 or empty.Height ~= 0 or empty.Depth ~= 0 then
		return "empty MutableShape: wrong size (1)"
	end

	---- Object extension checks

	local mutableShapeAsObject = Client.objectSupportTestSuite(mutableShape)
	if mutableShapeAsObject ~= nil then
		return mutableShapeAsObject
	end

	---- Shape extension checks

	local mutableShapeAsShape = Client.shapeSupportTestSuite(mutableShape)
	if mutableShapeAsShape ~= nil then
		return mutableShapeAsShape
	end

	---- fields type checks

	if not checkType(mutableShape.AddBlock, "function") then
		return "MutableShape:AddBlock"
	end
	if not checkType(mutableShape.Save, "function") then
		return "MutableShape:Save"
	end

	---- functional checks

	--------------------------
	-- MutableShape:AddBlock
	--------------------------

	-- adding a block over an existing block is expected to fail
	if mutableShape:AddBlock(1, 1, 1, 1) ~= false then
		return "mutableShape:AddBlock(Block(10, 1, 1, 1)) ~= false"
	end
	if pcall(mutableShape.AddBlock, 1) then
		return "MutableShape.AddBlock did not raise an error (1)"
	end
	if pcall(mutableShape.AddBlock, nil, 1) then
		return "MutableShape.AddBlock did not raise an error (2)"
	end
	block = mutableShape:GetBlock(1, 1, 1)
	if block == nil then
		return "mutableShape:GetBlock(1, 1, 1) == nil"
	end

	--- AddBlock(Block(2, 1, 1, 1))
	block:Remove()

	block = mutableShape:GetBlock(1, 1, 1)
	if block ~= nil then
		return "mutableShape:GetBlock(1, 1, 1) ~= nil"
	end

	local added = mutableShape:AddBlock(2, Number3(1, 1, 1))
	if added == false then
		return "mutableShape:AddBlock(Block(10, 1, 1, 1)) == nil"
	end

	if mutableShape:GetBlock(1, 1, 1).PaletteIndex ~= 2 then
		return "block.PaletteIndex ~= mutableShape:GetBlock(1, 1, 1).PaletteIndex (1)"
	end

	--- AddBlock(10, 1, 1, 1)
	mutableShape:GetBlock(1, 1, 1):Remove()
	block = mutableShape:GetBlock(1, 1, 1)
	if block ~= nil then
		return "mutableShape:GetBlock(1, 1, 1) ~= nil"
	end

	added = mutableShape:AddBlock(2, 1, 1, 1)
	if added == false then
		return "mutableShape:AddBlock(Block(10, 1, 1, 1)) == nil"
	end

	if mutableShape:GetBlock(Number3(1, 1, 1)).PaletteIndex ~= 2 then
		return "block.PaletteIndex ~= mutableShape:GetBlock(1, 1, 1).PaletteIndex (2)"
	end

	-- add/remove blocks in empty MutableShape

	added = empty:AddBlock(Color(1.0, 1.0, 1.0, 1.0), 5, 5, 5)
	if added == false then
		return "empty:AddBlock(color, 5, 5, 5) == nil"
	end
	if empty.Width ~= 1 or empty.Height ~= 1 or empty.Depth ~= 1 then
		return "empty MutableShape with one block: wrong size"
	end

	empty:GetBlock(5, 5, 5):Remove()
	block = empty:GetBlock(5, 5, 5)
	if block ~= nil then
		return "empty:GetBlock(5, 5, 5) ~= nil"
	end
	if empty.Width ~= 0 or empty.Height ~= 0 or empty.Depth ~= 0 then
		return "empty MutableShape: wrong size (2)"
	end

	return nil
end

Client.playerTestSuite = function()
	if Player == nil then
		return "Player == nil"
	end
	if not checkType(Player, Type.Player) then
		return "Player"
	end

	---- fields type check

	if not checkType(Player.ID, "number") then
		return "Player.ID"
	end
	if not checkType(Player.Username, Type.string) then
		return "Player.Username"
	end
	if not checkType(Player.IsLocal, "boolean") then
		return "Player.IsLocal"
	end
	if not checkType(Player.IsHidden, "boolean") then
		return "Player.IsHidden"
	end
	if not checkType(Player.IsOnGround, "boolean") then
		return "Player.IsOnGround"
	end
	--if not Client.checkType(Player.BlockUnderneath, Type.Block) then return "Player.BlockUnderneath" end
	--if not Client.checkType(Player.BlocksUnderneath, Type.table) then return "Player.BlocksUnderneath" end

	return nil
end

Client.playersTestSuite = function()
	if Players == nil then
		return "Players == nil"
	end

	-- only one player
	if #Players ~= 1 then
		return "#Players ~= 1 : " .. #Players
	end

	local playerID = 0
	if typeof(Players[playerID]) ~= Type.Player then
		return 'typeof(Players[playerID]) ~= "Player"'
	end

	local playerCount = 0
	for playerID, player in pairs(Players) do
		playerCount = playerCount + 1
	end
	if playerCount ~= 1 then
		return "Players: incorrect pairs loop count"
	end

	-- Note: ipairs cannot work with Players & OtherPlayers as the players IDs are not contiguous.

	-- LATER : also test OtherPlayers

	return nil
end

Client.otherPlayersTestSuite = function()
	if OtherPlayers == nil then
		return "OtherPlayers == nil"
	end

	if #OtherPlayers ~= 0 then
		return "#OtherPlayers ~= 0"
	end

	local a = 0
	for i, player in ipairs(OtherPlayers) do
		a = a + 1
	end
	if a ~= 0 then
		return "OtherPlayers: incorrect ipairs loop count"
	end

	a = 0
	for i, player in pairs(OtherPlayers) do
		a = a + 1
	end
	if a ~= 0 then
		return "OtherPlayers: incorrect pairs loop count"
	end

	return nil
end

Client.timeTestSuite = function()
	if Time == nil then
		return "Time == nil"
	end

	-- DEPRECATED: Time.Current doesn't do anything

	return nil
end

Client.timeCycleTestSuite = function()
	-- DEPRECATED: all TimeCycle.Marks point to global Sky colors, fields can't be set, marks can't be added

	return nil
end

Client.timeCycleMarkTestSuite = function()
	if TimeCycleMark == nil then
		return "TimeCycleMark == nil"
	end

	-- DEPRECATED: TimeCycleMark point to global Sky colors

	return nil
end

Client.timerTestSuite = function()
	local t1 = Timer(2, function()
		print(1)
	end)
	if typeof(t1) ~= Type.Timer then
		return 'typeof(t1) ~= "Timer"'
	end
	if typeof(t1.Time) ~= Type.number then
		return 'typeof(t1.Time) ~= "number"'
	end
	if typeof(t1.RemainingTime) ~= Type.number then
		return 'typeof(t1.RemainingTime) ~= "number"'
	end

	local t2 = Timer(3, true, function()
		print(2)
	end)
	if typeof(t2) ~= Type.Timer then
		return 'typeof(t2) ~= "Timer"'
	end
end

Client.jsonTestSuite = function()
	-- ENCODE
	local data = {}
	data.name = "Bob"
	data.age = 24
	data.hobbies = { dancing = 285, fishing = 2954 }
	data.list = { 2, 5, 78 }
	data.married = false
	data["42"] = "foo"
	data.func = function()
		print("foo")
	end
	data.friends = { { name = "Dave", age = 25 }, { name = "Foo", age = 27 } }
	local encoded = JSON:Encode(data)
	if pcall(JSON.Encode, data) then
		return "JSON.Encode did not raise an error (1)"
	end
	if pcall(JSON.Encode, nil, data) then
		return "JSON.Encode did not raise an error (2)"
	end
	if not string.find(encoded, "[2,5,78]") then
		return 'string.find(encoded, "[2,5,78]")'
	end
	if not string.find(encoded, '"age":27') then
		return 'string.find(encoded, "\\"age\\":27")'
	end
	if not string.find(encoded, '"married":false') then
		return 'string.find(encoded, "\\"married\\":false")'
	end
	if not string.find(encoded, '"42":"foo"') then
		return 'string.find(encoded, "42":"foo")'
	end
	if string.find(encoded, "func") then
		return "string.find(encoded, id)"
	end -- check if function is skipped

	-- decode encoded JSON:
	local decodedData, decodeErr
	local ok, err = pcall(function()
		decodedData, decodeErr = JSON:Decode(encoded)
	end)
	if not ok then
		return "Couldn't decode json encoded with JSON.Encode: " .. err
	end
	if decodeErr ~= nil then
		return "Error whend decoding json encoded with JSON.Encode: " .. decodeErr
	end
	if data.name ~= decodedData.name then
		return "JSON.Encode / JSON.Decode: data.name ~ decodedData.name"
	end
	if data.age ~= decodedData.age then
		return "JSON.Encode / JSON.Decode: data.age ~ decodedData.age"
	end
	if data.hobbies.dancing ~= decodedData.hobbies.dancing then
		return "JSON.Encode / JSON.Decode: data.hobbies.dancing ~ decodedData.hobbies.dancing"
	end
	if data.list[1] ~= decodedData.list[1] then
		return "JSON.Encode / JSON.Decode: data.list[1] ~ decodedData.list[1]"
	end
	if data.married ~= decodedData.married then
		return "JSON.Encode / JSON.Decode: data.married ~ decodedData.married"
	end
	if data["42"] ~= decodedData["42"] then
		return 'JSON.Encode / JSON.Decode: data["42"] ~ decodedData["42"]'
	end

	-- DECODE
	local data, err = JSON:Decode('{"fact":"A cat has 230 bones in its body.","length":32}')
	if pcall(JSON.Decode, '{"fact":"A cat has 230 bones in its body.","length":32}') then
		return "JSON.Decode did not raise an error"
	end
	if data.fact ~= "A cat has 230 bones in its body." then
		return 'data.fact ~= "A cat has 230 bones in its body."'
	end
	if data.length ~= 32 then
		return "data.length ~= 32"
	end
	if err ~= nil then
		return "err ~= nil"
	end

	-- Sub objects
	d, err = JSON:Decode('{"agents":{"agent1":{"name":"Bob"}}}')
	if d.agents.agent1.name ~= "Bob" then
		return 'd.agents.agent1.name ~= "Bob"'
	end

	d, err = JSON:Decode('{"agents":{"agent1":{"name":"Bob","test"}}}')
	if err == nil then
		return "Can't parse JSON but err is nil"
	end

	d, err = JSON:Decode('{"list":[10,24]}')
	if d.list[2] ~= 24 then
		return "d.list[2] == 24"
	end

	d, err = JSON:Decode(42)
	if d ~= 42 then
		return "d ~= 42"
	end
end

-- TODO: block:Remove, block:Replace w/ shapes

-- testing to define Client.Tick
Client.Tick = function(dt)
	-- dummy function
end

-- object containing the unit test functions
unit = {}

unit.point = {}
unit.point.basic = function()
	local ms = MutableShape(Items.pen)
	ms.Position = { 0, 0, 0 }
	ms.Pivot = { 0, 0, 0 }
	if ms == nil then
		return "ms == nil"
	end
	pt = ms:GetPoint("not-found")
	if pcall(ms.GetPoint, "not-found") then
		return "MutableShape.GetPoint did not raise an error (1)"
	end
	if pcall(ms.GetPoint, nil, "not-found") then
		return "MutableShape.GetPoint did not raise an error (2)"
	end
	if pt ~= nil then
		return "pt ~= nil"
	end

	pt = ms:AddPoint("point")
	if pcall(ms.AddPoint, "point") then
		return "MutableShape.AddPoint did not raise an error (1)"
	end
	if pcall(ms.AddPoint, nil, "point") then
		return "MutableShape.AddPoint did not raise an error (2)"
	end

	if pt == nil then
		return "pt == nil"
	end

	-- print("pt", pt)
	-- print("pt.Position", pt.Position)
	-- print("pt.LocalPosition", pt.LocalPosition)
	-- print("pt.Coordinates", pt.Coordinates)

	if pt == nil then
		return "pt == nil"
	end
	if pt.Position == nil then
		return "pt.Position == nil"
	end
	if pt.Pos == nil then
		return "pt.Pos == nil"
	end
	if pt.LocalPosition == nil then
		return "pt.LocalPosition == nil"
	end
	if pt.Coordinates == nil then
		return "pt.Coordinates == nil"
	end
	if pt.Coords == nil then
		return "pt.Coords == nil"
	end

	if pt.Coords.X ~= 0 then
		return "pt.Coords.X ~= 0"
	end
	if pt.Coords.Y ~= 0 then
		return "pt.Coords.Y ~= 0"
	end
	if pt.Coords.Z ~= 0 then
		return "pt.Coords.Z ~= 0"
	end

	if pt.Position.X ~= 0 then
		return "pt.Position.X ~= 0"
	end
	if pt.Position.Y ~= 0 then
		return "pt.Position.Y ~= 0"
	end
	if pt.Position.Z ~= 0 then
		return "pt.Position.Z ~= 0"
	end

	if pt.LocalPosition.X ~= 0 then
		return "pt.LocalPosition.X ~= 0"
	end
	if pt.LocalPosition.Y ~= 0 then
		return "pt.LocalPosition.Y ~= 0"
	end
	if pt.LocalPosition.Z ~= 0 then
		return "pt.LocalPosition.Z ~= 0"
	end

	if pt.Rotation.X ~= 0 then
		return "pt.Rotation.X ~= 0"
	end
	if pt.Rotation.Y ~= 0 then
		return "pt.Rotation.Y ~= 0"
	end
	if pt.Rotation.Z ~= 0 then
		return "pt.Rotation.Z ~= 0"
	end

	-- move the shape
	ms.Position = { 2, 0, 0 }

	if pt.Coords.X ~= 0 then
		return "pt.Coords.X ~= 0"
	end
	if pt.Coords.Y ~= 0 then
		return "pt.Coords.Y ~= 0"
	end
	if pt.Coords.Z ~= 0 then
		return "pt.Coords.Z ~= 0"
	end

	if pt.Position.X ~= 2 then
		return "pt.Position.X ~= 2"
	end
	if pt.Position.Y ~= 0 then
		return "pt.Position.Y ~= 0"
	end
	if pt.Position.Z ~= 0 then
		return "pt.Position.Z ~= 0"
	end

	if pt.LocalPosition.X ~= 0 then
		return "pt.LocalPosition.X ~= 0"
	end
	if pt.LocalPosition.Y ~= 0 then
		return "pt.LocalPosition.Y ~= 0"
	end
	if pt.LocalPosition.Z ~= 0 then
		return "pt.LocalPosition.Z ~= 0"
	end

	pt = ms:GetPoint("point")

	if pt == nil then
		return "pt == nil"
	end

	if pt.Coords.X ~= 0 then
		return "pt.Coords.X ~= 0"
	end
	if pt.Coords.Y ~= 0 then
		return "pt.Coords.Y ~= 0"
	end
	if pt.Coords.Z ~= 0 then
		return "pt.Coords.Z ~= 0"
	end

	if pt.Position.X ~= 2 then
		return "pt.Position.X ~= 2"
	end
	if pt.Position.Y ~= 0 then
		return "pt.Position.Y ~= 0"
	end
	if pt.Position.Z ~= 0 then
		return "pt.Position.Z ~= 0"
	end

	if pt.LocalPosition.X ~= 0 then
		return "pt.LocalPosition.X ~= 0"
	end
	if pt.LocalPosition.Y ~= 0 then
		return "pt.LocalPosition.Y ~= 0"
	end
	if pt.LocalPosition.Z ~= 0 then
		return "pt.LocalPosition.Z ~= 0"
	end

	-- override "point"
	pt = ms:AddPoint("point", { 2, 2, 2 })

	-- print("pt", pt)
	-- print("ms.Position", ms.Position)
	-- print("ms.Pivot", ms.Pivot)
	-- print("pt.Position", pt.Position)
	-- print("pt.Position.X", pt.Position.X)
	-- print("pt.LocalPosition", pt.LocalPosition)
	-- print("pt.Coordinates", pt.Coordinates)

	if pt == nil then
		return "pt == nil"
	end

	if not checkNumber3(pt.Coords, 2, 2, 2, epsilon) then
		return "pt.Coords ~= {2,2,2}"
	end
	if not checkNumber3(pt.Position, 4, 2, 2, epsilon) then
		return "pt.Position ~= {4,2,2}"
	end
	if not checkNumber3(pt.LocalPosition, 2, 2, 2, epsilon) then
		return "pt.LocalPosition ~= {2,2,2}"
	end

	-- add more tests ...
	return nil
end

-- test functions should return an error string (on failure) or nil (on success)
-- test.test1 = function()
-- perform test here
-- end

-- Client.DidReceiveEvent = function(event)
-- 	if event.name == "data_test_response" then
-- 		if e.err == "" then
-- 			dataSuccess = true
-- 		else
-- 			print("Data:", e.err)
-- 		end
-- 		dataTestEnded = true
-- 	end
-- end

-- Server.DidReceiveEvent = function(event)
-- 	if event.name == "data_test" then
-- 		local path = "http://api.particubes.com/ping"
-- 		HTTP:Get(path, function (response)
-- 			local data = response.Body
-- 			local e = Event()
-- 			e.name = "data_test_response"
-- 			e.err = ""
-- 			if typeof(data) ~= Type.Data then
-- 				e.err = e.err + 'typeof(data) ~= "Data"'
-- 			end
-- 			if response.StatusCode ~= 200 then
-- 				e.err = e.err + "response.StatusCode ~= 200"
-- 			end
-- 			if data.Len ~= 15 then
-- 				e.err = e.err + "data.Len ~= 15"
-- 			end
-- 			if data[2] ~= 34 then -- 34 <=> "
-- 				e.err = e.err + "data[2] ~= 34"
-- 			end
-- 			if data:ToString() ~= "{\"msg\":\"pong\"}" then
-- 				e.err = e.err + "data:ToString() ~= \"{\"msg\":\"pong\"}\""
-- 			end
-- 			e:SendTo(event.Sender)
-- 		end)
-- 	end
-- end
