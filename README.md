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

TODO

- wren is okay with `wrenGetListCount` on a `*Sequence`, which happens because `IS_LIST` on a sequence value returns true (it shouldn't right? because that breaks things when doing `AS_LIST` on it to get a `ValueBuffer` afterwards...)
- the same problem as above seems to happen with foreign type slots
- speaking of which, it'd be nice to have a little documentation on *getting* element counts and elements from lists in C, rather than just setting
