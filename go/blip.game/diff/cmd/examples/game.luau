-- This script demonstrates physics interactions by allowing players to place crates
-- on a baseplate by clicking. It includes a UI counter for tracking placed crates
-- and handles player spawning and positioning.


Modules = {
    sfx = "sfx",
    ui = "uikit",
}

-- constants
local jumpValue = 100

-- variables
crates = {}
crate = nil
baseplate = nil

-- function executed when the game starts
-- initializes UI elements for tracking crate count
-- finds and sets up the baseplate and crate template objects
-- spawns the player into the world at the starting position
Client.OnStart = function()

    cratesLabel = ui:createText("crates: 0", { color = Color.Black })
    cratesLabel.parentDidResize = function(self)
        cratesLabel.pos = {
            Screen.Width - self.Width - 10,
            Screen.Height - self.Height - 10,
        }
    end

    for i, o in World.Children do
        if o.ItemName ~= nil then
            if string.find(o.ItemName, "baseplate") then
                baseplate = o
            elseif string.find(o.ItemName, "crate") then
                crate = o
                crate:RemoveFromParent()
            end
        end
    end

    World:AddChild(Player)
    dropPlayer()
end

-- Drop player to the ground
function dropPlayer()
    Player.Position = {0, 40, 0}
    Player.Rotation = { 0, 0, 0 }
    Player.Velocity = { 0, 0, 0 }
end

-- Place a new crate at the clicked location when the player clicks on the baseplate.
-- The crate is added to the world and tracked in the crates table for counting.
Pointer.Click = function(pointerEvent)
    local impact = pointerEvent:CastRay(baseplate)
    if impact ~= nil then
        local p = pointerEvent.Position + pointerEvent.Direction * impact.Distance
        local newCrate = crate:Copy()
        newCrate:SetParent(World)
        newCrate.Position:Set(p)
        table.insert(crates, newCrate)
        cratesLabel.text = "crates: " .. #crates
    end
end

-- jump function, with double jump
-- allows the player to jump and double jump
-- plays a sound effect when the player jumps
local doubleJumpAvailable = false
Client.Action1 = function()
    local jumped = false
    if Player.IsOnGround then
        Player.Velocity.Y = jumpValue
        doubleJumpAvailable = true
        jumped = true
    elseif doubleJumpAvailable then
        Player.Velocity.Y = jumpValue
        doubleJumpAvailable = false
        jumped = true
    end
    if jumped then
        sfx("hurtscream_1", { Volume = 0.8, Pitch = 1.0, Spatialized = false })
    end
end

