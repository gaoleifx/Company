function clone( tab )
    local ins = {}
    for key. var in pairs(tab) do
        ins[key] = var
    end
    return ins
end

function copy( dist, tab )
    for key,var in pairs(tab) do
        dist[key] = var
    end
end

peopel = {}

people.sayHi = function (self)
    print("people say hi:"..self.name)
end

peole.new = function(name)
    local self = clone(people)
    self.name = name
    return self
end

local p = people.new("zhangsan")
-- p.sayHi(p)
p:sayHi()

-- inherit
man = {}
man.new = function(name)
    local self = people.new(name)
    copy(self, man)
    return self
end

man.sayHello = function()
    print("man say hello")
end

local m = man.new("Lisi")
m:sayHello()