-- 函数
-- function 函数名()
-- end
function F1()
    print("F1函数")
end
-- 无参数返回值
F2 = function()
    print("F2函数")
end
-- 有参数
function F3(a)
    print(a)
end
F3(1)
F3("123")
F3(true)
-- 有返回值
-- 函数的类型
-- 函数的重载
-- 变长参数
-- 函数嵌套