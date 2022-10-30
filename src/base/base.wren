// this file is stuck at the top of every Wren build configuration file

foreign class CC {
	construct new() {}

	// getters

	foreign debug()
	foreign path()

	// setters

	foreign debug=(x)
	foreign path=(x)

	// methods

	foreign add_opt(opt)
	foreign compile(path)
}

class File {
	foreign static list(path, depth)
	static list(path) { list(path, 0) }
}

foreign class Linker {
	construct new() {}
	construct new(cc) {}

	// getters

	foreign path()
	foreign archiver_path()

	// setters

	foreign path=(path)
	foreign archiver_path=(path)

	// methods

	foreign link(path_list, out)
	foreign link(path_list, out, shared)
	foreign archive(path_list, out)
}
