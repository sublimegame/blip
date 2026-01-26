-- This is Cubzh's default world script. 
-- We'll provide more templates later on to cover specific use cases like FPS games, data storage, synchornized shapes, etc.
-- Cubzh dev team and devs from the community will be happy to help you on Discord if you have questions: discord.gg/cubzh 

Config = {
    -- using item as map
    Map = "aduermael.hills",
    -- items that are going to be loaded before startup
    Items = {
        "jacksbertox.crate"
    }
}

local CRATE_START_POSITION = Number3(382, 290, 153)

-- Client.OnStart is the first function to be called when the world is launched, on each user's device.
Client.OnStart = function()
   
    -- Setting up the ambience (lights)
    -- other possible presets:
    -- - ambience.dawn
    -- - ambience.dusk
    -- - ambience.midnight
    -- The "ambience" module also accepts
    -- custom settings (light colors, angles, etc.)
    local ambience = require("ambience") 
    ambience:set(ambience.noon)

    -- The sfx module can be used to play spatialized sounds in one line calls.
    -- A list of available sounds can be found here: 
    -- https://docs.cu.bzh/guides/quick/adding-sounds#list-of-available-sounds
    sfx = require("sfx")
    -- There's only one AudioListener, but it can be placed wherever you want:
    Player.Head:AddChild(AudioListener)

    -- Requiring "multi" module is all you need to see other players in your game!
    -- (remove this line if you want to be solo)
    require("multi")

    -- This function drops the local player above the center of the map:
    dropPlayer = function()
        Player.Position = Number3(Map.Width * 0.5, Map.Height + 10, Map.Depth * 0.5) * Map.Scale
        Player.Rotation = { 0, 0, 0 }
        Player.Velocity = { 0, 0, 0 }
    end

    -- Add player to the World (root scene Object) and call dropPlayer().
    World:AddChild(Player)
    dropPlayer()

    -- A Shape is an object made out of cubes.
    -- Let's instantiate one with one of our imported items:
    crate = Shape(Items.jacksbertox.crate)
    World:AddChild(crate)
    crate.Physics = PhysicsMode.StaticPerBlock
    -- By default, the pivot is at the center of the bounding box, 
    -- but since want to place this one with ground positions, let's
    -- move the pivot to the bottom:
    crate.Pivot.Y = 0
    crate.Position = CRATE_START_POSITION
    crateOwner = nil

    -- USER INTERFACE (buttons & labels)
    -- What can be seen as a 2D layer on top of your app is in fact built
    -- with the same features as all the rest (Shapes, Texts, Quads, Rays, etc.),
    -- but rendered with an orthographic camera. 
    -- That's what "uikit" module does, here's how to use it:

    local uikit = require("uikit")
    local margin = 8

    -- Adding a button to change the ambience (light, fog)
    local btn = uikit:createButton("Noon")
    local ambiences = {
        {name = "Dawn", value = ambience.dawn},
        {name = "Noon", value = ambience.noon},
        {name = "Dusk", value = ambience.dusk},
        {name = "Midnight", value = ambience.midnight},
        -- The ambience module allows to define custom ambiences like this:
        {name = "Crazy", value = { 
            sky = { skyColor = Color(0,0,255), horizonColor = Color(255,0,0), abyssColor = Color(0,255,0), lightColor = Color(30,186,108) },
            fog = { near = 100, far = 500, color = Color(194, 0, 62) }}}
    }
    local currentAmbience = 2
    -- It's a good habit to place uikit elements within parentDidResize() callbacks, 
    -- because elements usually need to be re-positioned when the window is resized
    -- or screen rotated.
    btn.parentDidResize = function()
        btn.pos.X = margin
        btn.pos.Y = Screen.Height - Screen.SafeArea.Top - btn.Height - margin
    end
    -- Calling parentDidResize manually once to place elements:
    btn:parentDidResize()
    -- onRelease is called after click/touch down & up events on the button:
    btn.onRelease = function()
        currentAmbience = currentAmbience + 1
        if currentAmbience > #ambiences then currentAmbience = 1 end
        ambience:set(ambiences[currentAmbience].value)
        btn.Text = ambiences[currentAmbience].name
    end

    -- Adding a label to display number of jumps:
    nbJumps = 0
    jumpsLabel = uikit:createText("Jumps: " .. nbJumps, Color.White)
    -- We want the label position to be updated whenever its parent or content is resized.
    -- Instead of defining that twice, let's create a function:
    local updateLabelPosition = function()
        jumpsLabel.pos.X = Screen.Width - Screen.SafeArea.Right - jumpsLabel.Width - margin
        jumpsLabel.pos.Y = Screen.Height - Screen.SafeArea.Top - jumpsLabel.Height - margin
    end
    jumpsLabel.parentDidResize = updateLabelPosition
    jumpsLabel.contentDidResize = updateLabelPosition
    -- call updateLabelPosition() now for initial placement.
    updateLabelPosition()
