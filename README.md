# bob

Bob the builder (yes I know, I currently already have a project called this).

- Written in C -> may be easily built without another build system already in place
- Works with AQUA
- Automatically installs dependencies (how the hell should this work?)
- Can build in sandboxes (local CI? integration with other CI providers such as Cirrus CI?) -> will need libaquarium for this
- Understands other popular buildsystems (simple makefile, cmake, autoconf, qmake, setup.py, &c) -> solves the frustrating issue of having to google for stuff when you just want something to compile
- Configuration files suck -> Wren as configuration instead
- Centralised system for building, testing, deploying?
- Easy method for building out a project skeleton to get things quickly up and running (serves the same purpose then as `aqua-manager` did)
- ZPK or FreeBSD PKG packaging handled
