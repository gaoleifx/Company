-- config = {hello = "hello", world = "world"}
-- print(config.hello)

arr = {}
for var = 1, 100 do
    table.insert( arr,1, var )
end

-- 求数组的长度
table.maxn(arr)

for key, value in pairs(arr) do
    print(key, value)
end