end

-- jump function, triggered with Action1
-- (space bar on PC, button 1 on mobile)
Client.Action1 = function()
    if Player.IsOnGround then
        Player.Velocity.Y = 100
        nbJumps = nbJumps + 1
        jumpsLabel.Text = "Jumps: " .. nbJumps
        sfx("hurtscream_1", {Position = Player.Position, Volume = 0.4})
    end
end

-- Client.Tick is executed up to 60 times per second on player's device.
Client.Tick = function(dt)
    -- Detect if player is falling and use dropPlayer() when it happens!
    if Player.Position.Y < -500 then
        dropPlayer()
        -- It's funnier with a message.
        Player:TextBubble("💀 Oops!", true)
    end
end

-- Triggered when posting message with chat input
Client.OnChat = function(payload)
    -- <0.0.52 : "payload" was a string value.
    -- 0.0.52+ : "payload" is a table, with a "message" key
    local msg = type(payload) == "string" and payload or payload.message

    Player:TextBubble(msg, 3, true)
    sfx("waterdrop_2", {Position = Player.Position, Pitch = 1.1 + math.random() * 0.5})

    -- We can modify the message before it is sent.
    -- payload.message = payload.message .. " my suffix"

    -- If true is returned, the payload is "consumed".
    -- This prevents it to be sent to others.
    -- return true
end

-- Pointer.Click is called following click/touch down & up events, 
-- without draging the pointer in between. 
-- Let's use this function to add a few interactions with the scene!
Pointer.Click = function(pointerEvent)

    -- Cast a ray from pointer event,
    -- do different things depending on what it hits.
    local impact = pointerEvent:CastRay()
    if impact ~= nil then
        if impact.Object == Player then
            -- clicked on local player -> display message + little jump
            Player:TextBubble("Easy, I'm ticklish! 😬", 1.0, true)
            sfx("waterdrop_2", {Position = Player.Position, Pitch = 1.1 + math.random() * 0.5})
            Player.Velocity.Y = 50

        elseif impact.Object == Map and impact.Block ~= nil then
            -- clicked on Map block -> display block info
            
            -- making an exception if player is holding the crate to place it
            if crateEquiped and crateOwner == Player.ID and impact.FaceTouched == Face.Top then
                -- The position of the impact can be computed from the origin of the ray
                -- (popointerEvent.Position here), its direction (pointerEvent.Direction),
                -- and the distance:
                local impactPosition = pointerEvent.Position + pointerEvent.Direction * impact.Distance
                
                Player:EquipRightHand(nil)
                World:AddChild(crate)
                crate.Physics = PhysicsMode.StaticPerBlock
                crate.Scale = 1
                crate.Pivot.Y = 0
                crate.Position = impactPosition
                crateEquiped = false

                -- send event to inform server and other players that
                -- crate as been placed. Of course this code is not
                -- needed if you turn off multiplayer.
                local e = Event()
                e.action = "place_crate"
                e.owner = Player.ID
                e.pos = crate.Position
                e:SendTo(Server, OtherPlayers)

                sfx("wood_impact_1", {Position = crate.Position})

                crateOwner = nil
                    
                return
            end
                
            local b = impact.Block
            local t = Text()
            t.Text = string.format("coords: %d,%d,%d\ncolor: %d,%d,%d",
                                b.Coords.X, b.Coords.Y, b.Coords.Z,
                                b.Color.R, b.Color.G, b.Color.B)
            t.FontSize = 44
            t.Type = TextType.Screen -- display text in screen space
            t.BackgroundColor = Color(0,0,0,0) -- transparent
            t.Color = Color(255,255,255)
            World:AddChild(t)

            local blockCenter = b.Coords + {0.5,0.5,0.5}
            -- convert block coordinates to world position:
            t.Position = impact.Object:BlockToWorld(blockCenter)

            -- Timer to request text removal in 1 second
            Timer(1.0, function()
                t:RemoveFromParent()
            end)
            
        elseif not crateEquiped and impact.Object == crate then
            if impact.Distance < 80 then
         
                crate.Physics = PhysicsMode.Disabled
                Player:EquipRightHand(crate)
                crate.Scale = 0.5
                crateEquiped = true
                crateOwner = Player.ID
                
                -- inform server and other players
                -- that crate has been picked
                local e = Event()
                e.action = "picked_crate"
                e:SendTo(Server, OtherPlayers)

                sfx("wood_impact_5", {Position = crate.Position, Pitch = 1.2})

            else
                Player:TextBubble("I'm too far to grab it!", 1, true)
                sfx("waterdrop_2", {Position = Player.Position, Pitch = 1.1 + math.random() * 0.5})
            end
        end
    end
