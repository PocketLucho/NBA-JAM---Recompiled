CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wno-deprecated-declarations
INC = -Isrc/emu -Isrc/mame -Isrc/board
CORE_SRC = src/emu/emu.cpp \
  src/mame/cpu/tms34010/tms34010.cpp src/mame/cpu/tms34010/34010dsm.cpp \
  src/mame/cpu/m6809/m6809.cpp src/mame/cpu/m6809/6x09dasm.cpp \
  src/mame/cpu/adsp2100/adsp2100.cpp src/mame/cpu/adsp2100/2100dasm.cpp \
  src/mame/sound/okiadpcm.cpp src/mame/ymfm/ymfm_opm.cpp src/mame/ymfm/ymfm_misc.cpp src/mame/ymfm/ymfm_adpcm.cpp src/mame/ymfm/ymfm_ssg.cpp \
  src/board/midway_video.cpp src/board/williams_adpcm.cpp src/board/tunit.cpp src/board/dcs.cpp src/board/wunit.cpp
BUILD ?= build/linux
CORE_OBJ = $(patsubst %.cpp,$(BUILD)/%.o,$(CORE_SRC))

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -MMD -MP -c $< -o $@

$(BUILD)/libcore.a: $(CORE_OBJ)
	ar rcs $@ $^

$(BUILD)/headless: tools/headless/headless.cpp $(BUILD)/libcore.a
	$(CXX) $(CXXFLAGS) $(INC) $< $(BUILD)/libcore.a -o $@

all: $(BUILD)/headless

-include $(CORE_OBJ:.o=.d)

$(BUILD)/headless_w: tools/headless/headless_w.cpp $(BUILD)/libcore.a
	$(CXX) $(CXXFLAGS) $(INC) $< $(BUILD)/libcore.a -o $@

# ---------------- Lua 5.4 (mods) ----------------
LUA_SRC = $(filter-out src/third_party/lua/lua.c src/third_party/lua/luac.c,$(wildcard src/third_party/lua/*.c))
$(BUILD)/lua/%.o: src/third_party/lua/%.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -std=gnu99 -c $< -o $@
$(BUILD)/liblua.a: $(patsubst src/third_party/lua/%.c,$(BUILD)/lua/%.o,$(LUA_SRC))
	ar rcs $@ $^

$(BUILD)/luatest: tools/headless/luatest.cpp src/app/mods.cpp src/app/romload.cpp $(BUILD)/libcore.a $(BUILD)/liblua.a $(BUILD)/miniz.o
	$(CXX) $(CXXFLAGS) $(INC) $(APP_INC) $< src/app/mods.cpp src/app/romload.cpp $(BUILD)/miniz.o $(BUILD)/libcore.a $(BUILD)/liblua.a -o $@

$(BUILD)/nettest: tools/headless/nettest.cpp src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/mods.cpp $(BUILD)/libcore.a $(BUILD)/liblua.a $(BUILD)/miniz.o
	$(CXX) $(CXXFLAGS) $(INC) $(APP_INC) $< src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/mods.cpp $(BUILD)/miniz.o $(BUILD)/libcore.a $(BUILD)/liblua.a -lpthread -o $@

$(BUILD)/modtest: tools/headless/modtest.cpp src/app/mods.cpp src/app/romload.cpp $(BUILD)/libcore.a $(BUILD)/liblua.a $(BUILD)/miniz.o
	$(CXX) $(CXXFLAGS) $(INC) $(APP_INC) $< src/app/mods.cpp src/app/romload.cpp $(BUILD)/miniz.o $(BUILD)/libcore.a $(BUILD)/liblua.a -o $@

# ---------------- application (Windows, llvm-mingw cross) ----------------
SDL2W ?= /home/claude/deps/SDL2-2.30.9/x86_64-w64-mingw32
APP_SRC = src/app/main.cpp src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/gl_present.cpp src/app/controls.cpp src/app/mods.cpp src/third_party/miniz/miniz.c
WIN_OUT ?= dist/NBAJamRecomp
win:
	@mkdir -p $(WIN_OUT) build/win
	x86_64-w64-mingw32-gcc -O2 -c src/third_party/miniz/miniz.c -o build/win/miniz.o
	$(MAKE) BUILD=build/win CC=x86_64-w64-mingw32-gcc build/win/liblua.a
	x86_64-w64-mingw32-g++ -std=c++20 -O2 -Isrc/emu -Isrc/mame -Isrc/board -Isrc/app -Isrc/third_party/miniz -Isrc/third_party/lua -I$(SDL2W)/include/SDL2 \
	  src/app/main.cpp src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/gl_present.cpp src/app/controls.cpp src/app/mods.cpp build/win/miniz.o build/win/libcore.a build/win/liblua.a \
	  -L$(SDL2W)/lib -lmingw32 -lSDL2main -lSDL2 -mwindows -static -lwinmm -lsetupapi -loleaut32 -lole32 -limm32 -lversion -lgdi32 -luser32 -lshell32 -luuid -ladvapi32 -lcfgmgr32 -lws2_32 -lwinpthread \
	  -o $(WIN_OUT)/NBAJamRecomp.exe

# ---------------- application (Linux, for testing) ----------------
SDL2L ?= /home/claude/deps/sdl2linux
APP_INC = -Isrc/emu -Isrc/mame -Isrc/board -Isrc/app -Isrc/third_party/miniz -Isrc/third_party/lua
$(BUILD)/miniz.o: src/third_party/miniz/miniz.c
	$(CC) -O2 -c $< -o $@
linux-app: $(BUILD)/libcore.a $(BUILD)/miniz.o $(BUILD)/liblua.a
	$(CXX) $(CXXFLAGS) $(APP_INC) -I$(SDL2L)/include/SDL2 src/app/main.cpp src/app/games.cpp src/app/romload.cpp src/app/netplay.cpp src/app/gl_present.cpp src/app/controls.cpp src/app/mods.cpp \
	  $(BUILD)/miniz.o $(BUILD)/libcore.a $(BUILD)/liblua.a -L$(SDL2L)/lib -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -static-libstdc++ -static-libgcc -lm -ldl -lpthread -o $(BUILD)/NBAJamRecomp
$(BUILD)/cattest: tools/headless/cattest.cpp src/app/games.cpp src/app/romload.cpp $(BUILD)/libcore.a $(BUILD)/miniz.o
	$(CXX) $(CXXFLAGS) $(APP_INC) $< src/app/games.cpp src/app/romload.cpp $(BUILD)/miniz.o $(BUILD)/libcore.a -o $@
.PHONY: win linux-app
