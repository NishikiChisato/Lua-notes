#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static const char *point = "point";
static const char *line = "line";

struct point {
  int x;
  int y;
};

void display_stack(struct lua_State *L) {
  for (int i = lua_gettop(L); i >= 1; i --) {
    printf("%d: type: %s\n", i, lua_typename(L, lua_type(L, i)));
  }
}

int l_pnew(lua_State *L) {
  // index 1 is a table
  lua_Integer arg1 = luaL_optinteger(L, 2, 0);
  lua_Integer arg2 = luaL_optinteger(L, 3, 0);
  struct point *p = lua_newuserdata(L, sizeof *p);
  luaL_setmetatable(L, point);
  p->x = arg1;
  p->y = arg2;
  return 1;
}

int l_pinc(lua_State *L) {
  struct point *p = luaL_checkudata(L, 1, point);
  lua_Integer arg1 = luaL_checkinteger(L, 2);
  lua_Integer arg2 = luaL_checkinteger(L, 3);
  p->x += arg1; p->y += arg2;
  return 0;
}

static void display_point(struct point *p) {
  printf("[C] point: (x: %d, y: %d) -> %p\n", p->x, p->y, p);
}

int l_pdis(lua_State *L) {
  struct point *p = luaL_checkudata(L, 1, point);
  display_point(p);
  return 0;
}

struct line {
  struct point lp;
  struct point rp;
};

int l_lnew(lua_State *L) {
  struct line *l = lua_newuserdata(L, sizeof *l);
  luaL_setmetatable(L, line);
  l->lp.x = l->lp.y = 0;
  l->rp.x = l->rp.y = 0;
  return 1;
}

int l_linc(lua_State *L) {
  struct line *l = luaL_checkudata(L, 1, line);
  lua_Integer pos = luaL_checkinteger(L, 2);
  lua_Integer xinc = luaL_checkinteger(L, 3);
  lua_Integer yinc = luaL_checkinteger(L, 4);
  if (pos == 0) {
    l->lp.x += xinc; l->lp.y += yinc;
  } else if (pos == 1) {
    l->rp.x += xinc; l->rp.y += yinc;
  } else {
    luaL_error(L, "type error");
  }
  return 0;
}

int l_lpoint(lua_State *L) {
  struct line *l = luaL_checkudata(L, 1, line);
  lua_Integer pos = luaL_checkinteger(L, 2);
  if (pos == 0) {
    lua_pushlightuserdata(L, &l->lp);
  } else {
    lua_pushlightuserdata(L, &l->rp);
  }
  luaL_setmetatable(L, point);
  return 1;
}

static void line_display(struct line *l) {
  printf("[C] line: [l: (x: %d, y: %d) => %p; r: (x: %d, y: %d) => %p] -> %p\n", l->lp.x, l->lp.y, &l->lp, l->rp.x, l->rp.y, &l->rp, l);
}

int l_ldis(lua_State *L) {
  struct line *l = luaL_checkudata(L, 1, line);
  line_display(l);
  return 0;
}

int main() {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  {
    static const luaL_Reg l[] = {
      {"pnew", l_pnew},
      {"pinc", l_pinc},
      {"pdis", l_pdis},
      {NULL, NULL},
    };
    // set point table for external function
    luaL_newmetatable(L, point);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    luaL_setfuncs(L, l, 0);

    // metatable for point
    lua_newtable(L);
    lua_pushcfunction(L, l_pnew);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setglobal(L, "point");
  }

  {
    static const luaL_Reg l[] = {
      {"lnew", l_lnew},
      {"linc", l_linc},
      {"ldis", l_ldis},
      {"lpoint", l_lpoint},
      {NULL, NULL},
    };
    // set line table for external function
    luaL_newmetatable(L, line);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, l, 0);

    // metatable for line
    lua_newtable(L);

    lua_pushcfunction(L, l_lnew);
    lua_setfield(L, -2, "__call");

    // line should derived from point
    luaL_getmetatable(L, point);
    lua_setfield(L, -2, "__index");

    lua_setmetatable(L, -2);

    lua_setglobal(L, "line");
  }

  if (luaL_dofile(L, "./clua.lua") == LUA_OK) {
    printf("[C] executed lua script\n");
  } else {
    printf("[C] lua script error: %s\n", lua_tostring(L, -1));
  }

  printf("[C] execute\n");

  return 0;
}