end

-- This function is executed on each user's device 
-- when an event arrives, from another player or server.
Client.DidReceiveEvent = function(e)
    if e.action == "picked_crate" then
        crate.Physics = PhysicsMode.Disabled
        e.Sender:EquipRightHand(crate)
        crateOwner = e.Sender.ID
        crate.Scale = 0.5
        crateEquiped = true
        sfx("wood_impact_1", {Position = crate.Position})

    elseif e.action == "set_crate_owner" then
        local p = Players[e.owner]
        if p then
            crate.Physics = PhysicsMode.Disabled
            e.Sender:EquipRightHand(crate)
            crateOwner = e.Sender.ID
            crate.Scale = 0.5
            crateEquiped = true
            sfx("wood_impact_1", {Position = crate.Position})
        end

    elseif e.action == "place_crate" then
        if crateOwner ~= nil then
            local p = Players[crateOwner]
            if p then
                p:EquipRightHand(nil)
            end
            crateOwner = nil
        end
        World:AddChild(crate)
        crate.Physics = PhysicsMode.StaticPerBlock
        crate.Scale = 1
        crate.Pivot.Y = 0
        crate.Position = e.pos
        crateEquiped = false
        sfx("wood_impact_5", {Position = e.pos, Pitch = 1.2})

    elseif e.action == "message" then
        print(e.Sender.Username .. ": " .. e.message)
        e.Sender:TextBubble(e.message, 3, true)
        sfx("waterdrop_2", {Position = e.Sender.Position, Pitch = 1.1 + math.random() * 0.5})
    end
end

-----------------
-- Server code --
-----------------

-- Function executed when the server starts.
Server.OnStart = function()
    cratePosition = CRATE_START_POSITION
    crateOwner = nil
end

-- All we need to do on the server is remember who's 
-- carrying the crate, or where it is currently placed, 
-- to inform newcomers.
Server.DidReceiveEvent = function(e)
    if e.action == "picked_crate" then
        -- print(e.Sender.Username .. " has the crate")
        crateOwner = e.Sender.ID
    elseif e.action == "place_crate" then
        cratePosition = e.pos
        crateOwner = nil
    end
end

-- Executed when players are joining.
Server.OnPlayerJoin = function(player)
    if crateOwner == nil then
        local e = Event()
        e.action = "place_crate"
        e.pos = cratePosition
        e:SendTo(player)
    else
        local e = Event()
        e.action = "set_crate_owner"
        e.owner = crateOwner
        e:SendTo(player)
    end
end

-- if player carryoing the crate leaves, 
-- we should place the crate back where it's been picked.
Server.OnPlayerLeave = function(player)
    if crateOwner ~= nil and player.ID == crateOwner then
        local e = Event()
        e.action = "place_crate"
        e.pos = cratePosition
        e:SendTo(Players)
        crateOwner = nil
    end
end

-- Server.Tick is executed up to 60 times per second on the server.
Server.Tick = function(dt) 
    -- nothing needed here in that script
end