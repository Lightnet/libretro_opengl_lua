#include <lua.h>
#include <lauxlib.h>
#include <GL/gl.h>
#include "glsym/glsym.h"
#include "libretro.h"
#include "globals.h"
#include <math.h>

#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER

// Clear the screen with color and buffers
static int l_gl_clear(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER); // r
   luaL_checktype(L, 2, LUA_TNUMBER); // g
   luaL_checktype(L, 3, LUA_TNUMBER); // b
   luaL_checktype(L, 4, LUA_TNUMBER); // a
   float r = (float)lua_tonumber(L, 1);
   float g = (float)lua_tonumber(L, 2);
   float b = (float)lua_tonumber(L, 3);
   float a = (float)lua_tonumber(L, 4);
   glClearColor(r, g, b, a);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   return 0;
}

// Set viewport
static int l_gl_viewport(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TNUMBER); // width
   luaL_checktype(L, 2, LUA_TNUMBER); // height
   int width = (int)lua_tonumber(L, 1);
   int height = (int)lua_tonumber(L, 2);
   glViewport(0, 0, width, height);
   return 0;
}

// Bind framebuffer
static int l_gl_bind_framebuffer(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TLIGHTUSERDATA); // hw_render
   struct retro_hw_render_callback *hw_render = (struct retro_hw_render_callback *)lua_touserdata(L, 1);
   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render->get_current_framebuffer());
   return 0;
}

// Setup shader program and vertex attributes
static int l_gl_use_program(lua_State *L)
{
   glUseProgram(prog);
   glEnable(GL_DEPTH_TEST);

   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   int vloc = glGetAttribLocation(prog, "aVertex");
   glVertexAttribPointer(vloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
   glEnableVertexAttribArray(vloc);
   int cloc = glGetAttribLocation(prog, "aColor");
   glVertexAttribPointer(cloc, 4, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(GLfloat)));
   glEnableVertexAttribArray(cloc);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   return 0;
}

// Set MVP matrix and draw
static int l_gl_draw(lua_State *L)
{
   luaL_checktype(L, 1, LUA_TTABLE); // MVP matrix
   int loc = glGetUniformLocation(prog, "uMVP");

   GLfloat mvp[16];
   for (int i = 0; i < 16; i++)
   {
      lua_rawgeti(L, 1, i + 1);
      if (lua_isnumber(L, -1))
         mvp[i] = (GLfloat)lua_tonumber(L, -1);
      else
         mvp[i] = 0.0f;
      lua_pop(L, 1);
   }

   glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   return 0;
}

// Disable vertex attributes and program
static int l_gl_cleanup(lua_State *L)
{
   int vloc = glGetAttribLocation(prog, "aVertex");
   int cloc = glGetAttribLocation(prog, "aColor");
   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(cloc);
   glUseProgram(0);
   return 0;
}

// Register Lua functions
static const struct luaL_Reg gl_funcs[] = {
   {"clear", l_gl_clear},
   {"viewport", l_gl_viewport},
   {"bind_framebuffer", l_gl_bind_framebuffer},
   {"use_program", l_gl_use_program},
   {"draw", l_gl_draw},
   {"cleanup", l_gl_cleanup},
   {NULL, NULL}
};

int luaopen_gl(lua_State *L)
{
   luaL_newlib(L, gl_funcs);
   return 1;
}