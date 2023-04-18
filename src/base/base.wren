// this file is stuck at the top of every Wren build configuration file
// object classes

foreign class CC {
	construct new() {}

	// getters

	foreign path()

	// setters

	foreign path=(x)

	// methods

	foreign add_opt(opt)
	foreign add_lib(lib)
	foreign compile(path)
}

foreign class RustC {
	construct new() {}

	// getters

	foreign path()

	// setters

	foreign path=(x)

	// methods

	foreign compile(path)
}

foreign class Linker {
	construct new() {}

	// getters

	foreign path()
	foreign archiver_path()

	// setters

	foreign path=(path)
	foreign archiver_path=(path)

	// methods

	foreign add_opt(opt)
	foreign add_lib(lib)

	foreign link(path_list, libs, out)
	foreign link(path_list, libs, out, shared)
	foreign archive(path_list, out)
}

foreign class Package {
	construct new(entry) {}

	// getters

	foreign name        ()
	foreign description ()
	foreign version     ()
	foreign author      ()
	foreign organization()
	foreign www         ()

	// setters

	foreign name=        (name)
	foreign description= (description)
	foreign version=     (version)
	foreign author=      (author)
	foreign organization=(organization)
	foreign www=         (www)
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
	// TODO binary/octal literals in Wren should really be a thing

	static EXTRA { 0xE00 }
	static OWNER { 0x1C0 }
	static GROUP { 0x038 }
	static OTHER { 0x007 }

	static READ   { 0x1 }
	static WRITE  { 0x2 }
	static EXEC   { 0x4 }
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
