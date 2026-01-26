Config = {
    -- using item as map
    Map = "aduermael.hills",
    -- items that are going to be loaded before startup
    Items = {
        "jacksbertox.crate"
    }
}

local CRATE_START_POSITION = Number3(382, 290, 153)

Client.OnStart = function()
   
    local ambience = require("ambience") 
    ambience:set(ambience.noon)

    sfx = require("sfx")
    Player.Head:AddChild(AudioListener)

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
    crate.Pivot.Y = 0
    crate.Position = CRATE_START_POSITION
    crateOwner = nil

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
    local msg = type(payload) == "string" and payload or payload.message

    Player:TextBubble(msg, 3, true)
    sfx("waterdrop_2", {Position = Player.Position, Pitch = 1.1 + math.random() * 0.5})
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
            t.Position = impact.Object:BlockToWorld(blockCenter)

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
