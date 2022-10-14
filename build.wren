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
	foreign static list(path, depth) // TODO does this recursively, should there be an easy way to filter out entries we don't like through Wren rather than through options? e.g. by having a special 'Path' object which contains extra information about the path such as its depth and whatnot? Or maybe letting the user split the path string how they want through Wren is better?
	// foreign static list(path, depth) // TODO perhaps in the case of depth specifically we'd like to be able to specify it straight away, since otherwise we may be wasting a lot of resources for nothing?

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

class Package {
	construct new(name) {
		name = name
	}

	foreign package()
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

// packaging setup
// TODO how do I know where artifacts end up?

var package = Package.new("bob")

package.descr = "Bob the builder"
package.vers = "1.0"
package.www = "https://github.com/inobulles/bob"

package.package()
