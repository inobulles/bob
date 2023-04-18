// C compilation

var cc = CC.new()

cc.add_opt("-std=c99")
cc.add_opt("-Wall")
cc.add_opt("-Wextra")
cc.add_opt("-Werror")

var src = File.list("src")
	.where { |path| path.endsWith(".c") }

src
	.each { |path| cc.compile(path) }

// link program

var linker = Linker.new()
linker.link(src.toList, [], "cmd")

// running

class Runner {
	static run(args) { File.exec("cmd", args) }
}

// installation map

var entry = "bin/bob-skeleton-c"

var install = {
	"cmd": entry,
}

// packaging

var pkg = Package.new(entry)

pkg.unique = "bob.skeleton.c"
pkg.name = "Bob C Skeleton"
pkg.description = "Skeleton for a C project with Bob."
pkg.version = "0.1.0"
pkg.author = "Bob the Builder"
pkg.organization = "Inobulles"
pkg.www = "https://github.com/inobulles/bob"

var packages = {
	"default": pkg,
}

// testing

class Tests {
}

var tests = []
