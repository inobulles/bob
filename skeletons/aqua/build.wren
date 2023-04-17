// dependencies

Deps.git_inherit("https://github.com/inobulles/aqua-unix")
Deps.git_inherit("https://github.com/inobulles/iar")

// compilation

var rustc = RustC.new()

var src = File.list("src")
	.where { |path| path.endsWith(".rs") }

src
	.each { |path| rustc.compile(path) }

// link program

var linker = Linker.new()
linker.link(src.toList, ["std-7c7f3bd22bdaa9dd"], "main", true)

// running

class Runner {
	static run(args) { File.exec("aqua", ["--boot", "default.zpk"]) }
}

// installation map
// TODO make the KOS support stuff like bin/bob-skeleton-aqua, because it's annoying to have everything here flatly

var entry = "bob-skeleton-aqua"

var install = {
	"main": entry,
}

// packaging

var pkg = Package.new(entry)

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
