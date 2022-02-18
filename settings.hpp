#pragma once

#include <fmt/core.h>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

struct Settings {
    // TODO: Would probably make sense to merge this with options.
    //       could have this class extend options.
    Settings(const std::string &settings_file) {
        LuaFile file(settings_file);

        LuaTable netatmo_config = file.get_table("netatmo");
        netatmo.password = netatmo_config.get_string_field("password");
        netatmo.client_id = netatmo_config.get_string_field("client_id");
        netatmo.username = netatmo_config.get_string_field("username");
        netatmo.client_secret = netatmo_config.get_string_field("client_secret");
        netatmo.device_id = netatmo_config.get_string_field("device_id");

        LuaTable modules = netatmo_config.get_table_field("modules");
        netatmo.modules.outdoor = modules.get_string_field("outdoor");
        netatmo.modules.rain = modules.get_string_field("rain");
    }

    struct Netatmo {
        std::string password;
        std::string client_id;
        std::string username;
        std::string client_secret;
        std::string device_id;
        struct Modules {
            std::string outdoor;
            std::string rain;
        } modules;
    } netatmo;

  private:
    // TODO: Generalize this approach not always look at top of stack,
    //       instead keep track of height of stack, and the position of
    //       self.
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
                throw std::runtime_error(
                    fmt::format("Expected field {} to be a string, but it's not", name));
            }
            return LuaString{L};
        }

        LuaTable get_table_field(const char *name) {
            int status = lua_getfield(L, -1, name);
            if (status != LUA_TTABLE) {
                throw std::runtime_error(
                    fmt::format("Expected {} to be a table, but it's not", name));
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
                throw std::runtime_error(
                    fmt::format("Unable to open and read settings file: {}", lua_tostring(L, -1)));
            }
        }

        ~LuaFile() {
            lua_close(L);
        }

        LuaTable get_table(const char *table) {
            int status = lua_getglobal(L, table);
            if (status != LUA_TTABLE) {
                throw std::runtime_error(
                    fmt::format("Expected {} to be a table, but it's not", table));
            }
            return LuaTable{L};
        }
    };
};
