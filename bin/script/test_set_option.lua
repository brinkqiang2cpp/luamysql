local mysql = require 'luamysql'

print('mysql client version:', mysql._VERSION)

local flag = (mysql.CLIENT_MULTI_STATEMENTS | mysql.CLIENT_MULTI_RESULTS)
local conf = 
{
    host = 'localhost',
    user = 'root',
    passwd = '91idol.com',
    db = 'world',
    client_flag = flag
}

local client = mysql.createClient()
client:setCharset('utf8')
client:setReconnect(true)
client:setConnectTimeout(3)
client:setReadTimeout(3)
client:setWriteTimeout(3)
client:setProtocol(mysql.PROTOCOL_TCP)
client:setCompress()
client:connect(conf)
client:close()
print('passed')