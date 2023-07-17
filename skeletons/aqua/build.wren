// dependencies

Meta.setenv("DEVSET", "aquabsd.alps")
Deps.git_inherit("https://github.com/inobulles/aqua-unix")

// compilation

var rustc = RustC.new()

rustc.add_dep("aqua", "https://github.com/inobulles/aqua-rs")

var src = ["src/main.rs"]

src
	.each { |path| rustc.compile(path) }

// link program

var linker = Linker.new()
linker.link(src.toList, [], "main", true)

// running

class Runner {
	static pre_package { "zpk" }
	static run(args) { File.exec("kos", ["--boot", "default.zpk"]) }
}

// installation map
// TODO make the KOS support stuff like bin/bob-skeleton-aqua, because it's annoying to have everything here flatly

var entry = "bob-skeleton-aqua"

var install = {
	"main": entry,
}

// packaging

var pkg = Package.new(entry)

pkg.unique = "bob.skeleton.aqua"
pkg.name = "Bob AQUA Skeleton"
pkg.description = "Skeleton for an AQUA project with Bob (written in Rust)."
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
