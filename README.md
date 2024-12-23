# bob

Say hello to Bob the builder! 👷

Bob will help you build all sorts of different projects, as he's the buildsystem for the AQUA ecosystem!

[https://www.youtube.com/watch?v=0ldh_Cw6W0c](https://www.youtube.com/watch?v=0ldh_Cw6W0c)

## Installation

If you don't already have Bob on your system, you can bootstrap him as such:

```console
sh bootstrap.sh
```

Then, you can install him to your system with:

```console
.bootstrap/bob install
```

## Usage

### Building

Bob always loves to give a helping hand! 🤝

To build a project with Bob, navigate to its root directory and run:

```console
bob build
```

### Running

Bob's favourite pastime is running! 🏃

To run a project with Bob, run:

```console
bob run
```

Alternatively, you can enter the temporary installation prefix environment with:

```console
bob sh
```

This will use the value stored in the `$SHELL` environment variable to launch a shell inside of that prefix, similar to a Python virtual environment.
You can also just run any command you want by passing it to `bob sh`, like so:

```console
bob sh echo test
bob sh -- sh -c "echo \$PATH"
```

The second command will print out the value of the `$PATH` environment variable, and it should be prepended by the `bin` directory of the temporary installation prefix.

### Installing

Bob always helps his grandma install [Linux Mint](https://linuxmint.com/) on her computer! 👵

To install a project with Bob, run:

```console
bob install
```

### Cleaning

If for whatever reason you need to clean up Bob's mess, you can run:

```console
bob clean
```

In practice, this is just a safer way to run `rm -rf .bob`.

## Testing

To run the Bob tests, simply run:

```console
sh tests.sh
```
