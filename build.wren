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

var prefix = "/usr/local" // TODO way to discriminate between OS' - on Linux distros, this would usually be simply "/usr" instead

var install = {
	"bob": "%(prefix)/bin/bob",
}

// testing
// TODO argument to 'Deps.git' to tell it what to do with the dependency (e.g. should it test or just build?)

class Tests {
	static umber {
		return Deps.git("https://github.com/inobulles/umber.git")
	}
}

var tests = ["umber"]
