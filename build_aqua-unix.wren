// example build configuration to replace aqua-unix
// install dependencies, living on separate git repositories
// TODO how am I supposed to know where these were installed, if they were even installed anywhere at all?

dependency("git@github.com:inobulles/umber.git")
dependency("git@github.com:inobulles/iar.git")

// components are kinda like git submodules, except they actually work more than half the time
// components may be automatically updated by passing the 'update' instruction to bob

component("git@github.com:inobulles/aqua-kos.git", "src/kos")
component("git@github.com:inobulles/aqua-zvm.git", "src/zvm")
component("git@github.com:inobulles/aqua-dev.git", "src/dev")

// C compilation setup

var ccflags = CCFlags.new()

ccflags.debugging = true
ccflags.optimization = "2"
ccflags.standard = "c99"

if (OS.wsl) {
	ccflags.define("__WSL__")
}

// compile devices

exec("ls src/devices").split().each { |path|
	build("src/devices/%(path)")
}

// compile KOS
