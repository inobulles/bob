# Bob Rust skeleton for AQUA

Skeleton for a Rust project for AQUA with Bob.

This is a simple graphical program which opens a window title "Bob AQUA Skeleton" and closes it after 3 seconds.

It makes use of the [`aqua-rs`](https://github.com/inobulles/aqua-rs) crate to provide wrappers around raw VDEV commands.

You don't need to [create an AQUA environment](https://github.com/inobulles/aqua-unix), as one will automatically be created for you.

## Running

To build and run the project:

```console
bob run
```

## Packaging

To build and package the project without running it:

```console
bob package zpk
```

This will create a `default.zpk` ZPK package in the current directory.

## Building

Building without packaging in AQUA is like a joke without a punchline (ChatGPT generated metaphor don't judge).
You necessarily need to package it to be able to run it.
So while you can `bob build`, it's a bit useless.

Requiring a package be created before running can be achieved with the `Runner.pre_package` method.
