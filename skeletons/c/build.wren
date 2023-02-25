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

var linker = Linker.new(cc)
linker.link(src.toList, [], "cmd")

// running

class Runner {
	static run(args) { File.exec("cmd", args) }
}

// testing

class Tests {
}

var tests = []
