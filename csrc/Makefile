include_dir=include

.cache/%.o: %.c
	@test -d $(@D) || mkdir -p $(@D)
	gcc $< -Wall -Wextra -c -g -o $@ --std=c11 -I$(include_dir)

bin/bfcomp-c: .cache/bfcomp.o .cache/main.o
	@test -d $(@D) || mkdir -p $(@D)
	gcc -fuse-ld=lld $^ -g -o $@

bin/hidey: .cache/hidey.o .cache/tween.o .cache/util.o .cache/select_set.o
	@test -d $(@D) || mkdir -p $(@D)
	gcc -fuse-ld=lld $^-g -o $@

clean:
	@if [ -d ./.cache ]; then rm -r .cache; fi
	@if [ -d ./bin ]; then rm -r bin; fi
