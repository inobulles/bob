// XXX all the stuff here should be included automatically by bob, not in the configuration file

foreign class CC {
	construct new() {}

	// getters

	foreign debug()
	foreign path()

	// setters

	foreign debug=(x)
	foreign path=(x)

	// methods

	foreign add_opt(opt)
	foreign compile(path)
}

class File {
	foreign static list(path, depth)
	static list(path) { list(path, 0) }
}

foreign class Linker {
	construct new() {}
	construct new(cc) {}

	// getters

	foreign path()

	// setters

	foreign path=(x)

	// methods

	foreign link(path_list, out)
}

// C compilation setup

var cc = CC.new()

cc.add_opt("-Isrc/wren/include")
cc.add_opt("-DWREN_OPT_META=0")
cc.add_opt("-DWREN_OPT_RANDOM=0")

var src = File.list("src").where { |path| path.endsWith(".c") }
src.each { |path| cc.compile(path) }

// linking

var linker = Linker.new(cc)
linker.link(src.toList, "bin/bobby")
