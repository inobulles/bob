// this file is stuck at the top of every Wren build configuration file
// object classes

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

	foreign link(path_list, libs, out)
	foreign link(path_list, libs, out, shared)
	foreign archive(path_list, out)
}

// static classes

class Deps {
	foreign static git(url)
	foreign static git(url, branch)

	static git_inherit(url, branch) {
		var path = git(url, branch)

		if (path == null) {
			return null
		}

		return File.bob(path, [Meta.instruction()])
	}

	static git_inherit(url) {
		var path = git(url)

		if (path == null) {
			return null
		}

		return File.bob(path, [Meta.instruction()])
	}
}

class File {
	foreign static list(path, depth)
	static list(path) { list(path, 0) }

	foreign static exec(path)
	foreign static exec(path, args)

	foreign static bob(path, args)
}

class Meta {
	foreign static getenv(env)
	foreign static instruction()
	foreign static os()
	foreign static prefix()
}

class Resources {
	foreign static install(path)
}
