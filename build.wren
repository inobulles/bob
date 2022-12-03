// dependencies
// TODO see issue #1

Deps.git("https://github.com/inobulles/umber")

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
linker.link(src.toList, ["m", "umber"], "bob")

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

// TODO testing

class Tests {
}

var tests = []
