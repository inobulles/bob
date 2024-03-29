// compilation

var rustc = RustC.new()
var src = ["src/main.rs"]

src
	.each { |path| rustc.compile(path) }

// link program
// XXX linking with pthread is necessary for rust

var linker = Linker.new()
linker.link(src.toList, ["pthread"], "cmd")

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
