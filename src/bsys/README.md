# bsys

Short for "buildsystem", this directory is where all the different buildsystems Bob supports are, each in their own directory (e.g. Bob is in `src/bsys/bob`).

Each new bsys must export a `bsys_t` descriptor, which looks like this:

```c
struct bsys_t {
	char const* name;
	char const* key;

	bool (*identify)(void);
	int (*setup)(void);
	int (*build)(void);
	int (*install)(char const* prefix);
	int (*run)(int argc, char* argv[]);
	void (*destroy)(void);
};
```

This is then added to the `BSYS` array in `bsys.h`.

All the code to support the buildsystem should be contained in its directory.
The one exception to this rule is Bob itself, which has it's main source file in `src/bsys/bob/main.c` for the sake of consistency, but whose bulk of code is rooted in `src` itself.
