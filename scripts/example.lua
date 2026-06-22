-- Elaina Executor - Example Script
-- This demonstrates common executor API functions

print("Hello from Elaina Executor!")
print("Script identity:", getscriptidentity())

-- Get executor environment
local env = getgenv()
if not env then
    print("Failed to get executor env")
    return
end

-- Wire (ESP) example
local function CreateESP(player)
    local highlight = Instance.new("Highlight")
    highlight.Name = "ESP_" .. player.Name
    highlight.Adornee = player.Character
    highlight.FillColor = Color3.fromRGB(255, 50, 50)
    highlight.OutlineColor = Color3.fromRGB(255, 255, 255)
    highlight.FillTransparency = 0.5
    highlight.Parent = player.Character
end

for _, player in ipairs(game:GetService("Players"):GetPlayers()) do
    if player ~= game:GetService("Players").LocalPlayer then
        CreateESP(player)
    end
end

game:GetService("Players").PlayerAdded:Connect(function(player)
    player.CharacterAdded:Connect(function()
        CreateESP(player)
    end)
end)

print("ESP script loaded. All players highlighted.")
