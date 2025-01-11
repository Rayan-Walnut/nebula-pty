# Nebula PTY

Nebula PTY is a modern and efficient pseudo-terminal library for Windows. This project requires `mingw` and `cmake` to build and run. Additionally, you need to clone the `winpty` dependency into the `deps` directory.

## Requirements

- `mingw`
- `cmake`
- `nodejs`
- `python`

## Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/nebula-pty.git
    cd nebula-pty
    ```

2. Clone the `winpty` dependency:
    ```sh
    git clone https://github.com/rprichard/winpty.git deps/winpty
    ```

3. Install Node.js dependencies:
    ```sh
    npm install
    ```

4. Build the project:
    ```sh
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

## Usage

Once the library is built, you can use it to create and manage pseudo-terminals in your applications. Here is a basic example of how to use Nebula PTY with Node.js:

```javascript
const { spawn } = require('child_process');
const pty = require('nebula-pty');

const shell = process.env.ComSpec || 'cmd.exe';
const ptyProcess = pty.spawn(shell, [], {
    name: 'xterm-color',
    cols: 80,
    rows: 30,
    cwd: process.env.HOME,
    env: process.env
});

ptyProcess.on('data', function(data) {
    console.log(data);
});
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgements

Special thanks to the contributors of the `winpty` project for their hard work and dedication.