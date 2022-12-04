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
	"bob": "%(OS.prefix())/bin/bob",
}

// testing

class Tests {
	static umber {
		var path = Deps.git("https://github.com/inobulles/umber.git")
		return path == null ? 1 : File.bob(path, ["test"])
	}
}

var tests = ["umber"]
