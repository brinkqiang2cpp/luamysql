
// Copyright (c) 2018 brinkqiang (brink.qiang@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "luamysql_module.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <WinSock2.h>
#endif

#ifdef _WIN32
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif

#define LUAMYSQL_CONN       "Connection*"
#define LUAMYSQL_CURSOR     "Cursor*"

#define check_conn(L)   ((Connection*)luaL_checkudata(L, 1, LUAMYSQL_CONN))
#define check_cursor(L) ((Cursor*)luaL_checkudata(L, 1, LUAMYSQL_CURSOR))


// MySQL connection object
typedef struct _Connection
{
    int     closed;
    MYSQL   my_conn;
}Connection;

// Cursor object
typedef struct _Cursor
{
    int conn;                   // reference to connection
    int fetch_all;              // fetch all result in one query
    unsigned int    numcols;    // number of columns
    MYSQL_RES*      my_res;     // mysql result instance
    MYSQL_FIELD*    fields;     // column names and types
}Cursor;


static int create_cursor(lua_State* L, int conn, MYSQL_RES* result,
                         int numcols, int fetch_all)
{
    Cursor* cur = (Cursor*)lua_newuserdata(L, sizeof(Cursor));
    lua_pushvalue(L, conn);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    assert(ref != LUA_REFNIL);
    luaL_getmetatable(L, LUAMYSQL_CURSOR);
    lua_setmetatable(L, -2);
    cur->conn = ref;
    cur->numcols = numcols;
    cur->fetch_all = fetch_all;
    cur->fields = NULL;
    cur->my_res = result;
    return 1;
}

// Closes the cursos and nullify all structure fields.
static void cursor_nullify(lua_State* L, Cursor* cur)
{
    /* Nullify structure fields. */
    if (cur->my_res)
    {
        mysql_free_result(cur->my_res);
        cur->my_res = NULL;
        cur->fields = NULL;
    }
    if (cur->conn != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, cur->conn);
        cur->conn = LUA_NOREF;
    }
}

static int cursor_gc(lua_State *L)
{
    Cursor* cur = check_cursor(L);
    if (cur)
    {
        cursor_nullify(L, cur);
    }
    return 0;
}

static int cursor_numrows(lua_State *L)
{
    Cursor* cur = check_cursor(L);
    luaL_argcheck(L, cur && cur->my_res, 1, "invalid Cursor object");
    my_ulonglong rows = mysql_num_rows(cur->my_res);
    lua_pushinteger(L, (lua_Integer)rows);
    return 1;
}

static void pushvalue(lua_State* L, enum enum_field_types type,
                      const char* row, unsigned long len)
{
    if (row == NULL || type == MYSQL_TYPE_NULL)
    {
        lua_pushnil(L);
        return;
    }

    switch (type)
    {
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
        lua_pushinteger(L, atoll(row)); // default integer type in lua 5.3 is long long
        break;
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
        lua_pushnumber(L, atof(row));
        break;
    default:
        lua_pushlstring(L, row, len);
        break;
    }
}

static void result_to_table(lua_State* L, Cursor* cur, MYSQL_ROW row,
                            unsigned long* lengths, int alpha_idx)
{
    assert(L && cur && lengths);
    if (alpha_idx)
    {
        lua_createtable(L, 0, cur->numcols);
        for (unsigned i = 0; i < cur->numcols; i++)
        {
            pushvalue(L, cur->fields[i].type, row[i], lengths[i]);
            lua_setfield(L, -2, cur->fields[i].name);
        }
    }
    else // numeric index
    {
        lua_createtable(L, cur->numcols, 0);
        for (unsigned i = 0; i < cur->numcols; i++)
        {
            pushvalue(L, cur->fields[i].type, row[i], lengths[i]);
            lua_rawseti(L, -2, i + 1);
        }
    }
}

