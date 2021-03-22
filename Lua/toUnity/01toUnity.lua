local this = Controller
local Gameobject = UnityEngine.Gameobject
local Input = UnityEngine.Input
local Rigidbody = UnityEngine.Rigidbody
local Color = UnityEngine.Color

local sphere
local Rigi
local force

function this.Start()
    Sphere = Gameobject.Find("Sphere")