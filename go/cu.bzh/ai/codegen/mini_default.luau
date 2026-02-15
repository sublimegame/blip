Config = {
	Map = "aduermael.hills",
}

-- Client.OnStart is the first function to be called when the world is launched, on each user's device.
Client.OnStart = function()
	-- This function drops the local player above the center of the map:
	dropPlayer = function()
		Player.Position = Number3(Map.Width * 0.5, Map.Height + 10, Map.Depth * 0.5) * Map.Scale
		Player.Rotation = { 0, 0, 0 }
		Player.Velocity = { 0, 0, 0 }
	end

	-- Add player to the World (root scene Object) and call dropPlayer().
	World:AddChild(Player)
	dropPlayer()
end

-- jump function, triggered with Action1
-- (space bar on PC, button 1 on mobile)
Client.Action1 = function()
	if Player.IsOnGround then
		Player.Velocity.Y = 100
	end
end
