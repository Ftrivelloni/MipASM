[![✗](https://github.com/Ftrivelloni/MipASM/actions/workflows/pipeline.yaml/badge.svg?branch=production)](https://github.com/Ftrivelloni/MipASM/actions/workflows/pipeline.yaml)

<p align="center">
	<img src="MipASM.png" alt="MipASM logo" />
</p>

# MipASM

A C-style language whose programs compile into Standard MIDI Files, developed with Flex and Bison. The compiler builds as the `mipasm` command. The language is documented in [`doc`](doc).

* [Requirements](#requirements)
* [Configuration](#configuration)
* [Commands](#commands)
* [CI/CD](#cicd)
* [Recommended Extensions](#recommended-extensions)

## Requirements

* [Docker v28.3.2](https://www.docker.com/)

## Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

## Commands

### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

### Run

Compiles a program with the development build (no installation needed):

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to a `.mip` source file. The MIDI output lands
next to the input (`song.mip` → `song.mid`); pass `-o <file>` to choose
another path, and see `run.sh --help` for the full option list. For example,
compile one of the bundled programs:

```bash
src/main/bash/run.sh examples/ode-to-joy.mip
```

### Install

Builds a release binary (without AddressSanitizer) and installs the `mipasm`
command into the system binary directory, so it can be invoked from anywhere:

```bash
cmake -S . -B .build-release -DMIPASM_SANITIZE=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build .build-release
sudo cmake --install .build-release
mipasm <program>
```

### Test

Executes every available unit-test under `test/c` folder:

```bash
src/main/bash/test.sh
```

### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

## Editor support

A VS Code extension for the language lives in [`editors/vscode`](editors/vscode).
It provides syntax highlighting, snippets, autocompletion (keywords, built-ins
and the `stdlib` constants such as `VIOLIN`, `QUARTER`, `C4`) and on-save
diagnostics: it runs the `mipasm` compiler and turns its `line:column: error:`
output (see [`doc/LANGUAGE_FEATURES.md`](doc/LANGUAGE_FEATURES.md#diagnostics-error-format))
into red squiggles on the offending line. To try it:

```bash
cd editors/vscode
npm install
npm run compile
```

Then open the `editors/vscode` folder in VS Code and press <kbd>F5</kbd> to launch
an Extension Development Host, or package it with `npx @vscode/vsce package`.
Point the extension at a compiler with the `mipasm.compilerPath` setting (it
falls back to `.build/mipasm` in the workspace when `mipasm` is not on `PATH`).

## CI/CD

To trigger an automatic integration on every push or PR (_Pull Request_), you must activate _GitHub Actions_ in the _Settings_ tab. Use the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

## Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
