include_dir=include

.cache/%.o: src/%.c
	@test -d $(@D) || mkdir -p $(@D)
	gcc $< -Wall -Wextra -c -g -o $@ --std=c11 -I$(include_dir) -D_XOPEN_SOURCE=700 

bin/bfcomp-c: .cache/impl.o .cache/options.o .cache/ir_compile.o .cache/elf_gen.o .cache/ir_gen.o .cache/ir_interpret.o .cache/main.o
	@test -d $(@D) || mkdir -p $(@D)
	gcc -fuse-ld=lld $^ -g -o $@

clean:
	@if [ -d ./.cache ]; then rm -r .cache; fi
	@if [ -d ./bin ]; then rm -r bin; fi
