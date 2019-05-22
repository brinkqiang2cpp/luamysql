local mysql = require 'luamysql'

local conf =
{
    host = 'localhost',
    user = 'root',
    passwd = '91idol.com',
    db = 'world',
}

local client = mysql.createClient()
client:connect(conf)

local stmt = [[select count(*) as num from country where continent='asia']]
local cur = client:execute(stmt)
local r = cur:fetch('a')
local total = r.num

local stmt1 = [[update country set region='whaterver' where continent='asia']]
local r1 = client:execute(stmt1)
assert(r1 == total)

print('passed')