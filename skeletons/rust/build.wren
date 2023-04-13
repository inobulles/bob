// compilation

var rustc = RustC.new()

var src = File.list("src")
	.where { |path| path.endsWith(".rs") }

src
	.each { |path| rustc.compile(path) }

// link program

var linker = Linker.new()
linker.link(src.toList, ["std-66b716baf5a60e28"], "cmd")

// running

class Runner {
	static run(args) { File.exec("cmd", args) }
}

// testing

class Tests {
}

var tests = []
