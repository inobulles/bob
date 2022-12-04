// C compilation

var cc = CC.new()

cc.add_opt("-std=c99")
cc.add_opt("-I/usr/local/include")
cc.add_opt("-Isrc/wren/include")
cc.add_opt("-DWREN_OPT_META=0")
cc.add_opt("-DWREN_OPT_RANDOM=0")

var src = File.list("src")
	.where { |path| path.endsWith(".c") }

src
	.each { |path| cc.compile(path) }

// linking

var linker = Linker.new(cc)
linker.link(src.toList, ["m"], "bob")

// running

class Runner {
	static run(args) {
		return File.exec("bob", args)
	}
}

// installation map

var install = {
	"bob": "%(Meta.prefix())/bin/bob",
}

// testing

class Tests {
	// unit tests

	static file_read_error { // check if we error correctly if file does not exist
		return File.read("this_file_does_not_exist") == null ? 0 : -1
	}

	static file_read { // check if we read files correctly
		return File.read("lorem") == "Lorem ipsum dolor sit amet\n" ? 0 : -1
	}

	static file_write { // check if we write files correctly
		File.write("lorem", "Lorem ipsum dolor sit amet\n")
		return file_read
	}

	// e2e tests

	static umber { // check if we can correctly clone & build a dependency completely
		var path = Deps.git("https://github.com/inobulles/umber.git")
		return path == null ? 1 : File.bob(path, ["test"])
	}
}

var tests = ["file_read_error", "file_read", "file_write", "umber"]
