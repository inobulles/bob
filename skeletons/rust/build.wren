// compilation

var rustc = RustC.new()

var src = File.list("src")
	.where { |path| path.endsWith(".rs") }

src
	.each { |path| rustc.compile(path) }

// link program

var linker = Linker.new()
linker.link(src.toList, ["std-7c7f3bd22bdaa9dd"], "cmd")

// running

class Runner {
	static run(args) { File.exec("cmd", args) }
}

// installation map

var entry = "bin/bob-skeleton-rust"

var install = {
	"cmd": entry,
}

// packaging

var pkg = Package.new(entry)

pkg.unique = "bob.skeleton.rust"
pkg.name = "Bob Rust Skeleton"
pkg.description = "Skeleton for a Rust project with Bob."
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
