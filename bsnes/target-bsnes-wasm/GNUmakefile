name := bsnes-wasm

ifeq ($(shell which ccache),)
compiler = emcc
else
$(info Using ccache)
compiler = ccache emcc
endif

options += -sENVIRONMENT=web
options += -sALLOW_MEMORY_GROWTH=1
# must be multiple of 64 Ki = 65536
options += -sINITIAL_MEMORY=50331648

objects += web-wasm
objects := $(objects:%=obj/%.o)

obj/web-wasm.o: $(ui)/wasm.cpp

all: $(objects)
	$(info Linking out/$(name) ...)
	@mkdir -p out/web
	+@$(compiler) -o out/web/$(name).html $(objects) $(options)

clean.wasm:
	$(call rdelete,out/web)

clean: clean.wasm
