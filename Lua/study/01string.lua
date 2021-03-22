str1 = "abeCdefs"
print(#str1)--字符串长度#， 一个汉字占三个字符
-- 转义字符的应用
print("123\n123")
s = [[
    the 
    world
    is good
]]

print(s)
-- 字符串拼接
print("123".."456")
s1 = "123456"
s2 = 111
print(s1 .. s2)

--%d:数字 %a:字符 %s:字符
print(string.format( "wangwu age is %d", 18 ))

--类型转换
s1 = 123
print(tostring(s1))

--小写转大写
str = "absDE"
print(string.upper( str ))
--大写转小写
print(string.lower( str ))
--翻转字符串
print(string.reverse( str ))
--字符串查找索引
print(string.find( str,"Cde",0,1 )
--截取字符串
print(string.sub(str, 3, 4))
--字符串重复
print(string.rep( str, 2 ))
--字符串修改
print(string.gsub( str,"Cd", "**"))