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
// TODO ALL THE TESTS
// XXX this tests the build being used to run the build configuration, not the one currently being built
//     it's a little bizarre, but since bootstrapping anyway requires building twice, it's fine imo

var Strcmp = Fn.new { |a, b| // XXX because 'String' does not implement '<(_)' - must be capitalized, likely a bug, cf https://wren.io/classes.html#method-scope
	// don't do 'a.count', because that looks at codepoints, not bytes

	var a_len = a.bytes.count
	var b_len = b.bytes.count

	for (i in 0...a_len.min(b_len)) {
		var byte_a = a.bytes[i]
		var byte_b = b.bytes[i]

		if (byte_a != byte_b) {
			return byte_a < byte_b
		}
	}

	return a_len < b_len
}

class Tests {
	// unit tests

	static file_exec_error {
		return File.exec("this-is-not-a-script") == null
	}

	static file_exec {
		return File.exec("execute-me.sh", ["69"]) == 69
	}

	static file_list_error {
		return File.list("this-tree-does-not-exist") == null
	}

	static file_list { // check if we can fully list a directory
		var correct = ["tree", "tree/a", "tree/b", "tree/c", "tree/c/a", "tree/c/b"]
		return File.list("tree").sort(Strcmp).join(":") == correct.join(":")
	}

	static file_list_depth {
		var correct = ["tree", "tree/a", "tree/b", "tree/c"]
		return File.list("tree", 1).sort(Strcmp).join(":") == correct.join(":")
	}

	static file_read_error { // check if we error correctly if file does not exist
		return File.read("this-file-does-not-exist") == null
	}

	static file_read { // check if we read files correctly
		return File.read("lorem") == "Lorem ipsum dolor sit amet\n"
	}

	static file_write { // check if we write files correctly
		File.write("lorem", "Lorem ipsum dolor sit amet\n")
		return file_read
	}

	// e2e tests

	static umber { // check if we can correctly clone & build a dependency completely
		var path = Deps.git("https://github.com/inobulles/umber.git")
		return path == null ? false : (File.bob(path, ["test"]) == 0)
	}
}

var tests = ["file_exec_error", "file_exec", "file_list_error", "file_list", "file_list_depth", "file_read_error", "file_read", "file_write", "umber"]
