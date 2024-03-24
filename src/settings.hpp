#pragma once

#include <fmt/core.h>
#include <optional>

#include "common.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#ifndef LOOPBACK
#define LOOPBACK true
#endif

/* TODO: Now when we have a web server running we probably want to replace configuration by Lua
 * script with a web page and a sqlite database backing. For example the page could include some
 * form of factory reset button, which purges the database. This kind of configuration would render
 * the lua script obsolete */
struct Settings : public Options {
    Settings(const Options &options) : Options{options} {
        // TODO: Need to create some form of empty configuration state
        LuaFile file(settings_file);

        LuaTable netatmo_config = file.get_table("netatmo");
        netatmo.client_id = netatmo_config.get_string_field("client_id");
        netatmo.client_secret = netatmo_config.get_string_field("client_secret");
        netatmo.device_id = netatmo_config.get_string_field("device_id");

        LuaTable modules = netatmo_config.get_table_field("modules");
        netatmo.modules.outdoor = modules.get_string_field("outdoor");
        netatmo.modules.rain = modules.get_string_field("rain");

        try {
            LuaTable position_table = file.get_table("position");
            std::string longitude = position_table.get_string_field("longitude");
            std::string latitude = position_table.get_string_field("latitude");
            position = Position{
                .longitude = longitude,
                .latitude = latitude,
            };
        } catch (const std::runtime_error &err) {
            fmt::print("No (valid) position provided, will use netatmo position\n");
        }
    }

    struct Netatmo {
        std::string client_id;
        std::string client_secret;
        std::string device_id;
        struct Modules {
            std::string outdoor;
            std::string rain;
        } modules;
    } netatmo;

#if defined(LOOPBACK) && LOOPBACK
    static constexpr std::string_view auth_server{"http://localhost:8080/oauth2/authorize"};
    static constexpr std::string_view token_server{"http://localhost:8080/oauth2/token"};
    static constexpr std::string_view station_addr{"http://localhost:8080/api/getstationsdata"};
#else
    static constexpr std::string_view auth_server{"https://api.netatmo.com/oauth2/authorize"};
    static constexpr std::string_view token_server{"https://api.netatmo.com/oauth2/token"};
    static constexpr std::string_view station_addr{"https://api.netatmo.com/api/getstationsdata"};
#endif

    std::optional<Position> position{};

  private:
    /* TODO: Generalize this approach not always look at top of stack, instead keep track of height
     * of stack, and the position of self. */
    /* Helper object for automatic stack popping */
    struct LuaObj {
        LuaObj(lua_State *L) : L(L) {
        }
        virtual ~LuaObj() {
            lua_pop(L, 1);
        }

        lua_State *L;
    };

    struct LuaString : public LuaObj {
        LuaString(lua_State *L) : LuaObj(L) {
        }
        operator std::string() const {
            return lua_tostring(L, -1);
        }
    };

    struct LuaTable : public LuaObj {
        LuaTable(lua_State *L) : LuaObj(L) {
        }

        LuaString get_string_field(const char *name) const {
            int status = lua_getfield(L, -1, name);
            if (status != LUA_TSTRING) {
                throw utils::runtime_error("Expected field {} to be a string, but it's not", name);
            }
            return LuaString{L};
        }

        LuaTable get_table_field(const char *name) {
            int status = lua_getfield(L, -1, name);
            if (status != LUA_TTABLE) {
                throw utils::runtime_error("Expected {} to be a table, but it's not", name);
            }
            return LuaTable{L};
        }
    };

    struct LuaFile {
        lua_State *L = luaL_newstate();

        LuaFile(const std::string &filename) {
            luaL_openlibs(L);

            int status = luaL_dofile(L, filename.c_str());
            if (status) {
                throw utils::runtime_error("Unable to open and read settings file: {}",
                                           lua_tostring(L, -1));
            }
        }

        ~LuaFile() {
            lua_close(L);
        }

        LuaTable get_table(const char *table) {
            int status = lua_getglobal(L, table);
            if (status != LUA_TTABLE) {
                throw utils::runtime_error("Expected {} to be a table, but it's not", table);
            }
            return LuaTable{L};
        }
    };
};