static int cursor_fetch(lua_State* L)
{
    Cursor* cur = check_cursor(L);
    luaL_argcheck(L, cur && cur->my_res, 1, "invalid Cursor object");
    int alpha_idx = 0;
    const char* opt = lua_tostring(L, 2);
    if (opt && strcmp(opt, "a") == 0) // alphabetic table index
    {
        alpha_idx = 1;
    }

    MYSQL_RES* res = cur->my_res;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) // no more results
    {
        return 0;
    }
    unsigned long* lengths = mysql_fetch_lengths(res);
    if (cur->fields == NULL)
    {
        cur->fields = mysql_fetch_fields(res);
    }
    luaL_argcheck(L, lengths && cur->fields, 1, "fetch fields failed");
    luaL_checkstack(L, cur->numcols, "too many columns");
    result_to_table(L, cur, row, lengths, alpha_idx);
    return 1;
}

static int cursor_fetch_all(lua_State* L)
{
    Cursor* cur = check_cursor(L);
    luaL_argcheck(L, cur && cur->my_res, 1, "invalid Cursor object");
    luaL_argcheck(L, cur->fetch_all, 1, "not compatible with execute()");

    int alpha_idx = 0;
    const char* opt = lua_tostring(L, 2);
    if (opt && strcmp(opt, "a") == 0) // alphabetic table index
    {
        alpha_idx = 1;
    }

    MYSQL_RES* res = cur->my_res;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL || cur->numcols == 0) // no results
    {
        cursor_nullify(L, cur);
        return 0;
    }
    if (cur->fields == NULL)
    {
        cur->fields = mysql_fetch_fields(res);
    }
    unsigned long* lengths = mysql_fetch_lengths(res);
    luaL_argcheck(L, lengths && cur->fields, 1, "fetch fields failed");
    int num_rows = (int)mysql_num_rows(cur->my_res);
    lua_createtable(L, num_rows, 0);
    int rownum = 1;
    while (row)
    {
        result_to_table(L, cur, row, lengths, alpha_idx);
        lua_rawseti(L, -2, rownum++);
        row = mysql_fetch_row(res);
    }
    cursor_nullify(L, cur);
    return 1;
}


static int conn_create(lua_State* L)
{
    Connection* conn = (Connection*)lua_newuserdata(L, sizeof(Connection));
    conn->closed = 0;
    if (mysql_init(&conn->my_conn) == &conn->my_conn)
    {
        luaL_getmetatable(L, LUAMYSQL_CONN);
        lua_setmetatable(L, -2);
        return 1;
    }
    else
    {
        return luaL_error(L, "%s\n", mysql_error(&conn->my_conn));
    }
}

static int conn_close(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn, 1, "invalid Connection object");
    if (!conn->closed)
    {
        mysql_close(&conn->my_conn);
        conn->closed = 1;
    }
    return 0;
}

#define conn_gc conn_close

static int conn_tostring(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn, 1, "invalid Connection object");
    lua_pushfstring(L, "Connection* (%p)", conn);
    return 1;
}

