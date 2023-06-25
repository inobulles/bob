# Bob Rust skeleton

Skeleton for a Rust project with Bob.

This is a simple CLI program which prints out "Hello world!" when ran.

## Note on Bob and Rust

It isn't really recommend to use `build.wren` for building CLI programs in Rust.
For this, it's better to use [Cargo](https://doc.rust-lang.org/cargo/).

![Cargo space? No, car go road.](cargo.webp)

Bob statically links a bunch of stuff in with your actual Rust code when using `RustC`.
This is necessary for ZPK files and has numerous advantages, but results in bloated executables, which are undesirable for simple CLI tools.

Soon, you'll be able to use Bob with Cargo projects without needing `build.wren`, though ;)

## Building

To build the project:

```console
bob build
```

## Running

To run the project:

```console
bob run
```

## Installing

To install the `bob-skeleton-rust` command:

```console
bob install
```
