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
	foreign add_lib(lib)
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

	static git_inherit_internal(path) {
		if (path == null) {
			return null
		}

		var instruction = Meta.instruction()

		if (instruction == "run") { // we obviously don't want to pass run instructions down to our dependencies
			instruction = "build"
		}

		return File.bob(path, [instruction])
	}

	static git_inherit(url, branch) { git_inherit_internal(git(url, branch)) }
	static git_inherit(url)         { git_inherit_internal(git(url        )) }
}

class File {
	static EXTRA { 07000 }
	static OWNER { 00700 }
	static GROUP { 00070 }
	static OTHER { 00007 }

	static READ   { 01 }
	static WRITE  { 02 }
	static EXEC   { 04 }
	static RWX    { File.READ | File.WRITE | File.EXEC }

	static STICKY { File.READ  }
	static SETGID { File.WRITE }
	static SETUID { File.EXEC  }

	foreign static bob(path, args)

	foreign static chmod(path, mask, bit)
	foreign static chown(path, user)
	foreign static chown(path, user, group)

	foreign static exec(path)
	foreign static exec(path, args)

	foreign static list(path, depth)
	static list(path) { list(path, 0) }

	foreign static read(path)
	foreign static write(path, str)
}

class Meta {
	foreign static cwd()
	foreign static getenv(env)
	foreign static instruction()
	foreign static os()
	foreign static prefix()
	foreign static setenv(env)
	foreign static setenv(env, val)
}

class Resources {
	foreign static install(path)
}