static int conn_connect(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    luaL_argcheck(L, lua_istable(L, 2), 1, "argument must be table");

    lua_getfield(L, 2, "host");
    const char* host = luaL_checkstring(L, -1);
    lua_getfield(L, 2, "user");
    const char* user = luaL_checkstring(L, -1);
    lua_getfield(L, 2, "passwd");
    const char* passwd = luaL_checkstring(L, -1);
    lua_getfield(L, 2, "db");
    const char* db = lua_tostring(L, -1);
    lua_getfield(L, 2, "port");
    unsigned int port = (unsigned int)luaL_optinteger(L, -1, 3306);
    lua_getfield(L, 2, "unix_socket");
    const char* unix_socket = lua_tostring(L, -1);
    lua_getfield(L, 2, "client_flag");
    unsigned long flags = (unsigned long)luaL_optinteger(L, -1, 0);
    lua_pop(L, 7);

    if (mysql_real_connect(&conn->my_conn, host, user, passwd, db, port,
            unix_socket, flags) != &conn->my_conn)
    {
        return luaL_error(L, "connect failed, %s\n", mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_execute(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");

    size_t length = 0;
    const char* stmt = luaL_checklstring(L, 2, &length);
    const char* fetch_opt = lua_tostring(L, 3);
    int fetch_all = 1;
    if (fetch_opt)
    {
        if (strcmp(fetch_opt, "store") == 0)
            fetch_all = 1;
        else if (strcmp(fetch_opt, "use") == 0)
            fetch_all = 0;
    }

    MYSQL* my_conn = &conn->my_conn;
    int err = mysql_real_query(my_conn, stmt, (unsigned long)length);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(my_conn));
    }

    MYSQL_RES* res = NULL;
    if (fetch_all)
        res = mysql_store_result(my_conn);
    else
        res = mysql_use_result(my_conn);
    unsigned int numcols = mysql_field_count(my_conn);
    if (res)
    {
        return create_cursor(L, 1, res, numcols, fetch_all);
    }
    else
    {
        if (numcols == 0) // query does not return data (not SELECT)
        {
            lua_pushinteger(L, (lua_Integer)mysql_affected_rows(my_conn));
            return 1;
        }
    }
    return 0;
}

static int conn_commit(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    if (mysql_commit(&conn->my_conn) != 0)
    {
        return luaL_error(L, "commit failed, %s\n", mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_rollback(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    if (mysql_rollback(&conn->my_conn) != 0)
    {
        return luaL_error(L, "rollback failed, %s\n", mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_charset(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    const char* charset = luaL_checkstring(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_SET_CHARSET_NAME, charset);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_reconnect(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    my_bool val = (my_bool)lua_toboolean(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_RECONNECT, &val);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_connect_timeout(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    uint32_t timeout = (unsigned int)luaL_checkinteger(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_read_timeout(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    uint32_t timeout = (unsigned int)luaL_checkinteger(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_write_timeout(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    uint32_t timeout = (unsigned int)luaL_checkinteger(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_compress(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_COMPRESS, NULL);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_set_protocol(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    unsigned int protocol = (unsigned int)luaL_checkinteger(L, 2);
    int err = mysql_options(&conn->my_conn, MYSQL_OPT_PROTOCOL, &protocol);
    if (err != 0)
    {
        return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
    }
    return 0;
}

static int conn_ping(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    MYSQL* my_conn = &conn->my_conn;
    if (my_conn->methods == NULL)
    {
        int err = mysql_ping(my_conn);
        if (err != 0)
        {
            return luaL_error(L, "%d: %s\n", err, mysql_error(&conn->my_conn));
        }
    }
    return 0;
}

static int conn_escape_string(lua_State* L)
{
    Connection* conn = check_conn(L);
    luaL_argcheck(L, conn && !conn->closed, 1, "invalid Connection object");
    size_t length = 0;
    const char* stmt = luaL_checklstring(L, 2, &length);
    char* dest = malloc(length * 2 + 1);
    if (dest == NULL)
    {
        return luaL_error(L, "malloc() failed.");
    }
    unsigned long newlen = mysql_real_escape_string(&conn->my_conn, dest,
        stmt, (unsigned long)length);
    lua_pushlstring(L, dest, newlen);
    free(dest);
    return 1;
}


//////////////////////////////////////////////////////////////////////////

#define push_literal(L, name, value)\
    lua_pushstring(L, name);        \
    lua_pushinteger(L, value);      \
    lua_rawset(L, -3);


static void push_mysql_constant(lua_State* L)
{
    // protocol type
    push_literal(L, "PROTOCOL_DEFAULT", MYSQL_PROTOCOL_DEFAULT);
    push_literal(L, "PROTOCOL_TCP", MYSQL_PROTOCOL_TCP);
    push_literal(L, "PROTOCOL_SOCKET", MYSQL_PROTOCOL_SOCKET);
    push_literal(L, "PROTOCOL_PIPE", MYSQL_PROTOCOL_PIPE);
    push_literal(L, "PROTOCOL_MEMORY", MYSQL_PROTOCOL_MEMORY);

    // client flag
    push_literal(L, "CLIENT_LONG_PASSWORD", CLIENT_LONG_PASSWORD);
    push_literal(L, "CLIENT_FOUND_ROWS", CLIENT_FOUND_ROWS);
    push_literal(L, "CLIENT_LONG_FLAG", CLIENT_LONG_FLAG);
    push_literal(L, "CLIENT_CONNECT_WITH_DB", CLIENT_CONNECT_WITH_DB);
    push_literal(L, "CLIENT_NO_SCHEMA", CLIENT_NO_SCHEMA);
    push_literal(L, "CLIENT_COMPRESS", CLIENT_COMPRESS);
    push_literal(L, "CLIENT_ODBC", CLIENT_ODBC);
    push_literal(L, "CLIENT_LOCAL_FILES", CLIENT_LOCAL_FILES);
    push_literal(L, "CLIENT_IGNORE_SPACE", CLIENT_IGNORE_SPACE);
    push_literal(L, "CLIENT_PROTOCOL_41", CLIENT_PROTOCOL_41);
    push_literal(L, "CLIENT_INTERACTIVE", CLIENT_INTERACTIVE);
    push_literal(L, "CLIENT_SSL", CLIENT_SSL);
    push_literal(L, "CLIENT_IGNORE_SIGPIPE", CLIENT_IGNORE_SIGPIPE);
    push_literal(L, "CLIENT_TRANSACTIONS", CLIENT_TRANSACTIONS);
    //push_literal(L, "CLIENT_SECURE_CONNECTION", CLIENT_SECURE_CONNECTION);
    push_literal(L, "CLIENT_MULTI_STATEMENTS", CLIENT_MULTI_STATEMENTS);
    push_literal(L, "CLIENT_MULTI_RESULTS", CLIENT_MULTI_RESULTS);
    push_literal(L, "CLIENT_PS_MULTI_RESULTS", CLIENT_PS_MULTI_RESULTS);
}

static void create_meta(lua_State* L, const char* name, const luaL_Reg* methods)
{
    assert(L && name && methods);
    luaL_newmetatable(L, name);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    lua_pushliteral(L, "__metatable");
    lua_pushliteral(L, "cannot access this metatable");
    lua_settable(L, -3);
    lua_pop(L, 1);  /* pop new metatable */
}

static void make_meta(lua_State* L)
{
    static const luaL_Reg conn_methods[] =
    {
        { "__gc", conn_gc },
        { "__tostring", conn_tostring },
        { "setCharset", conn_set_charset },
        { "setReconnect", conn_set_reconnect },
        { "setConnectTimeout", conn_set_connect_timeout },
        { "setWriteTimeout", conn_set_write_timeout },
        { "setReadTimeout", conn_set_read_timeout },
        { "setProtocol", conn_set_protocol },
        { "setCompress", conn_set_compress },
        { "escape", conn_escape_string },
        { "connect", conn_connect },
        { "ping", conn_ping },
        { "close", conn_close },
        { "execute", conn_execute },
        { "commit", conn_commit },
        { "rollback", conn_rollback },
        { NULL, NULL },
    };

    static const luaL_Reg cursor_methods[] =
    {
        { "__gc", cursor_gc },
        { "fetch", cursor_fetch },
        { "fetchAll", cursor_fetch_all },
        { "numrows", cursor_numrows },
        { NULL, NULL },
    };

    create_meta(L, LUAMYSQL_CONN, conn_methods);
    create_meta(L, LUAMYSQL_CURSOR, cursor_methods);
}

LUAMOD_API int luaopen_luamysql(lua_State* L)
{
    static const luaL_Reg lib[] =
    {
        { "createClient", conn_create },
        { NULL, NULL },
    };

    make_meta(L);
    luaL_newlib(L, lib);
    push_mysql_constant(L);
    lua_pushliteral(L, "_VERSION");
    lua_pushstring(L, mysql_get_client_info());
    lua_settable(L, -3);
    return 1;
}

LUAMOD_API int require_luamysql(lua_State* L)
{
    luaL_requiref(L, "luamysql", luaopen_luamysql, 0);
    printf("lua module: require luamysql\n");
    return 1;
}
