// XXX all the stuff here should be included automatically by bob, not in the configuration file

class CC {
	construct new() {}

	debug { true } // TODO be able to choose between various build types when running the command, and CC.debug should default to that obviously
	std { "c99" }

	foreign compile(path)
}

class File {
	foreign static list(path) // TODO does this recursively, should there be an easy way to filter out entries we don't like through Wren rather than through options? e.g. by having a special 'Path' object which contains extra information about the path such as its depth and whatnot? Or maybe letting the user split the path string how they want through Wren is better?
	// foreign static list(path, depth) // TODO perhaps in the case of depth specifically we'd like to be able to specify it straight away, since otherwise we may be wasting a lot of resources for nothing?
}

class Linker {
	construct new() {
		_cc = CC.new()
	}

	construct new(cc) {
		_cc = cc
	}

	foreign link(pathList)
}

class Package {
	construct new(name) {
		name = name
	}

	foreign package()
}

// C compilation setup

var cc = CC.new()
var src = File.list("src")

cc.compile("src/main.c")

/* src.each { |path|
	cc.compile(path)
} */

// linking

var linker = Linker.new(cc)
linker.link(src)

// packaging setup
// TODO how do I know where artifacts end up?

var package = Package.new("bob")

package.descr = "Bob the builder"
package.vers = "1.0"
package.www = "https://github.com/inobulles/bob"

package.package()
